#include "model_catalog.h"

#include "../config/app_settings_defaults.h"

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
    if (value == QStringLiteral("deepseek-v4-flash")
        || value == QStringLiteral("deepseek-v4-pro")) {
        return value;
    }
    if (value == QStringLiteral("gpt-5.6-sol")
        || value == QStringLiteral("openai:gpt-5.6-sol")) {
        return QStringLiteral("openai:gpt-5.6-sol");
    }
    if (value == QStringLiteral("gpt-5.6-terra")
        || value == QStringLiteral("openai:gpt-5.6-terra")) {
        return QStringLiteral("openai:gpt-5.6-terra");
    }
    if (value == QStringLiteral("gpt-5.6-luna")
        || value == QStringLiteral("openai:gpt-5.6-luna")) {
        return QStringLiteral("openai:gpt-5.6-luna");
    }
    if (value == QStringLiteral("gpt-5.5")
        || value == QStringLiteral("openai:gpt-5.5")) {
        return QStringLiteral("openai:gpt-5.6-sol");
    }
    if (value == QStringLiteral("gpt-5.4-mini")
        || value == QStringLiteral("openai:gpt-5.4-mini")) {
        return QStringLiteral("openai:gpt-5.6-luna");
    }
    if (value == QStringLiteral("gpt-5.4")
        || value == QStringLiteral("openai:gpt-5.4")
        || value == QStringLiteral("gpt-4o")
        || value == QStringLiteral("openai:gpt-4o")
        || value == QStringLiteral("gpt-4.1")
        || value == QStringLiteral("openai:gpt-4.1")) {
        return QStringLiteral("openai:gpt-5.6-terra");
    }
    if (value == QStringLiteral("claude-fable-5")
        || value == QStringLiteral("claude:claude-fable-5")) {
        return QStringLiteral("claude:claude-fable-5");
    }
    if (value == QStringLiteral("claude-opus-5")
        || value == QStringLiteral("claude:claude-opus-5")) {
        return QStringLiteral("claude:claude-opus-5");
    }
    if (value == QStringLiteral("claude-sonnet-5")
        || value == QStringLiteral("claude:claude-sonnet-5")) {
        return QStringLiteral("claude:claude-sonnet-5");
    }
    if (value == QStringLiteral("claude-haiku-4-5")
        || value == QStringLiteral("claude:claude-haiku-4-5")) {
        return QStringLiteral("claude:claude-haiku-4-5");
    }
    if (value == QStringLiteral("opus-4-8")
        || value == QStringLiteral("claude-opus-4-8")
        || value == QStringLiteral("claude:opus-4-8")
        || value == QStringLiteral("claude:claude-opus-4-8")
        || value == QStringLiteral("opus-4-7")
        || value == QStringLiteral("claude-opus-4-7")
        || value == QStringLiteral("claude:opus-4-7")
        || value == QStringLiteral("claude:claude-opus-4-7")) {
        return QStringLiteral("claude:claude-opus-5");
    }
    if (value == QStringLiteral("sonnet-4-6")
        || value == QStringLiteral("claude-sonnet-4-6")
        || value == QStringLiteral("claude:sonnet-4-6")
        || value == QStringLiteral("claude:claude-sonnet-4-6")
        || value == QStringLiteral("claude-3-7-sonnet")
        || value == QStringLiteral("claude:claude-3-7-sonnet")
        || value == QStringLiteral("claude-3-5-haiku")
        || value == QStringLiteral("claude:claude-3-5-haiku")) {
        return QStringLiteral("claude:claude-sonnet-5");
    }
    return QString();
}

QString migratedBuiltInModelId(const QString &value)
{
    const QString knownId = canonicalCurrentOrRetiredModelId(value);
    if (!knownId.isEmpty()) {
        return knownId;
    }

    QString modelId = value;
    bool isOpenAi = false;
    if (modelId.startsWith(QStringLiteral("openai:"))) {
        isOpenAi = true;
        modelId = modelId.mid(QStringLiteral("openai:").size());
    }
    if (modelId.startsWith(QStringLiteral("gpt-")) || isOpenAi) {
        return QStringLiteral("openai:gpt-5.6-terra");
    }

    bool isClaude = false;
    if (modelId.startsWith(QStringLiteral("claude:"))) {
        isClaude = true;
        modelId = modelId.mid(QStringLiteral("claude:").size());
    }
    if (modelId.startsWith(QStringLiteral("claude-3"))
        || modelId.startsWith(QStringLiteral("3."))
        || modelId.startsWith(QStringLiteral("claude-"))
        || isClaude) {
        return QStringLiteral("claude:claude-sonnet-5");
    }
    return QString();
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

QVector<ModelOption> modelOptions()
{
    return modelOptionsForSecrets(loadSecrets());
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
    return trimmed;
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

    return fallback.trimmed().isEmpty() ? defaultModelForFunction(QString()) : fallback;
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
