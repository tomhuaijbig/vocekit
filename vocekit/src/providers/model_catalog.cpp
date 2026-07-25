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
    options << ModelOption{QStringLiteral("openai:gpt-5.5"), QStringLiteral("gpt-5.5"), mcTr8("OpenAI")};
    options << ModelOption{QStringLiteral("openai:gpt-5.4"), QStringLiteral("gpt-5.4"), mcTr8("OpenAI")};
    options << ModelOption{QStringLiteral("openai:gpt-5.4-mini"), QStringLiteral("gpt-5.4-mini"), mcTr8("OpenAI")};
    options << ModelOption{QStringLiteral("claude:claude-opus-4-8"), QStringLiteral("claude-opus-4-8"), mcTr8("Anthropic")};
    options << ModelOption{QStringLiteral("claude:claude-opus-4-7"), QStringLiteral("claude-opus-4-7"), mcTr8("Anthropic")};
    options << ModelOption{QStringLiteral("claude:claude-sonnet-4-6"), QStringLiteral("claude-sonnet-4-6"), mcTr8("Anthropic")};
    options << ModelOption{QStringLiteral("claude:claude-haiku-4-5"), QStringLiteral("claude-haiku-4-5"), mcTr8("Anthropic")};
    return options;
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
    for (const ModelOption &option : options) {
        if (option.id == id) {
            return option.title;
        }
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
    for (const ModelOption &option : options) {
        if (option.id == trimmed) {
            if (option.title == trimmed) {
                return option.title;
            }
            return option.title
                + QString::fromUtf8("\uff08")
                + trimmed
                + QString::fromUtf8("\uff09");
        }
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

    const QString customId = customModelIdForTitle(trimmed, options);
    if (!customId.isEmpty()) {
        return customId;
    }
    if (trimmed.startsWith(QStringLiteral("gpt-"))) {
        return QStringLiteral("openai:") + trimmed;
    }
    if (trimmed.startsWith(QStringLiteral("claude-"))) {
        return QStringLiteral("claude:") + trimmed;
    }
    if (trimmed.startsWith(QStringLiteral("custom:"))) {
        return trimmed;
    }
    if (trimmed.startsWith(QStringLiteral("openai:"))) {
        return QStringLiteral("openai:gpt-5.5");
    }
    if (trimmed.startsWith(QStringLiteral("claude:"))) {
        return QStringLiteral("claude:claude-opus-4-8");
    }

    return fallback.trimmed().isEmpty() ? defaultModelForFunction(QString()) : fallback;
}
