#ifndef VOCEKIT_HISTORY_RECORD_SERVICE_H
#define VOCEKIT_HISTORY_RECORD_SERVICE_H

#include "history_favorites.h"
#include "history_store.h"

#include "../domain/execution_types.h"
#include "../domain/history_record_builder.h"

#include <QString>
#include <QStringList>

struct HistoryRecordSaveRequest
{
    QString recordDirectory;
    QString modeId;
    QString modeTitle;
    QString sourceAudioPath;
    bool draft = false;
    HistoryRecordMetadataRequest metadata;
};

// HistoryRecordService 负责把一次功能执行结果转换为历史文件。
// 调用方只提供业务数据；详情 JSON、目录结构、索引追加交给这里和 HistoryStore。
struct HistoryDeleteResult
{
    bool ok = false;
    int requestedCount = 0;
    int removedCount = 0;
    QStringList failedPaths;
};

class HistoryRecordService
{
public:
    explicit HistoryRecordService(const QString &recordDirectory);

    static HistoryAppendResult save(const HistoryRecordSaveRequest &request);
    HistoryAppendResult saveOcr(
        const QString &modeTitle,
        const OcrPageHistoryMetadataRequest &metadata
    ) const;
    HistoryFavoriteUpdateResult updateFavorite(
        const QString &filePath,
        bool favorite,
        const QString &folder = QString()
    ) const;
    HistoryDeleteResult removeEntry(const HistoryEntry &entry) const;
    HistoryDeleteResult removeEntries(const QStringList &filePaths) const;
    bool updateRetriedSegment(
        HistoryEntry *entry,
        const HistorySegmentRetryResult &result
    ) const;
    bool updateFlowEditedText(
        const ExecutionId &runId,
        const QString &detailPath,
        const QString &editedText,
        OperationError *error
    ) const;
    void rebuildIndex() const;

private:
    bool isManagedDetailPath(const QString &filePath) const;

    HistoryStore m_store;
};

#endif // VOCEKIT_HISTORY_RECORD_SERVICE_H
