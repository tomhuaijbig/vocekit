#include "xfyun_speech_provider.h"

#include "../api/api_client_utils.h"
#include "../config/app_settings_defaults.h"
#include "../runtime_log.h"
#include "network_error_messages.h"
#include "xfyun_speech_protocol.h"

#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

OperationError operationError(
    const QString &code,
    const QString &message,
    const QString &detail = QString(),
    bool retryable = false)
{
    OperationError error;
    error.code = code;
    error.message = message;
    error.detail = detail;
    error.retryable = retryable;
    return error;
}

OperationError cancelledError()
{
    return operationError(
        QStringLiteral("task.cancelled"),
        tr8("任务已取消。")
    );
}

QByteArray requestAudio(const SpeechRecognitionRequest &request)
{
    if (!request.audioData.isEmpty()) {
        return request.audioData;
    }
    if (request.audioPath.trimmed().isEmpty()) {
        return QByteArray();
    }
    QFile file(request.audioPath);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

NetworkRequestOptions recognitionOptions(
    const NetworkRequestOptions &request,
    bool useSystemProxy)
{
    NetworkRequestOptions options = request;
    options.globalUseSystemProxy =
        request.globalUseSystemProxy || useSystemProxy;
    if (options.timeoutMs <= 0
        || options.timeoutMs == NetworkRequestOptions().timeoutMs) {
        options.timeoutMs = 75000;
    }
    return options;
}

QList<QByteArray> audioFrames(
    const SecretConfig &secrets,
    const QByteArray &audio,
    int sampleRate)
{
    QList<QByteArray> frames;
    int offset = 0;
    bool first = true;
    while (offset < audio.size()) {
        const QByteArray chunk = audio.mid(offset, 1280);
        offset += chunk.size();
        frames.append(
            xfyunAudioFrame(
                secrets,
                chunk,
                first ? 0 : 1,
                sampleRate,
                false
            )
        );
        first = false;
    }
    frames.append(xfyunAudioFrame(
        secrets,
        QByteArray(),
        2,
        sampleRate,
        false
    ));
    return frames;
}

OperationError websocketError(const ProviderWebSocketResult &response)
{
    if (response.cancelled
        || response.error.code == QStringLiteral("request.cancelled")) {
        return cancelledError();
    }

    OperationError error = response.error;
    if (error.code == QStringLiteral("network.timeout")) {
        error.message = tr8("讯飞语音听写网络请求超时。");
        error.retryable = true;
        return error;
    }

    const QString rawError = !error.detail.trimmed().isEmpty()
        ? error.detail
        : error.message;
    error.code = error.code.trimmed().isEmpty()
        ? QStringLiteral("speech.network")
        : error.code;
    error.message = xfyunNetworkErrorMessage(rawError);
    error.retryable = true;
    return error;
}

} // namespace

XfyunSpeechProvider::XfyunSpeechProvider(bool useSystemProxy)
    : XfyunSpeechProvider(
          createProviderWebSocketTransport(),
          []() { return loadSecrets(); },
          []() { return QDateTime::currentDateTimeUtc(); },
          useSystemProxy
      )
{
}

XfyunSpeechProvider::XfyunSpeechProvider(
    const QSharedPointer<IProviderWebSocketTransport> &transport,
    const SecretLoader &secretLoader,
    const UtcNow &utcNow,
    bool useSystemProxy)
    : m_transport(
          transport.isNull()
              ? createProviderWebSocketTransport()
              : transport
      ),
      m_secretLoader(secretLoader),
      m_utcNow(utcNow),
      m_useSystemProxy(useSystemProxy)
{
    m_secrets = m_secretLoader
        ? m_secretLoader()
        : SecretConfig();
}

QString XfyunSpeechProvider::id() const
{
    return speechProviderXfyun();
}

ProviderCheckResult XfyunSpeechProvider::checkConfiguration(
    const CancellationToken &cancellation) const
{
    ProviderCheckResult result;
    CancellationSource ownedCancellation;
    const CancellationToken effectiveCancellation = cancellation.isValid()
        ? cancellation
        : ownedCancellation.token();
    if (effectiveCancellation.isCancellationRequested()) {
        result.error = cancelledError();
        return result;
    }
    if (!m_secrets.hasXfyun()) {
        result.error = operationError(
            QStringLiteral("provider.configuration"),
            tr8(
                "缺少讯飞语音听写密钥。请在“设置 -> 接口”中填写"
                "讯飞 AppID、API Key 和 API Secret。"
            )
        );
        return result;
    }

    SpeechRecognitionRequest request;
    request.executionId = effectiveCancellation.executionId();
    request.audioData = QByteArray(16000 * 2, char(0));
    request.audioFormat = QStringLiteral("pcm");
    request.sampleRate = 16000;
    request.network.timeoutMs = 75000;
    request.network.globalUseSystemProxy = m_useSystemProxy;
    const SpeechRecognitionResult recognition =
        recognizeRequest(request, effectiveCancellation);
    result.durationMs = recognition.durationMs;
    if (recognition.error.isEmpty()) {
        result.available = true;
        result.message = tr8("讯飞连接、鉴权和识别返回成功。");
    } else if (
        recognition.error.code == QStringLiteral("speech.empty_result")) {
        result.available = true;
        result.message =
            tr8("讯飞连接和鉴权成功，静音测试未识别到语音。");
    } else {
        result.error = recognition.error;
    }
    return result;
}

