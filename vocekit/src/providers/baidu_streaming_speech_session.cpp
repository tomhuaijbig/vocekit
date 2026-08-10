#include "baidu_streaming_speech_session.h"

#include "baidu_realtime_speech_protocol.h"

#include <QMutexLocker>
#include <QUuid>

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

QString defaultSessionId()
{
    return QUuid::createUuid().toString().remove(QLatin1Char('{'))
        .remove(QLatin1Char('}'));
}

} // namespace

BaiduStreamingSpeechSession::BaiduStreamingSpeechSession(
    const TransportFactory &transportFactory,
    const SecretLoader &secretLoader,
    const SessionIdFactory &sessionIdFactory,
    const StreamingSpeechSessionRequest &request,
    const StreamingSpeechCallbacks &callbacks,
    const Timing &timing,
    QObject *parent
)
    : QObject(parent),
      m_transportFactory(transportFactory),
      m_secretLoader(secretLoader),
      m_sessionIdFactory(sessionIdFactory),
      m_request(request),
      m_callbacks(callbacks),
      m_timing(timing)
{
    m_frameTimer.setSingleShot(false);
    connect(&m_frameTimer, &QTimer::timeout, this, [this]() {
        pumpAudioFrame();
    });
}

bool BaiduStreamingSpeechSession::start(QString *error)
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
            *error = tr8("百度实时识别需要 16kHz、单声道、16 位 PCM 音频。");
        }
        return false;
    }

    m_secrets = m_secretLoader ? m_secretLoader() : SecretConfig();
    bool appIdOk = false;
    const qlonglong appId = m_secrets.baiduAppId.trimmed().toLongLong(&appIdOk);
    if (!appIdOk || appId <= 0
        || m_secrets.baiduApiKey.trimmed().isEmpty()) {
        if (error) {
            *error = tr8("百度实时识别需要数字 AppID 和 API Key。");
        }
        return false;
    }
    if (!m_transportFactory) {
        if (error) {
            *error = tr8("实时语音网络传输不可用。");
        }
        return false;
    }

    m_transport = m_transportFactory();
    if (m_transport.isNull()) {
        if (error) {
            *error = tr8("实时语音网络传输创建失败。");
        }
        return false;
    }
    m_sessionId = m_sessionIdFactory
        ? m_sessionIdFactory().trimmed()
        : defaultSessionId();
    if (m_sessionId.isEmpty()) {
        m_sessionId = defaultSessionId();
    }

    m_state = StreamingSpeechState::Connecting;
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
        baiduRealtimeSpeechUrl(m_sessionId),
        streamingOptions(m_request),
        callbacks
    );
    return true;
}

bool BaiduStreamingSpeechSession::pushAudio(const QByteArray &pcm)
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

void BaiduStreamingSpeechSession::finish()
{
    if (isTerminal() || m_state == StreamingSpeechState::Idle) {
        return;
    }
    m_state = StreamingSpeechState::Finalizing;
    pumpAudioFrame();
}

void BaiduStreamingSpeechSession::cancel()
{
    if (isTerminal()) {
        return;
    }
    m_frameTimer.stop();
    {
        QMutexLocker locker(&m_audioMutex);
        m_audioQueue.clear();
    }
    m_state = StreamingSpeechState::Cancelled;
    ++m_generation;
    if (!m_transport.isNull()) {
        m_transport->sendText(
            baiduRealtimeControlFrame(QStringLiteral("CANCEL"))
        );
        m_transport->cancel();
    }
}

StreamingSpeechState BaiduStreamingSpeechSession::state() const
{
    return m_state;
}

void BaiduStreamingSpeechSession::onOpened(int generation)
{
    if (generation != m_generation || isTerminal()) {
        return;
    }
    m_state = StreamingSpeechState::Streaming;
    m_transport->sendText(baiduRealtimeStartFrame(
        m_secrets,
        m_request,
        QStringLiteral("vocekit-") + m_sessionId
    ));
    m_frameTimer.start(qMax(1, m_timing.frameIntervalMs));
    pumpAudioFrame();
}

