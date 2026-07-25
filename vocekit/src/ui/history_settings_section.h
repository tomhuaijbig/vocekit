#ifndef VOCEKIT_HISTORY_SETTINGS_SECTION_H
#define VOCEKIT_HISTORY_SETTINGS_SECTION_H

#include <QLabel>
#include <QString>
#include <QWidget>

#include <functional>

class QSpinBox;

struct HistorySettingsSnapshot
{
    QString recordDirectoryPath;
    int historyInitialLoadCount = 20;
    int historyLoadMoreCount = 20;
    int logInitialLoadCount = 50;
    int logLoadMoreCount = 50;
};

// Settings subpage for history and log storage options.
class HistorySettingsSection : public QWidget
{
public:
    struct Callbacks
    {
        std::function<HistorySettingsSnapshot()> snapshotProvider;
        std::function<void(const QString &)> setRecordDirectory;
        std::function<void()> resetRecordDirectory;
        std::function<void(int)> setHistoryInitialLoadCount;
        std::function<void(int)> setHistoryLoadMoreCount;
        std::function<void(int)> setLogInitialLoadCount;
        std::function<void(int)> setLogLoadMoreCount;
        std::function<void()> saveAndRefresh;
        std::function<void(const QString &, const QString &)> showDetail;
    };

    explicit HistorySettingsSection(
        const Callbacks &callbacks,
        QWidget *parent = nullptr
    );

    void refreshFromSettings();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QWidget *recordDirectoryRow();
    QWidget *historyLoadCountRow();
    QWidget *logLoadCountRow();
    void attachSettingDetail(QWidget *card, const QString &title, const QString &detail);
    void installSettingDetailTarget(QWidget *target, const QString &title, const QString &detail);
    void saveAndRefresh();
    HistorySettingsSnapshot snapshot() const;

    static QString recordDirectoryDetailText();
    static QString historyLoadCountDetailText();
    static QString logLoadCountDetailText();

    Callbacks m_callbacks;
    QLabel *m_recordDirectoryLabel = nullptr;
    QSpinBox *m_historyInitialLoadBox = nullptr;
    QSpinBox *m_historyLoadMoreBox = nullptr;
    QSpinBox *m_logInitialLoadBox = nullptr;
    QSpinBox *m_logLoadMoreBox = nullptr;
};

#endif // VOCEKIT_HISTORY_SETTINGS_SECTION_H
