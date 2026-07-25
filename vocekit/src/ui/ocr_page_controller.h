#ifndef VOCEKIT_OCR_PAGE_CONTROLLER_H
#define VOCEKIT_OCR_PAGE_CONTROLLER_H

#include "../config/app_settings_data.h"
#include "../domain/app_legacy_types.h"
#include "../ocr/ocr_batch_queue.h"
#include "../tasks/cancellation_token.h"

#include <QList>
#include <QObject>
#include <QPixmap>
#include <QString>

#include <functional>

class QLabel;
class QLineEdit;
class OcrManager;
class OcrPage;
class QPushButton;
class QComboBox;
class QTextEdit;
class QWidget;
template <typename T>
class QFutureWatcher;

// OCR 页面通过类型化设置快照读取配置，并通过回调通知历史记录变化。
struct OcrPageAccess
{
    std::function<AppSettingsData()> settingsSnapshotProvider;
    std::function<void(const QString &)> historyRecordSaved;
};

// Controls the OCR page state, batch queue, recognition callbacks and OCR text AI actions.
class OcrPageController : public QObject
{
public:
    explicit OcrPageController(
        const OcrPageAccess &access,
        QWidget *dialogParent,
        QObject *parent = nullptr
    );
    ~OcrPageController() override;

    QWidget *page();
    bool pageCreated() const;

    void refreshConfiguration();
    void refreshPage();

private:
    QPixmap loadPreview(const QString &path) const;
    void saveCurrentEditorText();
    void displayCurrentItem();
    void switchImage(int direction);
    void selectImage();
    void startRecognition();
    void startQueueItem(int index);
    void finishRecognition(const OcrResult &result);
    QString historyEngineName(OcrEngine engine) const;
    void saveHistory(const OcrResult &result, const QString &imagePath);
    void setAiButtonsEnabled(bool enabled);
    void runAiAction(const QString &action);
    AppSettingsData settingsSnapshot() const;

    OcrPageAccess m_access;
    QWidget *m_dialogParent = nullptr;
    OcrManager *m_ocrManager = nullptr;
    OcrPage *m_page = nullptr;
    QLineEdit *m_imagePathEdit = nullptr;
    QLabel *m_previewLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QComboBox *m_engineBox = nullptr;
    QTextEdit *m_resultEdit = nullptr;
    QPushButton *m_selectButton = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
    QPushButton *m_previousButton = nullptr;
    QPushButton *m_nextButton = nullptr;
    QLabel *m_positionLabel = nullptr;
    QList<QPushButton *> m_aiButtons;
    QFutureWatcher<OcrAiTaskResult> *m_aiWatcher = nullptr;
    CancellationSource m_aiCancellation;
    OcrBatchQueue m_batchQueue;
    bool m_batchRunning = false;
    int m_activeIndex = -1;
    bool m_updatingResultEdit = false;
};

#endif // VOCEKIT_OCR_PAGE_CONTROLLER_H
