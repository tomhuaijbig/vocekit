#include "streaming_speech_session_factory.h"

namespace {

QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

} // namespace

StreamingSpeechSessionCreation createStreamingSpeechSession(
    const StreamingSpeechSessionRequest &request,
    const StreamingSpeechCallbacks &callbacks,
    const StreamingSpeechSessionFactoryDependencies &dependencies
)
{
    StreamingSpeechSessionCreation result;
    const QString provider = request.provider.trimmed().toLower();
    if (provider != QStringLiteral("xfyun")
        && provider != QStringLiteral("baidu")) {
        result.unavailableReason = tr8(
            "当前语音服务商不支持实时识别，将使用录音结束后的整段识别。"
        );
        return result;
    }
    if (!dependencies.loadSecrets) {
        result.unavailableReason = tr8(
            "无法读取实时识别配置，将使用录音结束后的整段识别。"
        );
        return result;
    }

    const SecretConfig secrets = dependencies.loadSecrets();
    if (provider == QStringLiteral("xfyun")) {
        if (secrets.xfyunAppId.trimmed().isEmpty()
            || secrets.xfyunApiKey.trimmed().isEmpty()
            || secrets.xfyunApiSecret.trimmed().isEmpty()) {
            result.unavailableReason = tr8(
                "讯飞实时识别需要 AppID、API Key 和 API Secret，"
                "将使用录音结束后的整段识别。"
            );
            return result;
        }
        if (!dependencies.createXfyun) {
            result.unavailableReason = tr8("讯飞实时识别组件不可用。");
            return result;
        }
        result.session = dependencies.createXfyun(
            secrets,
            request,
            callbacks
        );
        return result;
    }

    bool appIdOk = false;
    const qlonglong appId = secrets.baiduAppId.trimmed().toLongLong(&appIdOk);
    if (!appIdOk || appId <= 0
        || secrets.baiduApiKey.trimmed().isEmpty()) {
        result.unavailableReason = tr8(
            "百度实时识别需要数字 AppID 和 API Key，"
            "将使用录音结束后的整段识别。"
        );
        return result;
    }
    if (!dependencies.createBaidu) {
        result.unavailableReason = tr8("百度实时识别组件不可用。");
        return result;
    }
    result.session = dependencies.createBaidu(
        secrets,
        request,
        callbacks
    );
    return result;
}
