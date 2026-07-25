#ifndef VOCEKIT_VOCABULARY_STORE_H
#define VOCEKIT_VOCABULARY_STORE_H

#include "../domain/app_legacy_types.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

// VocabularyStore 负责词库文件读写、匹配、替换和模型提示词注入。
class VocabularyStore
{
public:
    explicit VocabularyStore(const QString &path = QString());

    QString path() const;
    QVector<VocabularyEntry> loadEntries() const;
    bool saveEntries(const QVector<VocabularyEntry> &entries) const;
    bool appendEntry(VocabularyEntry *entry, QString *error) const;

    QString applyEntries(const QString &text, const QString &modeId, bool enabled) const;
    QString promptBlock(
        const QString &modeId,
        bool enabled,
        const QString &contextText,
        int maxEntries
    ) const;

    static QString defaultPath();
    static QString normalizeScope(const QString &scope);
    static QString normalizeMatchMode(const QString &mode);
    static VocabularyEntry entryFromJsonObject(const QJsonObject &object);
    static QJsonObject entryToJsonObject(const VocabularyEntry &entry);
    static QString entryUniqueKey(const VocabularyEntry &entry);
    static QString csvEscape(QString value);
    static QStringList parseCsvLine(const QString &line);
    static QString nextEntryId(const QVector<VocabularyEntry> &entries);
    static QString matchModeTitle(const QString &mode);
    static QStringList terms(const VocabularyEntry &entry);
    static bool entryHasCorrection(const VocabularyEntry &entry);
    static bool entryAppliesToMode(const VocabularyEntry &entry, const QString &modeId);
    static bool entryRelevantToText(const VocabularyEntry &entry, const QString &contextText);

private:
    QString m_path;
};

QString vocabularyStorePath();
QString normalizeVocabularyScope(const QString &scope);
QString normalizeVocabularyMatchMode(const QString &mode);
VocabularyEntry vocabularyEntryFromJsonObject(const QJsonObject &object);
QJsonObject vocabularyEntryToJsonObject(const VocabularyEntry &entry);
QString vocabularyEntryUniqueKey(const VocabularyEntry &entry);
QString csvEscape(QString value);
QStringList parseCsvLine(const QString &line);
QVector<VocabularyEntry> loadVocabularyEntries();
bool saveVocabularyEntries(const QVector<VocabularyEntry> &entries);
bool appendVocabularyEntry(VocabularyEntry *entry, QString *error);
QString nextVocabularyEntryId(const QVector<VocabularyEntry> &entries);
QString vocabularyMatchModeTitle(const QString &mode);
QStringList vocabularyTerms(const VocabularyEntry &entry);
bool vocabularyEntryHasCorrection(const VocabularyEntry &entry);
bool vocabularyEntryAppliesToMode(const VocabularyEntry &entry, const QString &modeId);
QString applyVocabularyEntries(const QString &text, const QString &modeId, bool enabled);
QString vocabularyPromptBlock(
    const QString &modeId,
    bool enabled,
    const QString &contextText,
    int maxEntries
);

#endif // VOCEKIT_VOCABULARY_STORE_H
