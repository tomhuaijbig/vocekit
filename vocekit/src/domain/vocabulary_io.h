#ifndef VOCEKIT_VOCABULARY_IO_H
#define VOCEKIT_VOCABULARY_IO_H

#include "app_legacy_types.h"

#include <QByteArray>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

struct VocabularyScopeOption
{
    QString id;
    QString title;
};

QVector<VocabularyScopeOption> builtinVocabularyScopeOptions();
QString vocabularyScopeTitleForId(
    const QString &scopeId,
    const QVector<VocabularyScopeOption> &scopeOptions
);
QString normalizeVocabularyImportScope(
    const QString &value,
    const QVector<VocabularyScopeOption> &scopeOptions
);
QString normalizeVocabularyImportMatchMode(const QString &value);
bool vocabularyEntryMatchesSearch(
    const VocabularyEntry &entry,
    const QString &keyword,
    const QVector<VocabularyScopeOption> &scopeOptions
);
bool vocabularyEntryInScope(const VocabularyEntry &entry, const QString &scopeId);

QMap<QString, int> vocabularyCsvHeaderMap(const QStringList &columns);
VocabularyEntry vocabularyEntryFromCsvColumns(
    const QStringList &columns,
    const QMap<QString, int> &header,
    const QVector<VocabularyScopeOption> &scopeOptions
);
QVector<VocabularyEntry> parseVocabularyJsonImport(
    const QByteArray &data,
    const QVector<VocabularyScopeOption> &scopeOptions
);
QVector<VocabularyEntry> parseVocabularyCsvImport(
    const QString &text,
    const QVector<VocabularyScopeOption> &scopeOptions
);
QVector<VocabularyEntry> parseVocabularyTextImport(const QString &text);
QVector<VocabularyEntry> parseVocabularyImportData(
    const QByteArray &data,
    const QString &suffix,
    const QVector<VocabularyScopeOption> &scopeOptions
);
QVector<VocabularyEntry> parseVocabularyImportFile(
    const QString &path,
    const QVector<VocabularyScopeOption> &scopeOptions
);

QString vocabularyCsvExportText(const QVector<VocabularyEntry> &entries);
QString vocabularyPlainExportText(
    const QVector<VocabularyEntry> &entries,
    const QVector<VocabularyScopeOption> &scopeOptions
);

#endif // VOCEKIT_VOCABULARY_IO_H
