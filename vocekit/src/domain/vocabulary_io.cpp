#include "vocabulary_io.h"

#include "../file_utils.h"
#include "../storage/vocabulary_store.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegExp>

namespace {

QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

QString columnValueAt(const QStringList &columns, const QMap<QString, int> &header, const QString &name, int fallbackIndex)
{
    const int index = header.contains(name) ? header.value(name) : fallbackIndex;
    return index >= 0 && index < columns.size() ? columns.at(index).trimmed() : QString();
}

} // namespace

QVector<VocabularyScopeOption> builtinVocabularyScopeOptions()
{
    QVector<VocabularyScopeOption> options;
    options.append({QStringLiteral("__all"), tr8("全部")});
    options.append({QStringLiteral("__global"), tr8("全局")});
    options.append({QStringLiteral("dictate"), tr8("听写")});
    options.append({QStringLiteral("translate"), tr8("翻译")});
    options.append({QStringLiteral("ask"), tr8("问答")});
    return options;
}

QString vocabularyScopeTitleForId(
    const QString &scopeId,
    const QVector<VocabularyScopeOption> &scopeOptions
)
{
    for (const VocabularyScopeOption &option : scopeOptions) {
        if (option.id == scopeId) {
            return option.title.trimmed().isEmpty() ? option.id : option.title;
        }
    }
    return scopeId;
}

QString normalizeVocabularyImportScope(
    const QString &value,
    const QVector<VocabularyScopeOption> &scopeOptions
)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()
        || trimmed == QStringLiteral("__all")
        || trimmed == tr8("全部")
        || trimmed == tr8("全局")) {
        return QStringLiteral("__global");
    }

    for (const VocabularyScopeOption &option : scopeOptions) {
        if (option.id == QStringLiteral("__all")) {
            continue;
        }
        if (trimmed == option.id || trimmed == option.title.trimmed()) {
            return option.id;
        }
    }
    return normalizeVocabularyScope(trimmed);
}

QString normalizeVocabularyImportMatchMode(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed == tr8("精确匹配")) return QStringLiteral("exact");
    if (trimmed == tr8("包含匹配")) return QStringLiteral("contains");
    if (trimmed == tr8("正则匹配")) return QStringLiteral("regex");
    if (trimmed == tr8("忽略大小写")) return QStringLiteral("caseInsensitive");
    return normalizeVocabularyMatchMode(trimmed);
}

bool vocabularyEntryMatchesSearch(
    const VocabularyEntry &entry,
    const QString &keyword,
    const QVector<VocabularyScopeOption> &scopeOptions
)
{
    const QString trimmed = keyword.trimmed();
    if (trimmed.isEmpty()) {
        return true;
    }
    return entry.source.contains(trimmed, Qt::CaseInsensitive)
        || entry.target.contains(trimmed, Qt::CaseInsensitive)
        || entry.aliases.contains(trimmed, Qt::CaseInsensitive)
        || entry.note.contains(trimmed, Qt::CaseInsensitive)
        || vocabularyScopeTitleForId(entry.scopeId, scopeOptions).contains(trimmed, Qt::CaseInsensitive)
        || vocabularyMatchModeTitle(entry.matchMode).contains(trimmed, Qt::CaseInsensitive);
}

bool vocabularyEntryInScope(const VocabularyEntry &entry, const QString &scopeId)
{
    if (scopeId == QStringLiteral("__all")) {
        return true;
    }
    return normalizeVocabularyScope(entry.scopeId) == scopeId;
}

