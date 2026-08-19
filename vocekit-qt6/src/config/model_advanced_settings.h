#ifndef VOCEKIT_MODEL_ADVANCED_SETTINGS_H
#define VOCEKIT_MODEL_ADVANCED_SETTINGS_H

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

struct ModelSystemPromptPreset
{
    QString id;
    QString name;
    QString content;
};

// 每个模型拥有独立的高级配置。parameters 是便捷控件生成的字段，
// rawJson 是最后应用的自由 JSON 覆盖层；其中 null 表示删除基础请求字段。
struct ModelAdvancedProfile
{
    QString key;
    bool enabled = false;
    QJsonObject parameters;
    QJsonObject rawJson;
    QVector<ModelSystemPromptPreset> systemPrompts;
    QString activeSystemPromptId;
    bool systemPromptOverrideEnabled = false;
    QString modelsEndpoint;
    QStringList fetchedModels;
    // Full prompts, tool definitions and raw responses may contain private
    // user content. Persist them only after an explicit per-profile opt-in.
    bool logRequestResponseContent = false;
    double inputPricePerMillion = -1.0;
    double outputPricePerMillion = -1.0;
    double reasoningPricePerMillion = -1.0;

    QString activeSystemPrompt() const;
};

QString modelAdvancedProfileKey(const QString &modelId);
QJsonObject modelAdvancedProfileToJson(const ModelAdvancedProfile &profile);
ModelAdvancedProfile modelAdvancedProfileFromJson(
    const QString &key,
    const QJsonObject &object
);

class ModelAdvancedSettingsStore
{
public:
    explicit ModelAdvancedSettingsStore(const QString &path = QString());

    QString path() const;
    ModelAdvancedProfile loadProfile(const QString &modelId) const;
    QVector<ModelAdvancedProfile> loadProfiles() const;
    bool saveProfile(const ModelAdvancedProfile &profile) const;

private:
    QString m_path;
};

ModelAdvancedProfile loadModelAdvancedProfile(const QString &modelId);
bool saveModelAdvancedProfile(const ModelAdvancedProfile &profile);

#endif // VOCEKIT_MODEL_ADVANCED_SETTINGS_H
