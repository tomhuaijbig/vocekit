#include "secret_store.h"

#include "../file_utils.h"

#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace {

QString ssTr8(const char *text)
{
    return QString::fromUtf8(text);
}

QString defaultSecretBasePath()
{
    QDir dir(QCoreApplication::applicationDirPath());
    const QString folder = dir.dirName().toLower();
    if (folder == QStringLiteral("debug") || folder == QStringLiteral("release")) {
        dir.cdUp();
    }
    return dir.absolutePath();
}

QString defaultSecretPath()
{
    return QDir(defaultSecretBasePath()).filePath(QStringLiteral("config/secrets.json"));
}

QVector<QJsonObject> customModelObjectsFromSecretsRoot(const QJsonObject &root)
{
    QVector<QJsonObject> objects;
    const QJsonArray array = root.value(QStringLiteral("custom_models")).toArray();
    for (const QJsonValue &value : array) {
        if (value.isObject()) {
            objects.append(value.toObject());
        }
    }
    if (objects.isEmpty() && !root.value(QStringLiteral("custom_model_url")).toString().trimmed().isEmpty()) {
        QJsonObject legacy;
        legacy.insert(QStringLiteral("id"), QStringLiteral("model"));
        legacy.insert(QStringLiteral("name"), ssTr8("自定义大模型"));
        legacy.insert(QStringLiteral("url"), root.value(QStringLiteral("custom_model_url")).toString());
        legacy.insert(QStringLiteral("api_key"), root.value(QStringLiteral("custom_model_api_key")).toString());
        legacy.insert(QStringLiteral("model"), root.value(QStringLiteral("custom_model_name")).toString());
        objects.append(legacy);
    }
    return objects;
}

CustomModelProfile customModelProfileFromJsonObject(const QJsonObject &object)
{
    CustomModelProfile profile;
    profile.id = normalizeCustomModelProfileId(object.value(QStringLiteral("id")).toString());
    profile.name = object.value(QStringLiteral("name")).toString().trimmed();
    profile.url = object.value(QStringLiteral("url")).toString().trimmed();
    profile.apiKey = object.value(QStringLiteral("api_key")).toString().trimmed();
    profile.model = object.value(QStringLiteral("model")).toString().trimmed();
    if (profile.name.isEmpty()) {
        profile.name = profile.model.trimmed().isEmpty() ? ssTr8("自定义大模型") : profile.model.trimmed();
    }
    return profile;
}

QJsonObject customModelProfileToJsonObject(const CustomModelProfile &profile)
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), normalizeCustomModelProfileId(profile.id));
    object.insert(QStringLiteral("name"), profile.name.trimmed());
    object.insert(QStringLiteral("url"), profile.url.trimmed());
    object.insert(QStringLiteral("api_key"), profile.apiKey.trimmed());
    object.insert(QStringLiteral("model"), profile.model.trimmed());
    return object;
}

} // namespace

SecretStore::SecretStore(const QString &path)
    : m_path(path.trimmed().isEmpty() ? defaultSecretPath() : path)
{
}

SecretConfig SecretStore::load() const
{
    SecretConfig secrets;
    QJsonObject root;
    if (readJsonObjectFile(m_path, &root)) {
        secrets.deepseekApiKey = root.value(QStringLiteral("deepseek_api_key")).toString();
        secrets.openaiApiKey = root.value(QStringLiteral("openai_api_key")).toString();
        secrets.anthropicApiKey = root.value(QStringLiteral("anthropic_api_key")).toString();
        secrets.baiduApiKey = root.value(QStringLiteral("baidu_api_key")).toString();
        secrets.baiduSecretKey = root.value(QStringLiteral("baidu_secret_key")).toString();
        secrets.baiduAppId = root.value(QStringLiteral("baidu_app_id")).toString();
        secrets.xfyunAppId = root.value(QStringLiteral("xfyun_app_id")).toString();
        secrets.xfyunApiKey = root.value(QStringLiteral("xfyun_api_key")).toString();
        secrets.xfyunApiSecret = root.value(QStringLiteral("xfyun_api_secret")).toString();
        secrets.customSpeechUrl = root.value(QStringLiteral("custom_speech_url")).toString();
        secrets.customSpeechApiKey = root.value(QStringLiteral("custom_speech_api_key")).toString();
        secrets.customSpeechModel = root.value(QStringLiteral("custom_speech_model")).toString();
        secrets.customOcrUrl = root.value(QStringLiteral("custom_ocr_url")).toString();
        secrets.customOcrApiKey = root.value(QStringLiteral("custom_ocr_api_key")).toString();
        secrets.customOcrModel = root.value(QStringLiteral("custom_ocr_model")).toString();
        secrets.customModelUrl = root.value(QStringLiteral("custom_model_url")).toString();
        secrets.customModelApiKey = root.value(QStringLiteral("custom_model_api_key")).toString();
        secrets.customModelName = root.value(QStringLiteral("custom_model_name")).toString();
        for (const QJsonObject &object : customModelObjectsFromSecretsRoot(root)) {
            const CustomModelProfile profile = customModelProfileFromJsonObject(object);
            if (!profile.id.trimmed().isEmpty()) {
                secrets.customModels.append(profile);
            }
        }
    }
    return secrets;
}

bool SecretStore::save(const SecretConfig &secrets) const
{
    QJsonObject root;
    root.insert(QStringLiteral("deepseek_api_key"), secrets.deepseekApiKey.trimmed());
    root.insert(QStringLiteral("openai_api_key"), secrets.openaiApiKey.trimmed());
    root.insert(QStringLiteral("anthropic_api_key"), secrets.anthropicApiKey.trimmed());
    root.insert(QStringLiteral("baidu_api_key"), secrets.baiduApiKey.trimmed());
    root.insert(QStringLiteral("baidu_secret_key"), secrets.baiduSecretKey.trimmed());
    root.insert(QStringLiteral("baidu_app_id"), secrets.baiduAppId.trimmed());
    root.insert(QStringLiteral("xfyun_app_id"), secrets.xfyunAppId.trimmed());
    root.insert(QStringLiteral("xfyun_api_key"), secrets.xfyunApiKey.trimmed());
    root.insert(QStringLiteral("xfyun_api_secret"), secrets.xfyunApiSecret.trimmed());
    root.insert(QStringLiteral("custom_speech_url"), secrets.customSpeechUrl.trimmed());
    root.insert(QStringLiteral("custom_speech_api_key"), secrets.customSpeechApiKey.trimmed());
    root.insert(QStringLiteral("custom_speech_model"), secrets.customSpeechModel.trimmed());
    root.insert(QStringLiteral("custom_ocr_url"), secrets.customOcrUrl.trimmed());
    root.insert(QStringLiteral("custom_ocr_api_key"), secrets.customOcrApiKey.trimmed());
    root.insert(QStringLiteral("custom_ocr_model"), secrets.customOcrModel.trimmed());
    root.insert(QStringLiteral("custom_model_url"), secrets.customModelUrl.trimmed());
    root.insert(QStringLiteral("custom_model_api_key"), secrets.customModelApiKey.trimmed());
    root.insert(QStringLiteral("custom_model_name"), secrets.customModelName.trimmed());

    QJsonArray customModels;
    for (const CustomModelProfile &profile : secrets.customModels) {
        customModels.append(customModelProfileToJsonObject(profile));
    }
    root.insert(QStringLiteral("custom_models"), customModels);

    return writeBytesAtomically(m_path, QJsonDocument(root).toJson(QJsonDocument::Indented));
}
