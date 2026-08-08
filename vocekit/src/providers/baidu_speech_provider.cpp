#include "baidu_speech_provider.h"

#include "../config/app_settings_defaults.h"
#include "../runtime_log.h"

#include <QElapsedTimer>
#include <QFile>
#include <QHostInfo>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

namespace {

struct SharedAccessToken
{
    QString token;
    QDateTime expiry;
};

QMutex &sharedAccessTokenMutex()
{
    static QMutex mutex;
    return mutex;
}

QHash<QByteArray, SharedAccessToken> &sharedAccessTokens()
{
    static QHash<QByteArray, SharedAccessToken> tokens;
    return tokens;
}

QByteArray accessTokenCacheKey(const SecretConfig &secrets)
{
    return QCryptographicHash::hash(
        secrets.baiduApiKey.trimmed().toUtf8()
            + QByteArrayLiteral("\0")
            + secrets.baiduSecretKey.trimmed().toUtf8(),
        QCryptographicHash::Sha256
    );
}

bool readSharedAccessToken(
    const SecretConfig &secrets,
    QString *token,
    QDateTime *expiry)
{
    QMutexLocker locker(&sharedAccessTokenMutex());
    const auto item = sharedAccessTokens().constFind(
        accessTokenCacheKey(secrets)
    );
    if (item == sharedAccessTokens().constEnd()
        || item->token.isEmpty()
        || item->expiry
            <= QDateTime::currentDateTime().addSecs(60)) {
        return false;
    }
    if (token) {
        *token = item->token;
    }
    if (expiry) {
        *expiry = item->expiry;
    }
    return true;
}

void writeSharedAccessToken(
    const SecretConfig &secrets,
    const QString &token,
    const QDateTime &expiry)
{
    QMutexLocker locker(&sharedAccessTokenMutex());
    SharedAccessToken item;
    item.token = token;
    item.expiry = expiry;
    sharedAccessTokens().insert(accessTokenCacheKey(secrets), item);
}

void removeSharedAccessToken(const SecretConfig &secrets)
{
    QMutexLocker locker(&sharedAccessTokenMutex());
    sharedAccessTokens().remove(accessTokenCacheKey(secrets));
}

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

NetworkRequestOptions tokenRequestOptions(
    const NetworkRequestOptions &request,
    bool useSystemProxy)
{
    NetworkRequestOptions options = request;
    options.globalUseSystemProxy =
        request.globalUseSystemProxy || useSystemProxy;
    if (options.timeoutMs <= 0) {
        options.timeoutMs = 30000;
    }
    return options;
}

NetworkRequestOptions recognitionRequestOptions(
    const NetworkRequestOptions &request,
    bool useSystemProxy)
{
    NetworkRequestOptions options = request;
    options.globalUseSystemProxy =
        request.globalUseSystemProxy || useSystemProxy;
    if (options.timeoutMs <= 0
        || options.timeoutMs == NetworkRequestOptions().timeoutMs) {
        options.timeoutMs = 70000;
    }
    return options;
}

QString responseDetail(const NetworkResponse &response)
{
    QString detail = response.error.detail.trimmed();
    if (detail.isEmpty() && !response.body.isEmpty()) {
        detail = QString::fromUtf8(response.body);
    }
    return detail;
}

OperationError networkOperationError(
    const NetworkResponse &response,
    const QString &stage)
{
    if (response.cancelled
        || response.error.code == QStringLiteral("request.cancelled")) {
        return cancelledError();
    }

    const QString detail = responseDetail(response);
    OperationError error = response.error;
    error.code = error.code.trimmed().isEmpty()
        ? QStringLiteral("speech.network")
        : error.code;
    if (error.code == QStringLiteral("network.timeout")) {
        error.message = stage + tr8("网络请求超时。");
        error.retryable = true;
    } else if (detail.contains(QStringLiteral("SSL"), Qt::CaseInsensitive)) {
        error.message = stage
            + tr8(
                "网络请求失败：SSL 运行库缺失或版本不匹配。"
                "请确认程序目录中存在 libeay32.dll 和 ssleay32.dll。"
            );
    } else if (response.statusCode == 401
               || detail.contains(
                   QStringLiteral("authentication"),
                   Qt::CaseInsensitive
               )) {
        error.message = stage
            + tr8("接口认证失败，请检查百度 API Key 和 Secret Key。");
    } else {
        const QString original = response.error.message.trimmed();
        error.message = stage
            + (original.isEmpty()
                ? tr8("网络请求失败。")
                : tr8("网络请求失败：") + original);
    }
    error.detail = detail;
    return error;
}

QString tokenApiError(const QJsonObject &root)
{
    QString message =
        root.value(QStringLiteral("error_description")).toString().trimmed();
    if (message.isEmpty()) {
        message = root.value(QStringLiteral("error")).toString().trimmed();
    }
    return message;
}

} // namespace

BaiduSpeechProvider::BaiduSpeechProvider(bool useSystemProxy)
    : BaiduSpeechProvider(
          createProviderNetworkTransport(),
          []() { return loadSecrets(); },
          useSystemProxy,
          true
      )
{
}

