#include "model_catalog.h"

#include "../config/app_paths.h"
#include "../config/app_settings_defaults.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

namespace {

QString mcTr8(const char *text)
{
    return QString::fromUtf8(text);
}

QVector<ModelOption> builtInModelOptions()
{
    QVector<ModelOption> options;
    options << ModelOption{QStringLiteral("deepseek-v4-flash"), QStringLiteral("deepseek-v4-flash"), mcTr8("DeepSeek")};
    options << ModelOption{QStringLiteral("deepseek-v4-pro"), QStringLiteral("deepseek-v4-pro"), mcTr8("DeepSeek")};
    options << ModelOption{QStringLiteral("openai:gpt-5.6-sol"), QStringLiteral("GPT-5.6 Sol"), mcTr8("OpenAI")};
    options << ModelOption{QStringLiteral("openai:gpt-5.6-terra"), QStringLiteral("GPT-5.6 Terra"), mcTr8("OpenAI")};
    options << ModelOption{QStringLiteral("openai:gpt-5.6-luna"), QStringLiteral("GPT-5.6 Luna"), mcTr8("OpenAI")};
    options << ModelOption{QStringLiteral("claude:claude-fable-5"), QStringLiteral("Claude Fable 5"), mcTr8("Anthropic")};
    options << ModelOption{QStringLiteral("claude:claude-opus-5"), QStringLiteral("Claude Opus 5"), mcTr8("Anthropic")};
    options << ModelOption{QStringLiteral("claude:claude-sonnet-5"), QStringLiteral("Claude Sonnet 5"), mcTr8("Anthropic")};
    options << ModelOption{QStringLiteral("claude:claude-haiku-4-5"), QStringLiteral("Claude Haiku 4.5"), mcTr8("Anthropic")};
    return options;
}

QString canonicalCurrentOrRetiredModelId(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.startsWith(QStringLiteral("openai:"))
        || trimmed.startsWith(QStringLiteral("claude:"))
        || trimmed.startsWith(QStringLiteral("custom:"))
        || trimmed.startsWith(QStringLiteral("deepseek-"))) {
        return trimmed;
    }
    if (trimmed.startsWith(QStringLiteral("gpt-"))) {
        return QStringLiteral("openai:") + trimmed;
    }
    if (trimmed.startsWith(QStringLiteral("claude-"))) {
        return QStringLiteral("claude:") + trimmed;
    }
    return QString();
}

QString migratedBuiltInModelId(const QString &value)
{
    return canonicalCurrentOrRetiredModelId(value);
}

const ModelOption *modelOptionForId(const QVector<ModelOption> &options, const QString &id)
{
    for (const ModelOption &option : options) {
        if (option.id == id) {
            return &option;
        }
    }
    return 0;
}

QString displayTextForOption(const ModelOption &option, const QString &id)
{
    if (option.title == id) {
        return option.title;
    }
    return option.title
        + QString::fromUtf8("\uff08")
        + id
        + QString::fromUtf8("\uff09");
}

QString customModelIdForTitle(const QString &title, const QVector<ModelOption> &options)
{
    for (const ModelOption &option : options) {
        if (option.id.startsWith(QStringLiteral("custom:")) && option.title == title) {
            return option.id;
        }
    }
    return QString();
}

QString fetchedModelOptionId(
    const QString &profileKey,
    const QString &fetchedModel)
{
    const QString model = fetchedModel.trimmed();
    if (model.isEmpty()) {
        return QString();
    }
    const QString provider = modelProvider(profileKey);
    if (provider == QStringLiteral("openai")) {
        return model.startsWith(QStringLiteral("openai:"))
            ? model
            : QStringLiteral("openai:") + model;
    }
    if (provider == QStringLiteral("claude")) {
        return model.startsWith(QStringLiteral("claude:"))
            ? model
            : QStringLiteral("claude:") + model;
    }
    if (provider == QStringLiteral("deepseek")) {
        return model;
    }
    // A custom profile ID identifies the endpoint and credentials, while the
    // fetched model name belongs in that profile's advanced Model override.
    // Creating a new custom:<model> ID here would lose the endpoint binding.
    return QString();
}

QVector<ModelAdvancedProfile> cachedModelProfiles()
{
    QVector<ModelAdvancedProfile> result;
    QFile file(appConfigFilePath(QStringLiteral("model_advanced.json")));
    if (!file.open(QIODevice::ReadOnly)) {
        return result;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(
        file.readAll(),
        &parseError
    );
    if (parseError.error != QJsonParseError::NoError
        || !document.isObject()) {
        return result;
    }
    const QJsonObject profiles = document.object()
        .value(QStringLiteral("profiles"))
        .toObject();
    for (auto it = profiles.constBegin(); it != profiles.constEnd(); ++it) {
        if (!it.value().isObject()) {
            continue;
        }
        ModelAdvancedProfile profile;
        profile.key = it.key();
        const QJsonArray models = it.value()
            .toObject()
            .value(QStringLiteral("diagnostics"))
            .toObject()
            .value(QStringLiteral("fetched_models"))
            .toArray();
        for (const QJsonValue &value : models) {
            const QString model = value.toString().trimmed();
            if (!model.isEmpty()
                && !profile.fetchedModels.contains(model)) {
                profile.fetchedModels.append(model);
            }
        }
        if (!profile.fetchedModels.isEmpty()) {
            result.append(profile);
        }
    }
    return result;
}

} // namespace