SpeechRecognitionResult XfyunSpeechProvider::recognize(
    const SpeechRecognitionRequest &request,
    const CancellationToken &cancellation)
{
    return recognizeRequest(request, cancellation);
}

void XfyunSpeechProvider::refreshConfiguration()
{
    m_secrets = m_secretLoader
        ? m_secretLoader()
        : SecretConfig();
}

SpeechRecognitionResult XfyunSpeechProvider::recognizeRequest(
    const SpeechRecognitionRequest &request,
    const CancellationToken &cancellation) const
{
    SpeechRecognitionResult result;
    result.executionId = request.executionId.isValid()
        ? request.executionId
        : cancellation.executionId();
    if (cancellation.isCancellationRequested()) {
        result.error = cancelledError();
        return result;
    }
    if (!m_secrets.hasXfyun()) {
        result.error = operationError(
            QStringLiteral("provider.configuration"),
            tr8(
                "缺少讯飞语音听写密钥。请在“设置 -> 接口”中填写"
                "讯飞 AppID、API Key 和 API Secret。"
            )
        );
        return result;
    }

    const QByteArray audio = requestAudio(request);
    if (audio.isEmpty()) {
        result.error = operationError(
            QStringLiteral("speech.empty_audio"),
            tr8("录音为空。")
        );
        return result;
    }

    ProviderWebSocketRequest websocketRequest;
    websocketRequest.url = xfyunSignedIatUrl(
        m_secrets,
        m_utcNow
            ? m_utcNow()
            : QDateTime::currentDateTimeUtc()
    );
    websocketRequest.textFrames = audioFrames(
        m_secrets,
        audio,
        request.sampleRate
    );
    websocketRequest.frameIntervalMs = 40;
    websocketRequest.network = recognitionOptions(
        request.network,
        m_useSystemProxy
    );

    logRuntimeEvent(
        tr8("讯飞语音听写"),
        tr8("开始"),
        QStringLiteral("音频字节=") + QString::number(audio.size())
    );
    QElapsedTimer timer;
    timer.start();
    const ProviderWebSocketResult response = m_transport->exchange(
        websocketRequest,
        [](const QByteArray &message) {
            return parseXfyunRecognitionEvent(message).isFinal();
        },
        cancellation
    );
    result.durationMs = response.durationMs >= 0
        ? response.durationMs
        : timer.elapsed();
    for (const QByteArray &message : response.messages) {
        if (!result.rawResponse.isEmpty()) {
            result.rawResponse.append('\n');
        }
        result.rawResponse.append(message);
    }

    if (!response.error.isEmpty() || response.cancelled) {
        result.error = websocketError(response);
    } else {
        QStringList parts;
        for (const QByteArray &message : response.messages) {
            const XfyunRecognitionEvent event =
                parseXfyunRecognitionEvent(message);
            if (!event.valid) {
                result.error = operationError(
                    QStringLiteral("speech.invalid_response"),
                    tr8("讯飞语音听写返回的不是有效 JSON。"),
                    QString::fromUtf8(message)
                );
                break;
            }
            if (event.code != 0) {
                result.error = operationError(
                    QStringLiteral("speech.api"),
                    tr8("讯飞识别失败：")
                        + (event.message.trimmed().isEmpty()
                            ? QString::number(event.code)
                            : event.message),
                    QString::fromUtf8(message)
                );
                break;
            }
            if (!event.text.isEmpty()) {
                parts.append(event.text);
            }
        }
        result.text = parts.join(QString()).trimmed();
        if (result.error.isEmpty() && result.text.isEmpty()) {
            result.error = operationError(
                QStringLiteral("speech.empty_result"),
                tr8("讯飞没有识别到语音。")
            );
        }
    }

    logRuntimeEvent(
        tr8("讯飞语音听写"),
        result.error.isEmpty() ? tr8("完成") : tr8("失败"),
        QStringLiteral("目标=iat-api.xfyun.cn/v2/iat，结果字数=")
            + QString::number(result.text.size())
            + (!result.error.isEmpty()
                ? QStringLiteral("，错误码=") + result.error.code
                : QString()),
        result.durationMs
    );
    return result;
}

QSharedPointer<ISpeechProvider> createXfyunSpeechProvider(
    bool useSystemProxy)
{
    return QSharedPointer<ISpeechProvider>(
        new XfyunSpeechProvider(useSystemProxy)
    );
}

QSharedPointer<ISpeechProvider> createXfyunSpeechProvider(
    const SecretConfig &secrets,
    bool useSystemProxy)
{
    return QSharedPointer<ISpeechProvider>(
        new XfyunSpeechProvider(
            createProviderWebSocketTransport(),
            [secrets]() { return secrets; },
            []() { return QDateTime::currentDateTimeUtc(); },
            useSystemProxy
        )
    );
}