QMap<QString, int> vocabularyCsvHeaderMap(const QStringList &columns)
{
    QMap<QString, int> header;
    for (int i = 0; i < columns.size(); ++i) {
        QString name = columns.at(i).trimmed();
        if (name.startsWith(QChar(0xfeff))) {
            name.remove(0, 1);
        }
        if (name.compare(QStringLiteral("source"), Qt::CaseInsensitive) == 0
            || name == tr8("原词")
            || name == tr8("错词")
            || name == tr8("原词/错词")) {
            header.insert(QStringLiteral("source"), i);
        } else if (name.compare(QStringLiteral("target"), Qt::CaseInsensitive) == 0
            || name == tr8("标准写法")
            || name == tr8("标准词")) {
            header.insert(QStringLiteral("target"), i);
        } else if (name.compare(QStringLiteral("aliases"), Qt::CaseInsensitive) == 0
            || name == tr8("别名")) {
            header.insert(QStringLiteral("aliases"), i);
        } else if (name.compare(QStringLiteral("scopeId"), Qt::CaseInsensitive) == 0
            || name == tr8("作用范围")) {
            header.insert(QStringLiteral("scopeId"), i);
        } else if (name.compare(QStringLiteral("matchMode"), Qt::CaseInsensitive) == 0
            || name == tr8("匹配方式")) {
            header.insert(QStringLiteral("matchMode"), i);
        } else if (name.compare(QStringLiteral("note"), Qt::CaseInsensitive) == 0
            || name == tr8("备注")) {
            header.insert(QStringLiteral("note"), i);
        } else if (name.compare(QStringLiteral("enabled"), Qt::CaseInsensitive) == 0
            || name == tr8("状态")
            || name == tr8("启用")) {
            header.insert(QStringLiteral("enabled"), i);
        }
    }
    return header;
}

VocabularyEntry vocabularyEntryFromCsvColumns(
    const QStringList &columns,
    const QMap<QString, int> &header,
    const QVector<VocabularyScopeOption> &scopeOptions
)
{
    VocabularyEntry entry;
    entry.source = columnValueAt(columns, header, QStringLiteral("source"), 0);
    entry.target = columnValueAt(columns, header, QStringLiteral("target"), 1);
    entry.aliases = columnValueAt(columns, header, QStringLiteral("aliases"), 2);
    entry.scopeId = normalizeVocabularyImportScope(
        columnValueAt(columns, header, QStringLiteral("scopeId"), 3),
        scopeOptions
    );
    entry.matchMode = normalizeVocabularyImportMatchMode(
        columnValueAt(columns, header, QStringLiteral("matchMode"), 4)
    );
    entry.note = columnValueAt(columns, header, QStringLiteral("note"), 5);

    const QString enabledText = columnValueAt(columns, header, QStringLiteral("enabled"), 6).toLower();
    entry.enabled = !(enabledText == QStringLiteral("0")
        || enabledText == QStringLiteral("false")
        || enabledText == tr8("否")
        || enabledText == tr8("关闭")
        || enabledText == tr8("停用"));
    return entry;
}

QVector<VocabularyEntry> parseVocabularyJsonImport(
    const QByteArray &data,
    const QVector<VocabularyScopeOption> &scopeOptions
)
{
    QVector<VocabularyEntry> entries;
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return entries;
    }

    QJsonArray items;
    if (doc.isArray()) {
        items = doc.array();
    } else if (doc.isObject()) {
        const QJsonObject root = doc.object();
        items = root.value(QStringLiteral("entries")).toArray();
        if (items.isEmpty()) {
            items = root.value(QStringLiteral("records")).toArray();
        }
    }

    for (const QJsonValue &value : items) {
        if (!value.isObject()) {
            continue;
        }
        VocabularyEntry entry = vocabularyEntryFromJsonObject(value.toObject());
        entry.scopeId = normalizeVocabularyImportScope(entry.scopeId, scopeOptions);
        entry.matchMode = normalizeVocabularyImportMatchMode(entry.matchMode);
        entries.append(entry);
    }
    return entries;
}

QVector<VocabularyEntry> parseVocabularyCsvImport(
    const QString &text,
    const QVector<VocabularyScopeOption> &scopeOptions
)
{
    QVector<VocabularyEntry> entries;
    const QStringList lines = text.split(QRegExp(QStringLiteral("\\r?\\n")), QString::SkipEmptyParts);
    if (lines.isEmpty()) {
        return entries;
    }

    int startLine = 0;
    QMap<QString, int> header = vocabularyCsvHeaderMap(parseCsvLine(lines.first()));
    if (!header.contains(QStringLiteral("source")) || !header.contains(QStringLiteral("target"))) {
        header.clear();
    } else {
        startLine = 1;
    }

    for (int i = startLine; i < lines.size(); ++i) {
        const QString line = lines.at(i).trimmed();
        if (line.isEmpty()) {
            continue;
        }
        entries.append(vocabularyEntryFromCsvColumns(parseCsvLine(line), header, scopeOptions));
    }
    return entries;
}

