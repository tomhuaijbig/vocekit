#include "custom_speech_provider.h"

#include "../config/app_settings_defaults.h"
#include "../runtime_log.h"

#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QUrl>

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

QUrl normalizedEndpoint(QString endpoint)
{
    endpoint = endpoint.trimmed();
    if (!endpoint.contains(QStringLiteral("://"))) {
        endpoint.prepend(QStringLiteral("https://"));
    }
    return QUrl(endpoint);
}

QJsonValue jsonValueAtPath(
    const QJsonValue &root,
    const QStringList &path)
{
    QJsonValue current = root;
    for (const QString &part : path) {
        if (current.isObject()) {
            current = current.toObject().value(part);
            continue;
        }
        if (current.isArray()) {
            bool ok = false;
            const int index = part.toInt(&ok);
            const QJsonArray values = current.toArray();
            current = ok && index >= 0 && index < values.size()
                ? values.at(index)
                : QJsonValue();
            continue;
        }
        return QJsonValue();
    }
    return current;
}

QString recognitionText(const QJsonObject &root)
{
    const QVector<QStringList> paths = QVector<QStringList>()
        << (QStringList() << QStringLiteral("text"))
        << (QStringList() << QStringLiteral("result"))
        << (QStringList() << QStringLiteral("transcript"))
        << (QStringList()
            << QStringLiteral("data")
            << QStringLiteral("text"))
        << (QStringList()
            << QStringLiteral("data")
            << QStringLiteral("result"))
        << (QStringList()
            << QStringLiteral("result")
            << QStringLiteral("0"))
        << (QStringList()
            << QStringLiteral("results")
            << QStringLiteral("0")
            << QStringLiteral("text"));
    for (const QStringList &path : paths) {
        const QJsonValue value = jsonValueAtPath(root, path);
        if (value.isString() && !value.toString().trimmed().isEmpty()) {
            return value.toString().trimmed();
        }
        if (value.isDouble()) {
            return QString::number(value.toDouble());
        }
    }
    return QString();
}

QString apiErrorMessage(const QJsonObject &root)
{
    const QJsonValue value = root.value(QStringLiteral("error"));
    if (value.isObject()) {
        return value.toObject().value(QStringLiteral("message")).toString();
    }
    return value.toString();
}

NetworkRequestOptions requestOptions(
    const SpeechRecognitionRequest &request,
    bool useSystemProxy)
{
    NetworkRequestOptions options = request.network;
    options.globalUseSystemProxy =
        request.network.globalUseSystemProxy || useSystemProxy;
    if (options.timeoutMs <= 0
        || options.timeoutMs == NetworkRequestOptions().timeoutMs) {
        options.timeoutMs = 70000;
    }
    return options;
}

OperationError networkOperationError(const NetworkResponse &response)
{
    if (response.cancelled
        || response.error.code == QStringLiteral("request.cancelled")) {
        return cancelledError();
    }

    QString message = response.error.message.trimmed();
    QString detail = response.error.detail.trimmed();
    if (detail.isEmpty() && !response.body.isEmpty()) {
        detail = QString::fromUtf8(response.body);
    }
    if (response.error.code == QStringLiteral("network.timeout")) {
        message = tr8("自定义语音接口调用失败：网络请求超时。");
    } else if (detail.contains(QStringLiteral("SSL"), Qt::CaseInsensitive)) {
        message = tr8(
            "自定义语音接口调用失败：SSL 运行库缺失或版本不匹配。"
            "请确认程序目录中存在 libeay32.dll 和 ssleay32.dll。"
        );
    } else if (response.statusCode == 401
               || detail.contains(
                   QStringLiteral("authentication"),
                   Qt::CaseInsensitive
               )) {
        message = tr8(
            "自定义语音接口调用失败：接口认证失败。"
            "请检查自定义语音接口密钥。"
        );
    } else if (message.isEmpty()) {
        message = tr8("自定义语音接口调用失败：网络请求失败。");
    } else {
        message = tr8("自定义语音接口调用失败：") + message;
    }

    OperationError error = response.error;
    error.code = error.code.trimmed().isEmpty()
        ? QStringLiteral("speech.network")
        : error.code;
    error.message = message;
    error.detail = detail;
    if (response.error.code == QStringLiteral("network.timeout")) {
        error.retryable = true;
    }
    return error;
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

} // namespace

CustomSpeechProvider::CustomSpeechProvider(bool useSystemProxy)
    : CustomSpeechProvider(
          createProviderNetworkTransport(),
          []() { return loadSecrets(); },
          useSystemProxy
      )
{
}

CustomSpeechProvider::CustomSpeechProvider(
    const QSharedPointer<IProviderNetworkTransport> &transport,
    const SecretLoader &secretLoader,
    bool useSystemProxy)
    : m_transport(
          transport.isNull() ? createProviderNetworkTransport() : transport
      ),
      m_secretLoader(secretLoader),
      m_useSystemProxy(useSystemProxy)
{
    m_secrets = m_secretLoader
        ? m_secretLoader()
        : SecretConfig();
}

QString CustomSpeechProvider::id() const
{
    return speechProviderCustom();
}

