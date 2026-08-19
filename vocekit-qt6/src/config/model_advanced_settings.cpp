#include "model_advanced_settings.h"

#include "app_paths.h"
#include "../file_utils.h"

#include <QJsonArray>
#include <QJsonDocument>

namespace {

QString defaultPath()
{
    return appConfigFilePath(QStringLiteral("model_advanced.json"));
}

QJsonObject readRoot(const QString &path)
{
    QJsonObject root;
    readJsonObjectFile(path, &root);
    return root;
}

QString normalizedPresetId(QString id, int index)
{
    id = id.trimmed();
    return id.isEmpty()
        ? QStringLiteral("prompt-") + QString::number(index + 1)
        : id;
}

} // namespace

QString ModelAdvancedProfile::activeSystemPrompt() const
{
    if (!systemPromptOverrideEnabled) {
        return QString();
    }
    for (const ModelSystemPromptPreset &preset : systemPrompts) {
        if (preset.id == activeSystemPromptId) {
            return preset.content;
        }
    }
    return QString();
}

QString modelAdvancedProfileKey(const QString &modelId)
{
    const QString key = modelId.trimmed();
    return key.isEmpty() ? QStringLiteral("default") : key;
}

QJsonObject modelAdvancedProfileToJson(const ModelAdvancedProfile &profile)
{
    QJsonObject object;
    object.insert(QStringLiteral("enabled"), profile.enabled);
    object.insert(QStringLiteral("parameters"), profile.parameters);
    object.insert(QStringLiteral("raw_json"), profile.rawJson);

    QJsonObject promptSettings;
    promptSettings.insert(
        QStringLiteral("override_enabled"),
        profile.systemPromptOverrideEnabled
    );
    promptSettings.insert(
        QStringLiteral("active_id"),
        profile.activeSystemPromptId.trimmed()
    );
    QJsonArray prompts;
    for (const ModelSystemPromptPreset &preset : profile.systemPrompts) {
        QJsonObject prompt;
        prompt.insert(QStringLiteral("id"), preset.id.trimmed());
        prompt.insert(QStringLiteral("name"), preset.name.trimmed());
        prompt.insert(QStringLiteral("content"), preset.content);
        prompts.append(prompt);
    }
    promptSettings.insert(QStringLiteral("presets"), prompts);
    object.insert(QStringLiteral("system_prompt"), promptSettings);

    QJsonObject diagnostics;
    diagnostics.insert(
        QStringLiteral("models_endpoint"),
        profile.modelsEndpoint.trimmed()
    );
    QJsonArray fetchedModels;
    for (const QString &model : profile.fetchedModels) {
        if (!model.trimmed().isEmpty()) {
            fetchedModels.append(model.trimmed());
        }
    }
    diagnostics.insert(QStringLiteral("fetched_models"), fetchedModels);
    object.insert(QStringLiteral("diagnostics"), diagnostics);

    QJsonObject privacy;
    privacy.insert(
        QStringLiteral("log_request_response_content"),
        profile.logRequestResponseContent
    );
    object.insert(QStringLiteral("privacy"), privacy);

    QJsonObject pricing;
    if (profile.inputPricePerMillion >= 0.0) {
        pricing.insert(
            QStringLiteral("input_per_million"),
            profile.inputPricePerMillion
        );
    }
    if (profile.outputPricePerMillion >= 0.0) {
        pricing.insert(
            QStringLiteral("output_per_million"),
            profile.outputPricePerMillion
        );
    }
    if (profile.reasoningPricePerMillion >= 0.0) {
        pricing.insert(
            QStringLiteral("reasoning_per_million"),
            profile.reasoningPricePerMillion
        );
    }
    object.insert(QStringLiteral("pricing"), pricing);
    return object;
}