QVector<VocabularyEntry> parseVocabularyTextImport(const QString &text)
{
    QVector<VocabularyEntry> entries;
    const QStringList lines = text.split(QRegExp(QStringLiteral("\\r?\\n")), QString::SkipEmptyParts);
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }

        QString left;
        QString right;
        const QStringList separators = QStringList()
            << QStringLiteral("=>")
            << QStringLiteral("->")
            << QStringLiteral("\t")
            << QStringLiteral(",");
        for (const QString &separator : separators) {
            const int index = line.indexOf(separator);
            if (index > 0) {
                left = line.left(index).trimmed();
                right = line.mid(index + separator.size()).trimmed();
                break;
            }
        }
        if (left.isEmpty() || right.isEmpty()) {
            continue;
        }

        VocabularyEntry entry;
        entry.source = left;
        entry.target = right;
        entry.scopeId = QStringLiteral("__global");
        entry.matchMode = QStringLiteral("caseInsensitive");
        entry.enabled = true;
        entries.append(entry);
    }
    return entries;
}

QVector<VocabularyEntry> parseVocabularyImportData(
    const QByteArray &data,
    const QString &suffix,
    const QVector<VocabularyScopeOption> &scopeOptions
)
{
    const QString normalizedSuffix = suffix.trimmed().toLower();
    if (normalizedSuffix == QStringLiteral("json")) {
        return parseVocabularyJsonImport(data, scopeOptions);
    }

    QString text = QString::fromUtf8(data);
    if (text.startsWith(QChar(0xfeff))) {
        text.remove(0, 1);
    }
    if (normalizedSuffix == QStringLiteral("csv")) {
        return parseVocabularyCsvImport(text, scopeOptions);
    }
    return parseVocabularyTextImport(text);
}

QVector<VocabularyEntry> parseVocabularyImportFile(
    const QString &path,
    const QVector<VocabularyScopeOption> &scopeOptions
)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QVector<VocabularyEntry>();
    }
    return parseVocabularyImportData(
        file.readAll(),
        QFileInfo(path).suffix(),
        scopeOptions
    );
}

QString vocabularyCsvExportText(const QVector<VocabularyEntry> &entries)
{
    QStringList lines;
    const QStringList headerColumns = QStringList()
        << csvEscape(QStringLiteral("source"))
        << csvEscape(QStringLiteral("target"))
        << csvEscape(QStringLiteral("aliases"))
        << csvEscape(QStringLiteral("scopeId"))
        << csvEscape(QStringLiteral("matchMode"))
        << csvEscape(QStringLiteral("note"))
        << csvEscape(QStringLiteral("enabled"));
    lines << headerColumns.join(QStringLiteral(","));
    for (const VocabularyEntry &entry : entries) {
        const QStringList columns = QStringList()
            << csvEscape(entry.source)
            << csvEscape(entry.target)
            << csvEscape(entry.aliases)
            << csvEscape(normalizeVocabularyScope(entry.scopeId))
            << csvEscape(normalizeVocabularyMatchMode(entry.matchMode))
            << csvEscape(entry.note)
            << csvEscape(entry.enabled ? QStringLiteral("true") : QStringLiteral("false"));
        lines << columns.join(QStringLiteral(","));
    }
    return QString(QChar(0xfeff)) + lines.join(QStringLiteral("\n"));
}

QString vocabularyPlainExportText(
    const QVector<VocabularyEntry> &entries,
    const QVector<VocabularyScopeOption> &scopeOptions
)
{
    QStringList lines;
    for (const VocabularyEntry &entry : entries) {
        QString line = entry.source + QStringLiteral(" -> ") + entry.target;
        if (!entry.aliases.trimmed().isEmpty()) {
            line += tr8("；别名：") + entry.aliases.trimmed();
        }
        line += tr8("；范围：") + vocabularyScopeTitleForId(entry.scopeId, scopeOptions);
        line += tr8("；匹配：") + vocabularyMatchModeTitle(entry.matchMode);
        if (!entry.enabled) {
            line += tr8("；已关闭");
        }
        lines << line;
    }
    return lines.join(QStringLiteral("\n"));
}