BaiduSpeechProvider::BaiduSpeechProvider(
    const QSharedPointer<IProviderNetworkTransport> &transport,
    const SecretLoader &secretLoader,
    bool useSystemProxy,
    bool shareAccessTokenCache)
    : m_transport(
          transport.isNull() ? createProviderNetworkTransport() : transport
      ),
      m_secretLoader(secretLoader),
      m_useSystemProxy(useSystemProxy),
      m_shareAccessTokenCache(shareAccessTokenCache)
{
    m_secrets = m_secretLoader
        ? m_secretLoader()
        : SecretConfig();
}

QString BaiduSpeechProvider::id() const
{
    return speechProviderBaidu();
}

ProviderCheckResult BaiduSpeechProvider::checkConfiguration(
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
    if (!m_secrets.hasBaidu()) {
        result.error = operationError(
            QStringLiteral("provider.configuration"),
            tr8(
                "缺少百度语音识别密钥。请在“设置 -> 接口”中填写"
                "百度 API Key 和 Secret Key。"
            )
        );
        return result;
    }

    NetworkRequestOptions options;
    options.timeoutMs = 30000;
    options.globalUseSystemProxy = m_useSystemProxy;
    const AccessTokenResult token =
        accessToken(options, effectiveCancellation);
    result.durationMs = token.durationMs;
    if (!token.error.isEmpty()) {
        result.error = token.error;
        return result;
    }
    result.available = true;
    result.message = tr8("百度令牌获取成功。");
    return result;
}