void BaiduStreamingSpeechSession::onTextMessage(
    int generation,
    const QByteArray &message
)
{
    if (generation != m_generation || isTerminal()) {
        return;
    }
    const BaiduRealtimeRecognitionEvent event =
        parseBaiduRealtimeRecognitionEvent(message);
    if (!event.valid) {
        degradeOnce(tr8("百度实时识别返回了无效数据。"));
        return;
    }
    if (event.errorNumber != 0
        || event.type == QStringLiteral("ERROR")) {
        degradeOnce(
            tr8("百度实时识别失败：")
                + (event.errorMessage.trimmed().isEmpty()
                    ? QString::number(event.errorNumber)
                    : event.errorMessage)
        );
        return;
    }

    bool changed = false;
    if (event.type == QStringLiteral("MID_TEXT")) {
        changed = m_transcript.setProvisional(event.text);
    } else if (event.type == QStringLiteral("FIN_TEXT")) {
        changed = m_transcript.commitProvisional(event.text);
    }
    if (changed) {
        emitSnapshot();
    }
    if (event.type == QStringLiteral("FIN_TEXT")
        && m_state == StreamingSpeechState::Finalizing) {
        completeSession();
    }
}

void BaiduStreamingSpeechSession::onTransportFailed(
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

void BaiduStreamingSpeechSession::onTransportClosed(
    int generation,
    bool cancelled
)
{
    if (generation != m_generation || isTerminal()) {
        return;
    }
    if (!cancelled && m_state == StreamingSpeechState::Finalizing) {
        completeSession();
        return;
    }
    degradeOnce(
        cancelled
            ? tr8("实时语音连接已取消。")
            : tr8("实时语音连接意外断开，已切换为录音结束后识别。")
    );
}

void BaiduStreamingSpeechSession::pumpAudioFrame()
{
    if (m_state != StreamingSpeechState::Streaming
        && m_state != StreamingSpeechState::Finalizing) {
        return;
    }
    if (m_transport.isNull() || m_sentFinish) {
        return;
    }

    QByteArray chunk;
    {
        QMutexLocker locker(&m_audioMutex);
        chunk = m_audioQueue.left(5120);
        m_audioQueue.remove(0, chunk.size());
    }
    if (!chunk.isEmpty()) {
        m_transport->sendBinary(chunk);
    }

    bool queueEmpty = false;
    {
        QMutexLocker locker(&m_audioMutex);
        queueEmpty = m_audioQueue.isEmpty();
    }
    if (m_state == StreamingSpeechState::Finalizing && queueEmpty) {
        sendFinishFrame();
    }
}

void BaiduStreamingSpeechSession::sendFinishFrame()
{
    if (m_sentFinish || m_transport.isNull()) {
        return;
    }
    m_sentFinish = true;
    m_frameTimer.stop();
    m_transport->sendText(
        baiduRealtimeControlFrame(QStringLiteral("FINISH"))
    );
}

void BaiduStreamingSpeechSession::completeSession()
{
    if (isTerminal()) {
        return;
    }
    m_state = StreamingSpeechState::Completed;
    m_frameTimer.stop();
    ++m_generation;
    if (!m_transport.isNull()) {
        m_transport->closeNormally();
    }
    if (m_callbacks.completed) {
        m_callbacks.completed(m_transcript.snapshot().displayText().trimmed());
    }
}

void BaiduStreamingSpeechSession::emitSnapshot()
{
    if (m_callbacks.transcriptUpdated) {
        m_callbacks.transcriptUpdated(m_transcript.snapshot());
    }
}

void BaiduStreamingSpeechSession::degradeOnce(const QString &message)
{
    if (m_degradedNotified || isTerminal()) {
        return;
    }
    m_degradedNotified = true;
    m_state = StreamingSpeechState::Degraded;
    m_frameTimer.stop();
    ++m_generation;
    if (!m_transport.isNull()) {
        m_transport->cancel();
    }
    if (m_callbacks.degraded) {
        m_callbacks.degraded(message);
    }
}

bool BaiduStreamingSpeechSession::isTerminal() const
{
    return m_state == StreamingSpeechState::Completed
        || m_state == StreamingSpeechState::Degraded
        || m_state == StreamingSpeechState::Cancelled;
}