ProviderCheckResult CustomSpeechProvider::checkConfiguration(
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
    if (!m_secrets.hasCustomSpeech()) {
        result.error = operationError(
            QStringLiteral("provider.configuration"),
            tr8("未填写自定义语音接口地址。")
        );
        return result;
    }

    SpeechRecognitionRequest request;
    request.executionId = effectiveCancellation.executionId();
    request.audioData = QByteArray(16000 * 2, char(0));
    request.audioFormat = QStringLiteral("pcm");
    request.sampleRate = 16000;
    request.network.timeoutMs = 15000;
    request.network.globalUseSystemProxy = m_useSystemProxy;
    const SpeechRecognitionResult recognition =
        execute(request, effectiveCancellation);
    result.durationMs = recognition.durationMs;
    if (recognition.error.isEmpty()) {
        result.available = true;
        result.message = tr8("自定义语音接口返回成功。");
        return result;
    }
    if (recognition.error.code == QStringLiteral("speech.empty_result")) {
        result.available = true;
        result.message =
            tr8("自定义语音接口已连通，静音测试没有返回识别文字。");
        return result;
    }
    result.error = recognition.error;
    return result;
}

SpeechRecognitionResult CustomSpeechProvider::recognize(
    const SpeechRecognitionRequest &request,
    const CancellationToken &cancellation)
{
    return execute(request, cancellation);
}

void CustomSpeechProvider::refreshConfiguration()
{
    m_secrets = m_secretLoader
        ? m_secretLoader()
        : SecretConfig();
}

SpeechRecognitionResult CustomSpeechProvider::execute(
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
    if (!m_secrets.hasCustomSpeech()) {
        result.error = operationError(
            QStringLiteral("provider.configuration"),
            tr8(
                "缺少自定义语音接口地址。"
                "请在“设置 -> 接口”中填写自定义语音接口地址。"
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

    const QUrl endpoint = normalizedEndpoint(m_secrets.customSpeechUrl);
    if (!endpoint.isValid() || endpoint.host().isEmpty()) {
        result.error = operationError(
            QStringLiteral("speech.invalid_endpoint"),
            tr8(
                "自定义语音接口地址无效。"
                "请填写完整域名或 https 地址。"
            )
        );
        return result;
    }

    QJsonObject body;
    body.insert(QStringLiteral("format"), QStringLiteral("pcm"));
    body.insert(
        QStringLiteral("rate"),
        request.sampleRate > 0 ? request.sampleRate : 16000
    );
    body.insert(QStringLiteral("channel"), 1);
    body.insert(QStringLiteral("len"), audio.size());
    body.insert(
        QStringLiteral("speech"),
        QString::fromLatin1(audio.toBase64())
    );
    if (!m_secrets.customSpeechModel.trimmed().isEmpty()) {
        body.insert(
            QStringLiteral("model"),
            m_secrets.customSpeechModel.trimmed()
        );
    }

    QNetworkRequest networkRequest(endpoint);
    networkRequest.setHeader(
        QNetworkRequest::ContentTypeHeader,
        QStringLiteral("application/json")
    );
    if (!m_secrets.customSpeechApiKey.trimmed().isEmpty()) {
        networkRequest.setRawHeader(
            "Authorization",
            QByteArrayLiteral("Bearer ")
                + m_secrets.customSpeechApiKey.trimmed().toUtf8()
        );
    }

    QElapsedTimer timer;
    timer.start();
    logRuntimeEvent(
        tr8("自定义语音接口"),
        tr8("开始"),
        QStringLiteral("音频字节=") + QString::number(audio.size())
            + QStringLiteral("，目标=") + endpoint.host()
    );
    const NetworkResponse response = m_transport->postJson(
        networkRequest,
        QJsonDocument(body).toJson(QJsonDocument::Compact),
        requestOptions(request, m_useSystemProxy),
        cancellation
    );
    result.durationMs = response.durationMs >= 0
        ? response.durationMs
        : timer.elapsed();
    result.rawResponse = response.body;

    if (!response.error.isEmpty() || response.cancelled
        || !response.isSuccess()) {
        result.error = networkOperationError(response);
    } else {
        QJsonParseError parseError;
        const QJsonDocument document =
            QJsonDocument::fromJson(response.body, &parseError);
        if (parseError.error != QJsonParseError::NoError
            || !document.isObject()) {
            result.error = operationError(
                QStringLiteral("speech.invalid_response"),
                tr8("自定义语音接口返回的不是 JSON。"),
                QString::fromUtf8(response.body)
            );
        } else {
            const QJsonObject root = document.object();
            if (root.contains(QStringLiteral("error"))) {
                QString message = apiErrorMessage(root).trimmed();
                if (message.isEmpty()) {
                    message = QString::fromUtf8(response.body);
                }
                result.error = operationError(
                    QStringLiteral("speech.api"),
                    tr8("自定义语音接口返回错误：") + message
                );
            } else {
                result.text = recognitionText(root);
                if (result.text.trimmed().isEmpty()) {
                    result.error = operationError(
                        QStringLiteral("speech.empty_result"),
                        tr8(
                            "自定义语音接口没有返回识别文字。"
                            "请确认返回 JSON 里包含 text、result、"
                            "transcript 或 data.text 字段。"
                        )
                    );
                }
            }
        }
    }

    logRuntimeEvent(
        tr8("自定义语音接口"),
        result.error.isEmpty() ? tr8("完成") : tr8("失败"),
        QStringLiteral("结果字数=") + QString::number(result.text.size())
            + (!result.error.isEmpty()
                ? QStringLiteral("，错误=") + result.error.message
                : QString()),
        result.durationMs
    );
    return result;
}

QSharedPointer<ISpeechProvider> createCustomSpeechProvider(
    bool useSystemProxy)
{
    return QSharedPointer<ISpeechProvider>(
        new CustomSpeechProvider(useSystemProxy)
    );
}

QSharedPointer<ISpeechProvider> createCustomSpeechProvider(
    const SecretConfig &secrets,
    bool useSystemProxy)
{
    return QSharedPointer<ISpeechProvider>(
        new CustomSpeechProvider(
            createProviderNetworkTransport(),
            [secrets]() { return secrets; },
            useSystemProxy
        )
    );
}