SpeechRecognitionResult BaiduSpeechProvider::recognize(
    const SpeechRecognitionRequest &request,
    const CancellationToken &cancellation)
{
    SpeechRecognitionResult result;
    result.executionId = request.executionId.isValid()
        ? request.executionId
        : cancellation.executionId();
    if (cancellation.isCancellationRequested()) {
        result.error = cancelledError();
        return result;
    }
    if (!m_secrets.hasBaidu()) {
        result.error = operationError(
            QStringLiteral("provider.configuration"),
            tr8(
                "缺少百度语音识别密钥。请在“设置 -> 接口”中填写"
                "百度 API Key 和 Secret Key。"
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

    QElapsedTimer totalTimer;
    totalTimer.start();
    const AccessTokenResult token =
        accessToken(request.network, cancellation);
    if (!token.error.isEmpty()) {
        result.error = token.error;
        result.rawResponse = token.rawResponse;
        result.durationMs = token.durationMs;
        return result;
    }
    if (cancellation.isCancellationRequested()) {
        result.error = cancelledError();
        result.durationMs = totalTimer.elapsed();
        return result;
    }

    QJsonObject body;
    body.insert(QStringLiteral("format"), QStringLiteral("pcm"));
    body.insert(
        QStringLiteral("rate"),
        request.sampleRate > 0 ? request.sampleRate : 16000
    );
    body.insert(QStringLiteral("channel"), 1);
    QString clientId = QHostInfo::localHostName().trimmed();
    if (clientId.isEmpty()) {
        clientId = QStringLiteral("vocekit");
    }
    body.insert(QStringLiteral("cuid"), clientId);
    body.insert(QStringLiteral("token"), token.token);
    body.insert(QStringLiteral("len"), audio.size());
    body.insert(
        QStringLiteral("speech"),
        QString::fromLatin1(audio.toBase64())
    );
    body.insert(QStringLiteral("dev_pid"), 1537);

    QNetworkRequest networkRequest(
        QUrl(QStringLiteral("https://vop.baidu.com/server_api"))
    );
    networkRequest.setHeader(
        QNetworkRequest::ContentTypeHeader,
        QStringLiteral("application/json")
    );

    logRuntimeEvent(
        tr8("百度语音识别"),
        tr8("开始"),
        QStringLiteral("音频字节=") + QString::number(audio.size())
    );
    const NetworkResponse response = m_transport->postJson(
        networkRequest,
        QJsonDocument(body).toJson(QJsonDocument::Compact),
        recognitionRequestOptions(request.network, m_useSystemProxy),
        cancellation
    );
    result.rawResponse = response.body;
    result.durationMs = token.durationMs >= 0 && response.durationMs >= 0
        ? token.durationMs + response.durationMs
        : totalTimer.elapsed();

    if (!response.error.isEmpty() || response.cancelled
        || !response.isSuccess()) {
        result.error = networkOperationError(
            response,
            tr8("百度语音识别")
        );
    } else {
        QJsonParseError parseError;
        const QJsonDocument document =
            QJsonDocument::fromJson(response.body, &parseError);
        if (parseError.error != QJsonParseError::NoError
            || !document.isObject()) {
            result.error = operationError(
                QStringLiteral("speech.invalid_response"),
                tr8("百度语音识别返回的不是有效 JSON。"),
                QString::fromUtf8(response.body)
            );
        } else {
            const QJsonObject root = document.object();
            const int errorNumber =
                root.value(QStringLiteral("err_no")).toInt(-1);
            if (errorNumber != 0) {
                const QString apiMessage =
                    root.value(QStringLiteral("err_msg"))
                        .toString(QString::number(errorNumber));
                result.error = operationError(
                    QStringLiteral("speech.api"),
                    tr8("百度识别失败：") + apiMessage,
                    QString::fromUtf8(response.body)
                );
            } else {
                const QJsonArray values =
                    root.value(QStringLiteral("result")).toArray();
                result.text = values.isEmpty()
                    ? QString()
                    : values.first().toString().trimmed();
                if (result.text.isEmpty()) {
                    result.error = operationError(
                        QStringLiteral("speech.empty_result"),
                        tr8("百度语音识别没有返回文字。")
                    );
                }
            }
        }
    }

    logRuntimeEvent(
        tr8("百度语音识别"),
        result.error.isEmpty() ? tr8("完成") : tr8("失败"),
        QStringLiteral("结果字数=") + QString::number(result.text.size())
            + (!result.error.isEmpty()
                ? QStringLiteral("，错误码=") + result.error.code
                : QString()),
        result.durationMs
    );
    return result;
}

void BaiduSpeechProvider::refreshConfiguration()
{
    if (m_shareAccessTokenCache && m_secrets.hasBaidu()) {
        removeSharedAccessToken(m_secrets);
    }
    m_secrets = m_secretLoader
        ? m_secretLoader()
        : SecretConfig();
    m_accessToken.clear();
    m_tokenExpiry = QDateTime();
}

BaiduSpeechProvider::AccessTokenResult BaiduSpeechProvider::accessToken(
    const NetworkRequestOptions &options,
    const CancellationToken &cancellation) const
{
    AccessTokenResult result;
    if (cancellation.isCancellationRequested()) {
        result.error = cancelledError();
        return result;
    }
    if (!m_secrets.hasBaidu()) {
        result.error = operationError(
            QStringLiteral("provider.configuration"),
            tr8("未填写百度 API Key 或 Secret Key。")
        );
        return result;
    }
    if (!m_accessToken.isEmpty()
        && m_tokenExpiry
            > QDateTime::currentDateTime().addSecs(60)) {
        result.token = m_accessToken;
        result.durationMs = 0;
        return result;
    }
    if (m_shareAccessTokenCache
        && readSharedAccessToken(
            m_secrets,
            &m_accessToken,
            &m_tokenExpiry)) {
        result.token = m_accessToken;
        result.durationMs = 0;
        return result;
    }

    QUrl url(QStringLiteral(
        "https://aip.baidubce.com/oauth/2.0/token"
    ));
    QUrlQuery query;
    query.addQueryItem(
        QStringLiteral("grant_type"),
        QStringLiteral("client_credentials")
    );
    query.addQueryItem(
        QStringLiteral("client_id"),
        m_secrets.baiduApiKey
    );
    query.addQueryItem(
        QStringLiteral("client_secret"),
        m_secrets.baiduSecretKey
    );
    url.setQuery(query);

    QElapsedTimer timer;
    timer.start();
    const NetworkResponse response = m_transport->get(
        QNetworkRequest(url),
        tokenRequestOptions(options, m_useSystemProxy),
        cancellation
    );
    result.durationMs = response.durationMs >= 0
        ? response.durationMs
        : timer.elapsed();
    result.rawResponse = response.body;
    if (!response.error.isEmpty() || response.cancelled
        || !response.isSuccess()) {
        result.error = networkOperationError(
            response,
            tr8("百度令牌")
        );
        return result;
    }

    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(response.body, &parseError);
    if (parseError.error != QJsonParseError::NoError
        || !document.isObject()) {
        result.error = operationError(
            QStringLiteral("speech.baidu_token"),
            tr8("百度令牌获取失败：返回内容不是有效 JSON。"),
            QString::fromUtf8(response.body)
        );
        return result;
    }

    const QJsonObject root = document.object();
    const QString token =
        root.value(QStringLiteral("access_token")).toString().trimmed();
    if (token.isEmpty()) {
        QString message = tokenApiError(root);
        if (message.isEmpty()) {
            message = tr8("返回内容没有 access_token。");
        }
        result.error = operationError(
            QStringLiteral("speech.baidu_token"),
            tr8("百度令牌获取失败：") + message,
            QString::fromUtf8(response.body)
        );
        return result;
    }

    const int expiresIn =
        root.value(QStringLiteral("expires_in")).toInt(2592000);
    m_accessToken = token;
    m_tokenExpiry = QDateTime::currentDateTime().addSecs(expiresIn);
    if (m_shareAccessTokenCache) {
        writeSharedAccessToken(
            m_secrets,
            m_accessToken,
            m_tokenExpiry
        );
    }
    result.token = token;
    return result;
}

QSharedPointer<ISpeechProvider> createBaiduSpeechProvider(
    bool useSystemProxy)
{
    return QSharedPointer<ISpeechProvider>(
        new BaiduSpeechProvider(useSystemProxy)
    );
}

QSharedPointer<ISpeechProvider> createBaiduSpeechProvider(
    const SecretConfig &secrets,
    bool useSystemProxy)
{
    return QSharedPointer<ISpeechProvider>(
        new BaiduSpeechProvider(
            createProviderNetworkTransport(),
            [secrets]() { return secrets; },
            useSystemProxy
        )
    );
}
