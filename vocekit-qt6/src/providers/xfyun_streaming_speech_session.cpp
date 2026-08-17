#include "xfyun_streaming_speech_session.h"

#include "xfyun_speech_protocol.h"

#include <QMutexLocker>

namespace {

QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

NetworkRequestOptions streamingOptions(
    const StreamingSpeechSessionRequest &request
)
{
    NetworkRequestOptions options;
    options.timeoutMs = 15000;
    options.globalUseSystemProxy = request.useSystemProxy;
    const QString policy = request.networkPolicy.trimmed().toLower();
    if (policy == QStringLiteral("direct")) {
        options.networkPolicy = QStringLiteral("direct");
    } else if (policy == QStringLiteral("system")) {
        options.networkPolicy = QStringLiteral("system");
    }
    return options;
}

} // namespace

XfyunStreamingSpeechSession::XfyunStreamingSpeechSession(
    const TransportFactory &transportFactory,
    const SecretLoader &secretLoader,
    const UtcNow &utcNow,
    const StreamingSpeechSessionRequest &request,
    const StreamingSpeechCallbacks &callbacks,
    const Timing &timing,
    QObject *parent
)
    : QObject(parent),
      m_transportFactory(transportFactory),
      m_secretLoader(secretLoader),
      m_utcNow(utcNow),
      m_request(request),
      m_callbacks(callbacks),
      m_timing(timing)
{
    m_frameTimer.setSingleShot(false);
    m_rotationTimer.setSingleShot(true);
    connect(&m_frameTimer, &QTimer::timeout, this, [this]() {
        pumpAudioFrame();
    });
    connect(&m_rotationTimer, &QTimer::timeout, this, [this]() {
        beginRotation();
    });
}

bool XfyunStreamingSpeechSession::start(QString *error)
{
    if (m_state != StreamingSpeechState::Idle) {
        if (error) {
            *error = tr8("实时语音会话已经启动。");
        }
        return false;
    }
    if (m_request.sampleRate != 16000
        || m_request.channelCount != 1
        || m_request.sampleSizeBits != 16) {
        if (error) {
            *error = tr8("讯飞实时识别需要 16kHz、单声道、16 位 PCM 音频。");
        }
        return false;
    }

    m_secrets = m_secretLoader ? m_secretLoader() : SecretConfig();
    if (m_secrets.xfyunAppId.trimmed().isEmpty()
        || m_secrets.xfyunApiKey.trimmed().isEmpty()
        || m_secrets.xfyunApiSecret.trimmed().isEmpty()) {
        if (error) {
            *error = tr8("缺少讯飞 AppID、API Key 或 API Secret。");
        }
        return false;
    }
    return openConnection(error);
}

bool XfyunStreamingSpeechSession::pushAudio(const QByteArray &pcm)
{
    if (pcm.isEmpty()) {
        return true;
    }
    if (isTerminal() || m_state == StreamingSpeechState::Finalizing) {
        return false;
    }

    bool overflow = false;
    {
        QMutexLocker locker(&m_audioMutex);
        if (m_audioQueue.size() + pcm.size() > m_timing.queueLimitBytes) {
            overflow = true;
        } else {
            m_audioQueue.append(pcm);
        }
    }
    if (overflow) {
        degradeOnce(tr8("实时语音缓冲区已满，已切换为录音结束后识别。"));
        return false;
    }
    return true;
}

void XfyunStreamingSpeechSession::finish()
{
    if (isTerminal() || m_state == StreamingSpeechState::Idle) {
        return;
    }
    m_state = StreamingSpeechState::Finalizing;
    m_rotating = false;
    m_rotationTimer.stop();
    pumpAudioFrame();
}

void XfyunStreamingSpeechSession::cancel()
{
    if (isTerminal()) {
        return;
    }
    m_frameTimer.stop();
    m_rotationTimer.stop();
    {
        QMutexLocker locker(&m_audioMutex);
        m_audioQueue.clear();
    }
    m_state = StreamingSpeechState::Cancelled;
    ++m_generation;
    if (!m_transport.isNull()) {
        m_transport->cancel();
    }
}

StreamingSpeechState XfyunStreamingSpeechSession::state() const
{
    return m_state;
}

bool XfyunStreamingSpeechSession::openConnection(QString *error)
{
    if (!m_transportFactory) {
        if (error) {
            *error = tr8("实时语音网络传输不可用。");
        }
        m_state = StreamingSpeechState::Degraded;
        return false;
    }

    m_transport = m_transportFactory();
    if (m_transport.isNull()) {
        if (error) {
            *error = tr8("实时语音网络传输创建失败。");
        }
        m_state = StreamingSpeechState::Degraded;
        return false;
    }

    m_state = StreamingSpeechState::Connecting;
    m_sentFirstFrame = false;
    m_sentFinalFrame = false;
    const int generation = ++m_generation;
    ProviderStreamingWebSocketCallbacks callbacks;
    callbacks.opened = [this, generation]() {
        onOpened(generation);
    };
    callbacks.textMessage = [this, generation](const QByteArray &message) {
        onTextMessage(generation, message);
    };
    callbacks.failed = [this, generation](const OperationError &transportError) {
        onTransportFailed(generation, transportError);
    };
    callbacks.closed = [this, generation](bool cancelled) {
        onTransportClosed(generation, cancelled);
    };
    m_transport->open(
        xfyunSignedIatUrl(
            m_secrets,
            m_utcNow ? m_utcNow() : QDateTime::currentDateTimeUtc()
        ),
        streamingOptions(m_request),
        callbacks
    );
    return true;
}

