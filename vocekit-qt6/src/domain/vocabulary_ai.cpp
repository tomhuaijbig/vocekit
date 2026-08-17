#include "vocabulary_ai.h"

#include "../storage/vocabulary_store.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QRegularExpression>
#include <QStringList>

namespace {

QString text(const char *value)
{
    return QString::fromUtf8(value);
}

QString extractJsonObjectText(QString value)
{
    value = value.trimmed();
    if (value.startsWith(QStringLiteral("```"))) {
        const int firstLineEnd = value.indexOf(QLatin1Char('\n'));
        if (firstLineEnd >= 0) {
            value = value.mid(firstLineEnd + 1).trimmed();
        }
        if (value.endsWith(QStringLiteral("```"))) {
            value.chop(3);
            value = value.trimmed();
        }
    }

    const int start = value.indexOf(QLatin1Char('{'));
    const int end = value.lastIndexOf(QLatin1Char('}'));
    if (start >= 0 && end > start) {
        return value.mid(start, end - start + 1);
    }
    return value;
}

QString jsonTextValue(const QJsonValue &value)
{
    if (value.isString()) {
        return value.toString().trimmed();
    }
    if (value.isDouble()) {
        return QString::number(value.toDouble()).trimmed();
    }
    if (value.isBool()) {
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    }
    return QString();
}

QString jsonTextListValue(const QJsonValue &value)
{
    if (value.isArray()) {
        QStringList parts;
        const QJsonArray array = value.toArray();
        for (const QJsonValue &item : array) {
            const QString itemText = jsonTextValue(item);
            if (!itemText.isEmpty()) {
                parts << itemText;
            }
        }
        return parts.join(text("，"));
    }
    return jsonTextValue(value);
}

} // namespace

QString compactPromptField(const QString &label, const QString &value)
{
    const QString trimmed = value.trimmed();
    return label + QStringLiteral(": ") + (trimmed.isEmpty() ? text("未填写") : trimmed);
}

QString mergeVocabularyAliasText(const QString &current, const QString &generated)
{
    QStringList merged;
    const QString combined = current + QStringLiteral("，") + generated;
    const QStringList parts = combined.split(
        QRegularExpression(QStringLiteral("[,，、;；\\n\\r]+")),
        Qt::SkipEmptyParts
    );
    for (QString part : parts) {
        part = part.trimmed();
        if (!part.isEmpty() && !merged.contains(part, Qt::CaseInsensitive)) {
            merged << part;
        }
    }
    return merged.join(text("，"));
}

VocabularySuggestion vocabularySuggestionFromModelReply(
    const QString &reply,
    const QString &fallbackText,
    const QString &scopeId
)
{
    VocabularySuggestion suggestion;
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(
        extractJsonObjectText(reply).toUtf8(),
        &parseError
    );
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return suggestion;
    }

    const QJsonObject root = doc.object();
    VocabularyEntry entry;
    entry.source = jsonTextValue(root.value(QStringLiteral("source")));
    entry.target = jsonTextValue(root.value(QStringLiteral("target")));
    entry.aliases = jsonTextListValue(root.value(QStringLiteral("aliases")));
    if (entry.source.isEmpty()) {
        entry.source = jsonTextValue(root.value(QStringLiteral("term")));
    }
    if (entry.target.isEmpty()) {
        entry.target = jsonTextValue(root.value(QStringLiteral("standard")));
    }
    if (entry.target.isEmpty()) {
        entry.target = jsonTextValue(root.value(QStringLiteral("replacement")));
    }
    entry.scopeId = normalizeVocabularyScope(
        root.value(QStringLiteral("scopeId")).toString(scopeId)
    );
    entry.matchMode = normalizeVocabularyMatchMode(
        root.value(QStringLiteral("matchMode")).toString(QStringLiteral("caseInsensitive"))
    );
    entry.note = jsonTextValue(root.value(QStringLiteral("note")));
    entry.enabled = root.value(QStringLiteral("enabled")).toBool(true);
    if (entry.source.isEmpty()) {
        entry.source = fallbackText.trimmed();
    }
    if (entry.target.isEmpty()) {
        entry.target = entry.source;
    }
    suggestion.entry = entry;
    suggestion.valid =
        !entry.source.isEmpty()
        && !entry.target.isEmpty()
        && vocabularyEntryHasCorrection(entry);
    return suggestion;
}
