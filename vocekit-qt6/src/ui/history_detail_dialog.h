#ifndef VOCEKIT_HISTORY_DETAIL_DIALOG_H
#define VOCEKIT_HISTORY_DETAIL_DIALOG_H

#include "../domain/history_types.h"
#include "app_dialogs.h"

#include <QSharedPointer>
#include <QString>

#include <functional>

class QPushButton;
class QVBoxLayout;
class HistoryDetailDialog : public AppDialog
{
public:
    struct Callbacks
    {
        std::function<QString(qint64)> elapsedText;
        std::function<QString(const HistoryEntry &)> modelText;
        std::function<QString(const HistoryEntry &)> recognizedText;
        std::function<QString(const HistoryEntry &)> detailPlainText;
        std::function<QString()> speechProvider;
        std::function<bool()> useSystemProxy;
        std::function<QString(const QString &)> speechNetworkPolicy;
        std::function<bool(HistoryEntry *, const HistorySegmentRetryResult &)> updateRetriedSegment;
        std::function<void()> historyChanged;
    };

    HistoryDetailDialog(
        const HistoryEntry &entry,
        const Callbacks &callbacks,
        QWidget *parent = nullptr
    );

private:
    void buildUi();
    void addSegmentsCard(QVBoxLayout *contentLayout);
    void retrySegment(const RecordingSegment &segment, QPushButton *retryButton);
    void copyDetailText() const;
    void copyOutputText() const;
    void openAudioFile() const;

    QString elapsedText(qint64 elapsedMs) const;
    QString modelText() const;
    QString recognizedText() const;
    QString detailPlainText() const;
    QString speechProvider() const;
    QString speechNetworkPolicy() const;
    bool useSystemProxy() const;

    QSharedPointer<HistoryEntry> m_entry;
    Callbacks m_callbacks;
};

#endif // VOCEKIT_HISTORY_DETAIL_DIALOG_H