void XfyunStreamingSpeechSession::onOpened(int generation)
{
    if (generation != m_generation || isTerminal()) {
        return;
    }
    m_state = StreamingSpeechState::Streaming;
    m_frameTimer.start(qMax(1, m_timing.frameIntervalMs));
    m_rotationTimer.start(qMax(1, m_timing.rotationIntervalMs));
    pumpAudioFrame();
}

void XfyunStreamingSpeechSession::onTextMessage(
    int generation,
    const QByteArray &message
)
{
    if (generation != m_generation || isTerminal()) {
        return;
    }
    const XfyunRecognitionEvent event = parseXfyunRecognitionEvent(message);
    if (!event.valid) {
        degradeOnce(tr8("讯飞实时识别返回了无效数据。"));
        return;
    }
    if (event.code != 0) {
        degradeOnce(
            tr8("讯飞实时识别失败：")
                + (event.message.trimmed().isEmpty()
                    ? QString::number(event.code)
                    : event.message)
        );
        return;
    }

    bool changed = false;
    if (event.pgs == QStringLiteral("rpl")) {
        changed = m_transcript.replaceCommittedRange(
            event.rangeStart,
            event.rangeEnd,
            event.sequence,
            event.text
        );
    } else if (!event.text.isEmpty()) {
        changed = m_transcript.appendCommitted(
            event.sequence,
            event.text
        );
    }
    if (changed) {
        emitSnapshot();
    }

    if (event.dataStatus == 2) {
        m_frameTimer.stop();
        if (m_rotating) {
            completeRotation();
        } else if (m_state == StreamingSpeechState::Finalizing) {
            completeSession();
        }
    }
}

void XfyunStreamingSpeechSession::onTransportFailed(
    int generation,
    const OperationError &error
)
{
    if (generation != m_generation || isTerminal()) {
        return;
    }
    degradeOnce(
        error.message.trimmed().isEmpty()
            ? tr8("实时语音连接失败，已切换为录音结束后识别。")
            : error.message
    );
}

void XfyunStreamingSpeechSession::onTransportClosed(
    int generation,
    bool cancelled
)
{
    if (generation != m_generation || isTerminal()) {
        return;
    }
    degradeOnce(
        cancelled
            ? tr8("实时语音连接已取消。")
            : tr8("实时语音连接意外断开，已切换为录音结束后识别。")
    );
}

void XfyunStreamingSpeechSession::pumpAudioFrame()
{
    if (m_state != StreamingSpeechState::Streaming
        && m_state != StreamingSpeechState::Finalizing) {
        return;
    }
    if (m_transport.isNull() || m_sentFinalFrame) {
        return;
    }

    QByteArray chunk;
    {
        QMutexLocker locker(&m_audioMutex);
        chunk = m_audioQueue.left(1280);
        m_audioQueue.remove(0, chunk.size());
    }
    if (!chunk.isEmpty()) {
        m_transport->sendText(xfyunAudioFrame(
            m_secrets,
            chunk,
            m_sentFirstFrame ? 1 : 0,
            m_request.sampleRate,
            true
        ));
        m_sentFirstFrame = true;
    }

    bool queueEmpty = false;
    {
        QMutexLocker locker(&m_audioMutex);
        queueEmpty = m_audioQueue.isEmpty();
    }
    if (m_state == StreamingSpeechState::Finalizing && queueEmpty) {
        sendFinalFrame();
    }
}

void XfyunStreamingSpeechSession::beginRotation()
{
    if (m_state != StreamingSpeechState::Streaming || m_rotating) {
        return;
    }
    m_rotating = true;
    m_frameTimer.stop();
    sendFinalFrame();
}

void XfyunStreamingSpeechSession::sendFinalFrame()
{
    if (m_transport.isNull() || m_sentFinalFrame) {
        return;
    }
    m_sentFinalFrame = true;
    m_transport->sendText(xfyunAudioFrame(
        m_secrets,
        QByteArray(),
        2,
        m_request.sampleRate,
        true
    ));
}

void XfyunStreamingSpeechSession::completeRotation()
{
    if (!m_rotating || isTerminal()) {
        return;
    }
    m_rotating = false;
    if (m_transcript.sealCurrentSession()) {
        emitSnapshot();
    }
    QSharedPointer<IProviderStreamingWebSocketTransport> oldTransport = m_transport;
    ++m_generation;
    if (!oldTransport.isNull()) {
        oldTransport->closeNormally();
    }
    QString error;
    if (!openConnection(&error)) {
        degradeOnce(error);
    }
}

void XfyunStreamingSpeechSession::completeSession()
{
    if (isTerminal()) {
        return;
    }
    m_state = StreamingSpeechState::Completed;
    m_frameTimer.stop();
    m_rotationTimer.stop();
    ++m_generation;
    if (!m_transport.isNull()) {
        m_transport->closeNormally();
    }
    if (m_callbacks.completed) {
        m_callbacks.completed(m_transcript.snapshot().displayText().trimmed());
    }
}

void XfyunStreamingSpeechSession::emitSnapshot()
{
    if (m_callbacks.transcriptUpdated) {
        m_callbacks.transcriptUpdated(m_transcript.snapshot());
    }
}

void XfyunStreamingSpeechSession::degradeOnce(const QString &message)
{
    if (m_degradedNotified || isTerminal()) {
        return;
    }
    m_degradedNotified = true;
    m_state = StreamingSpeechState::Degraded;
    m_frameTimer.stop();
    m_rotationTimer.stop();
    ++m_generation;
    if (!m_transport.isNull()) {
        m_transport->cancel();
    }
    if (m_callbacks.degraded) {
        m_callbacks.degraded(message);
    }
}

bool XfyunStreamingSpeechSession::isTerminal() const
{
    return m_state == StreamingSpeechState::Completed
        || m_state == StreamingSpeechState::Degraded
        || m_state == StreamingSpeechState::Cancelled;
}
