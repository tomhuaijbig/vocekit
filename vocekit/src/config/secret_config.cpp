#include "secret_config.h"
#include "secret_store.h"

#include <QString>
#include <algorithm>

namespace {

QString scTr8(const char *text)
{
    return QString::fromUtf8(text);
}

} // namespace

QString normalizeCustomModelProfileId(QString id)
{
    id = id.trimmed();
    QString normalized;
    for (const QChar &ch : id) {
        if (ch.isLetterOrNumber() || ch == QLatin1Char('_') || ch == QLatin1Char('-')) {
            normalized.append(ch);
        }
    }
    return normalized.isEmpty() ? QStringLiteral("model") : normalized;
}

bool SecretConfig::hasXfyun() const
{
    return !xfyunAppId.trimmed().isEmpty()
        && !xfyunApiKey.trimmed().isEmpty()
        && !xfyunApiSecret.trimmed().isEmpty();
}

bool SecretConfig::hasCustomSpeech() const
{
    return !customSpeechUrl.trimmed().isEmpty();
}

bool SecretConfig::hasCustomModel() const
{
    const QVector<CustomModelProfile> profiles = effectiveCustomModels();
    return std::any_of(
        profiles.cbegin(),
        profiles.cend(),
        [](const CustomModelProfile &profile) { return profile.hasEndpoint(); }
    );
}

QVector<CustomModelProfile> SecretConfig::effectiveCustomModels() const
{
    QVector<CustomModelProfile> profiles = customModels;
    if (profiles.isEmpty() && !customModelUrl.trimmed().isEmpty()) {
        CustomModelProfile legacy;
        legacy.id = QStringLiteral("model");
        legacy.name = scTr8("自定义大模型");
        legacy.url = customModelUrl;
        legacy.apiKey = customModelApiKey;
        legacy.model = customModelName;
        profiles.append(legacy);
    }
    return profiles;
}

CustomModelProfile SecretConfig::customModelProfileForProviderId(const QString &providerId) const
{
    const QVector<CustomModelProfile> profiles = effectiveCustomModels();
    if (profiles.isEmpty()) {
        return CustomModelProfile();
    }

    QString rawId = providerId.trimmed();
    if (rawId.startsWith(QStringLiteral("custom:"), Qt::CaseInsensitive)) {
        rawId = rawId.mid(QStringLiteral("custom:").size());
    }
    const QString normalized = normalizeCustomModelProfileId(rawId);
    const auto matched = std::find_if(
        profiles.cbegin(),
        profiles.cend(),
        [normalized](const CustomModelProfile &profile) {
            return normalizeCustomModelProfileId(profile.id) == normalized;
        }
    );
    if (matched != profiles.cend()) {
        return *matched;
    }
    if (normalized == QStringLiteral("model") || rawId.trimmed().isEmpty()) {
        return profiles.constFirst();
    }
    return CustomModelProfile();
}

SecretConfig loadSecrets()
{
    return SecretStore().load();
}

bool saveSecrets(const SecretConfig &secrets)
{
    return SecretStore().save(secrets);
}