QVector<ModelOption> modelOptionsForSecrets(const SecretConfig &secrets)
{
    QVector<ModelOption> options = builtInModelOptions();
    const QVector<CustomModelProfile> profiles = secrets.effectiveCustomModels();
    for (const CustomModelProfile &profile : profiles) {
        const QString id = normalizeCustomModelProfileId(profile.id);
        if (id.isEmpty() || !profile.hasEndpoint()) {
            continue;
        }

        QString title = profile.name.trimmed();
        if (title.isEmpty()) {
            title = profile.model.trimmed();
        }
        if (title.isEmpty()) {
            title = QString::fromUtf8("\u81ea\u5b9a\u4e49\u5927\u6a21\u578b");
        }

        ModelOption option;
        option.id = QStringLiteral("custom:") + id;
        option.title = title;
        option.hint = QString::fromUtf8("\u81ea\u5b9a\u4e49");
        options.append(option);
    }
    return options;
}

QVector<ModelOption> modelOptionsForSecretsAndProfiles(
    const SecretConfig &secrets,
    const QVector<ModelAdvancedProfile> &advancedProfiles)
{
    QVector<ModelOption> options = modelOptionsForSecrets(secrets);
    for (const ModelAdvancedProfile &profile : advancedProfiles) {
        const QString provider = modelProvider(profile.key);
        for (const QString &fetchedModel : profile.fetchedModels) {
            const QString id = fetchedModelOptionId(
                profile.key,
                fetchedModel
            );
            if (id.isEmpty() || modelOptionForId(options, id)) {
                continue;
            }
            ModelOption option;
            option.id = id;
            option.title = fetchedModel.trimmed();
            option.hint = provider + mcTr8("（接口缓存）");
            options.append(option);
        }
    }
    return options;
}

QVector<ModelOption> modelOptions()
{
    return modelOptionsForSecretsAndProfiles(
        loadSecrets(),
        cachedModelProfiles()
    );
}

QString modelTitle(const QString &id)
{
    const QVector<ModelOption> options = modelOptions();
    const QString trimmed = id.trimmed();
    if (const ModelOption *option = modelOptionForId(options, trimmed)) {
        return option->title;
    }

    const QString migratedId = migratedBuiltInModelId(trimmed);
    if (const ModelOption *option = modelOptionForId(options, migratedId)) {
        return option->title;
    }
    if (!migratedId.isEmpty()) {
        return migratedId;
    }

    if (!trimmed.isEmpty()) {
        return trimmed;
    }

    if (!options.isEmpty()) {
        return options.constFirst().title;
    }
    return QStringLiteral("deepseek-v4-flash");
}

QString modelDisplayText(const QString &id)
{
    const QString trimmed = id.trimmed();
    if (trimmed.isEmpty()) {
        return QString::fromUtf8("\u672a\u8c03\u7528\u5927\u6a21\u578b");
    }

    const QVector<ModelOption> options = modelOptions();
    if (const ModelOption *option = modelOptionForId(options, trimmed)) {
        return displayTextForOption(*option, trimmed);
    }

    const QString migratedId = migratedBuiltInModelId(trimmed);
    if (const ModelOption *option = modelOptionForId(options, migratedId)) {
        return displayTextForOption(*option, trimmed);
    }
    return migratedId.isEmpty() ? trimmed : migratedId;
}

QString normalizeModelId(const QString &value, const QString &fallback)
{
    const QString trimmed = value.trimmed();
    const QVector<ModelOption> options = modelOptions();
    for (const ModelOption &option : options) {
        if (option.id == trimmed) {
            return option.id;
        }
        if (option.title == trimmed) {
            return option.id;
        }
    }

    const QString migratedId = migratedBuiltInModelId(trimmed);
    if (!migratedId.isEmpty()) {
        return migratedId;
    }

    const QString customId = customModelIdForTitle(trimmed, options);
    if (!customId.isEmpty()) {
        return customId;
    }
    if (trimmed.startsWith(QStringLiteral("custom:"))) {
        return trimmed;
    }

    if (!trimmed.isEmpty()) {
        return trimmed;
    }
    return fallback.trimmed().isEmpty()
        ? defaultModelForFunction(QString())
        : fallback;
}

QString normalizeExplicitModelId(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }
    const QString knownId = canonicalCurrentOrRetiredModelId(trimmed);
    if (!knownId.isEmpty()) {
        return knownId;
    }
    return trimmed;
}
