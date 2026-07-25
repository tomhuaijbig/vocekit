#ifndef VOCEKIT_OCR_BATCH_QUEUE_H
#define VOCEKIT_OCR_BATCH_QUEUE_H

#include "ocr_types.h"

#include <QString>
#include <QStringList>
#include <QVector>

enum class OcrBatchItemState {
    Pending,
    Processing,
    Completed,
    Failed,
    Cancelled
};

struct OcrBatchItem
{
    QString path;
    QString text;
    QString errorCode;
    QString errorMessage;
    OcrBatchItemState state = OcrBatchItemState::Pending;
    OcrEngine engine = OcrEngine::Automatic;
    qint64 elapsedMs = -1;
    bool usedFallback = false;
};

class OcrBatchQueue
{
public:
    void replacePaths(const QStringList &paths);

    int count() const;
    bool isEmpty() const;
    int currentIndex() const;
    bool setCurrentIndex(int index);
    bool movePrevious();
    bool moveNext();

    const OcrBatchItem &item(int index) const;
    OcrBatchItem &item(int index);
    const OcrBatchItem *currentItem() const;
    OcrBatchItem *currentItem();

    bool setEditedText(int index, const QString &text);
    bool markProcessing(int index);
    bool markCancelled(int index);
    bool applyResult(int index, const OcrResult &result);
    int nextPendingIndex(int startIndex) const;

private:
    QVector<OcrBatchItem> m_items;
    int m_currentIndex = -1;
};

#endif // VOCEKIT_OCR_BATCH_QUEUE_H