ModelAdvancedProfile modelAdvancedProfileFromJson(
    const QString &key,
    const QJsonObject &object)
{
    ModelAdvancedProfile profile;
    profile.key = modelAdvancedProfileKey(key);
    profile.enabled = object.value(QStringLiteral("enabled")).toBool(false);
    profile.parameters = object.value(QStringLiteral("parameters")).toObject();
    profile.rawJson = object.value(QStringLiteral("raw_json")).toObject();

    const QJsonObject promptSettings =
        object.value(QStringLiteral("system_prompt")).toObject();
    profile.systemPromptOverrideEnabled = promptSettings
        .value(QStringLiteral("override_enabled"))
        .toBool(false);
    profile.activeSystemPromptId = promptSettings
        .value(QStringLiteral("active_id"))
        .toString()
        .trimmed();
    const QJsonArray prompts =
        promptSettings.value(QStringLiteral("presets")).toArray();
    int index = 0;
    for (const QJsonValue &value : prompts) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject object = value.toObject();
        ModelSystemPromptPreset preset;
        preset.id = normalizedPresetId(
            object.value(QStringLiteral("id")).toString(),
            index
        );
        preset.name = object.value(QStringLiteral("name")).toString().trimmed();
        preset.content = object.value(QStringLiteral("content")).toString();
        if (preset.name.isEmpty()) {
            preset.name = preset.id;
        }
        profile.systemPrompts.append(preset);
        ++index;
    }

    const QJsonObject diagnostics =
        object.value(QStringLiteral("diagnostics")).toObject();
    profile.modelsEndpoint = diagnostics
        .value(QStringLiteral("models_endpoint"))
        .toString()
        .trimmed();
    for (const QJsonValue &value :
         diagnostics.value(QStringLiteral("fetched_models")).toArray()) {
        const QString model = value.toString().trimmed();
        if (!model.isEmpty() && !profile.fetchedModels.contains(model)) {
            profile.fetchedModels.append(model);
        }
    }

    profile.logRequestResponseContent = object
        .value(QStringLiteral("privacy"))
        .toObject()
        .value(QStringLiteral("log_request_response_content"))
        .toBool(false);

    const QJsonObject pricing = object.value(QStringLiteral("pricing")).toObject();
    profile.inputPricePerMillion = pricing.contains(
        QStringLiteral("input_per_million")
    ) ? pricing.value(QStringLiteral("input_per_million")).toDouble(-1.0) : -1.0;
    profile.outputPricePerMillion = pricing.contains(
        QStringLiteral("output_per_million")
    ) ? pricing.value(QStringLiteral("output_per_million")).toDouble(-1.0) : -1.0;
    profile.reasoningPricePerMillion = pricing.contains(
        QStringLiteral("reasoning_per_million")
    ) ? pricing.value(QStringLiteral("reasoning_per_million")).toDouble(-1.0) : -1.0;
    return profile;
}

ModelAdvancedSettingsStore::ModelAdvancedSettingsStore(const QString &path)
    : m_path(path.trimmed().isEmpty() ? defaultPath() : path)
{
}

QString ModelAdvancedSettingsStore::path() const
{
    return m_path;
}

ModelAdvancedProfile ModelAdvancedSettingsStore::loadProfile(
    const QString &modelId) const
{
    const QString key = modelAdvancedProfileKey(modelId);
    const QJsonObject profiles = readRoot(m_path)
        .value(QStringLiteral("profiles"))
        .toObject();
    return modelAdvancedProfileFromJson(key, profiles.value(key).toObject());
}

QVector<ModelAdvancedProfile> ModelAdvancedSettingsStore::loadProfiles() const
{
    QVector<ModelAdvancedProfile> result;
    const QJsonObject profiles = readRoot(m_path)
        .value(QStringLiteral("profiles"))
        .toObject();
    for (auto it = profiles.constBegin(); it != profiles.constEnd(); ++it) {
        if (it.value().isObject()) {
            result.append(modelAdvancedProfileFromJson(it.key(), it.value().toObject()));
        }
    }
    return result;
}

bool ModelAdvancedSettingsStore::saveProfile(
    const ModelAdvancedProfile &profile) const
{
    QJsonObject root = readRoot(m_path);
    QJsonObject profiles = root.value(QStringLiteral("profiles")).toObject();
    const QString key = modelAdvancedProfileKey(profile.key);
    profiles.insert(key, modelAdvancedProfileToJson(profile));
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("profiles"), profiles);
    return writeBytesAtomically(
        m_path,
        QJsonDocument(root).toJson(QJsonDocument::Indented)
    );
}

ModelAdvancedProfile loadModelAdvancedProfile(const QString &modelId)
{
    return ModelAdvancedSettingsStore().loadProfile(modelId);
}

bool saveModelAdvancedProfile(const ModelAdvancedProfile &profile)
{
    return ModelAdvancedSettingsStore().saveProfile(profile);
}
