#ifndef VOCEKIT_HISTORY_STORE_H
#define VOCEKIT_HISTORY_STORE_H

#include "../domain/history_types.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

struct HistoryAppendRequest
{
    QString modeId;
    QString modeTitle;
    QString sourceAudioPath;
    bool draft = false;
    QJsonObject item;
};

struct HistoryAppendResult
{
    QJsonObject item;
    QString modeTextPath;
    QString modeDetailPath;
    QString allAudioPath;
    QString allTextPath;
    QString allDetailPath;
    bool ok = false;
};

// HistoryStore 负责历史记录目录、详情 JSON、索引和可读文本的纯文件读写。
// UI 展示、弹窗、播放器和导出选择逻辑仍留在页面层，后续再继续拆分。
class HistoryStore
{
public:
    explicit HistoryStore(const QString &recordDirectory);

    QString recordDirectory() const { return m_recordDirectory; }
    QString rootPath() const;
    QString backupDirectory() const;
    QString allDateDirectory(const QString &folderName, const QString &date) const;
    QString modeDirectory(const QString &modeTitle) const;
    QString modeDateDirectory(const QString &modeTitle, const QString &date) const;
    QString modeDateSubDirectory(
        const QString &modeTitle,
        const QString &date,
        const QString &subFolder
    ) const;
    QString indexPath() const;

    void ensureRootStructure() const;
    void ensureModeDateStructure(const QString &modeTitle, const QString &date) const;

    QVector<HistoryEntry> loadEntries() const;
    QVector<HistoryEntry> scanEntries() const;
    bool readIndex(QVector<HistoryEntry> *entries) const;
    static HistoryEntry entryFromFile(const QString &filePath);
    static QStringList relatedFilesForEntry(const HistoryEntry &entry);

    static QString safeFileNamePart(
        const QString &text,
        const QString &fallback = QStringLiteral("history")
    );
    static QString backupFolderName();
    static QString allAudioFolderName();
    static QString allTextFolderName();
    static QString allDetailFolderName();
    static QString textSubFolderName();
    static QString audioSubFolderName();
    static QString detailSubFolderName();
    static bool isDateFolderName(const QString &name);
    static bool isReservedRootFolder(const QString &name);
    static QString normalizedPathForCompare(const QString &path);
    static bool pathIsSameOrInside(const QString &candidate, const QString &parent);
    static HistoryEntry entryFromJsonObject(const QJsonObject &item, const QString &filePath);
    static QJsonObject entryToIndexObject(const HistoryEntry &entry);
    static QString textFromJsonObject(const QJsonObject &item);
    static void sortEntries(QVector<HistoryEntry> *entries);

private:
    friend class HistoryRecordService;
#ifdef VOCEKIT_HISTORY_STORE_TEST_ACCESS
    friend class HistoryStoreTests;
#endif

    bool writeIndex(const QVector<HistoryEntry> &entries) const;
    void rebuildIndex() const;
    void appendIndexEntry(const HistoryEntry &entry) const;
    HistoryAppendResult appendRecord(const HistoryAppendRequest &request) const;
    bool updateRetriedSegment(
        HistoryEntry *entry,
        const HistorySegmentRetryResult &result
    ) const;
    static bool removeEntryFiles(const HistoryEntry &entry);
    bool rootHasDetailJsonFiles() const;

    QString m_recordDirectory;
};

#endif // VOCEKIT_HISTORY_STORE_H
