#include "ocr_batch_queue.h"

#include <QDir>
#include <QSet>

void OcrBatchQueue::replacePaths(const QStringList &paths)
{
    m_items.clear();
    m_currentIndex = -1;

    QSet<QString> seen;
    for (const QString &rawPath : paths) {
        const QString path = QDir::cleanPath(rawPath.trimmed());
        if (path.isEmpty()) {
            continue;
        }
        const QString key = QDir::fromNativeSeparators(path).toLower();
        if (seen.contains(key)) {
            continue;
        }
        seen.insert(key);
        OcrBatchItem item;
        item.path = path;
        m_items.append(item);
    }

    if (!m_items.isEmpty()) {
        m_currentIndex = 0;
    }
}

int OcrBatchQueue::count() const
{
    return m_items.size();
}

bool OcrBatchQueue::isEmpty() const
{
    return m_items.isEmpty();
}

int OcrBatchQueue::currentIndex() const
{
    return m_currentIndex;
}

bool OcrBatchQueue::setCurrentIndex(int index)
{
    if (index < 0 || index >= m_items.size()) {
        return false;
    }
    m_currentIndex = index;
    return true;
}

bool OcrBatchQueue::movePrevious()
{
    return setCurrentIndex(m_currentIndex - 1);
}

bool OcrBatchQueue::moveNext()
{
    return setCurrentIndex(m_currentIndex + 1);
}

const OcrBatchItem &OcrBatchQueue::item(int index) const
{
    return m_items.at(index);
}

OcrBatchItem &OcrBatchQueue::item(int index)
{
    return m_items[index];
}

const OcrBatchItem *OcrBatchQueue::currentItem() const
{
    return m_currentIndex >= 0 && m_currentIndex < m_items.size()
        ? &m_items.at(m_currentIndex)
        : nullptr;
}

OcrBatchItem *OcrBatchQueue::currentItem()
{
    return m_currentIndex >= 0 && m_currentIndex < m_items.size()
        ? &m_items[m_currentIndex]
        : nullptr;
}

bool OcrBatchQueue::setEditedText(int index, const QString &text)
{
    if (index < 0 || index >= m_items.size()) {
        return false;
    }
    m_items[index].text = text;
    return true;
}

bool OcrBatchQueue::markProcessing(int index)
{
    if (index < 0 || index >= m_items.size()) {
        return false;
    }
    OcrBatchItem &entry = m_items[index];
    entry.state = OcrBatchItemState::Processing;
    entry.errorCode.clear();
    entry.errorMessage.clear();
    return true;
}

bool OcrBatchQueue::markCancelled(int index)
{
    if (index < 0 || index >= m_items.size()) {
        return false;
    }
    m_items[index].state = OcrBatchItemState::Cancelled;
    return true;
}

bool OcrBatchQueue::applyResult(int index, const OcrResult &result)
{
    if (index < 0 || index >= m_items.size()) {
        return false;
    }
    OcrBatchItem &entry = m_items[index];
    entry.text = result.text;
    entry.errorCode = result.errorCode;
    entry.errorMessage = result.errorMessage;
    entry.engine = result.engine;
    entry.elapsedMs = result.elapsedMs;
    entry.usedFallback = result.usedFallback;
    entry.state = result.ok
        ? OcrBatchItemState::Completed
        : result.errorCode == QStringLiteral("CANCELLED")
            ? OcrBatchItemState::Cancelled
            : OcrBatchItemState::Failed;
    return true;
}

int OcrBatchQueue::nextPendingIndex(int startIndex) const
{
    if (m_items.isEmpty()) {
        return -1;
    }
    const int normalizedStart = qBound(0, startIndex, m_items.size() - 1);
    for (int offset = 0; offset < m_items.size(); ++offset) {
        const int index = (normalizedStart + offset) % m_items.size();
        if (m_items.at(index).state == OcrBatchItemState::Pending) {
            return index;
        }
    }
    return -1;
}
