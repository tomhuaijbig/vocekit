#ifndef VOCEKIT_SCREENSHOT_RESULT_WINDOW_H
#define VOCEKIT_SCREENSHOT_RESULT_WINDOW_H

#include "../ocr/ocr_types.h"
#include "../output/clipboard_writer.h"

#include <QImage>
#include <QPair>
#include <QRect>
#include <QString>
#include <QVector>
#include <QWidget>

#include <functional>

class QLabel;
class QSlider;
class QSplitter;
class QTabBar;
class QTextEdit;
class QPushButton;
class ScreenshotImageCanvas;

class ScreenshotResultWindow : public QWidget
{
public:
    ScreenshotResultWindow(
        const QString &title,
        const QImage &image,
        const QVector<OcrTextBlock> &blocks,
        const QString &recognizedText,
        const QString &resultText,
        const QRect &savedGeometry,
        int opacityPercent,
        int autoCloseMsec,
        QWidget *parent = nullptr
    );

    void setCurrentModel(const QString &modelId);
    void setModelOptions(
        const QVector<QPair<QString, QString>> &modelOptions
    );
    void setResultText(const QString &text, bool resetDraftState = true);
    void appendResultText(const QString &text);
    QString resultText() const;
    void setBusy(bool busy, const QString &status = QString());
    void showNearBottom();

    void setActionCallbacks(
        const std::function<void()> &onRegenerate,
        const std::function<void(const QString &)> &onRetryModel,
        const std::function<void(const QString &)> &onFollowUp,
        const std::function<void(const QString &)> &onWrite,
        const std::function<void(const QString &)> &onReplace
    );
    void setCheckedWriteCallback(
        const std::function<ClipboardWriteResult(
            const QString &action,
            const QString &text
        )> &callback
    );
    void setDraftCallback(const std::function<void(const QString &)> &onDraft);
    void setLiveDraftCallback(
        const std::function<void(const QString &)> &onLiveDraft
    );
    void setResolvedCallback(const std::function<void()> &onResolved);
    void setWindowPreferenceCallback(
        const std::function<void(const QRect &, int)> &onChanged
    );

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    QPushButton *actionButton(const QString &text, bool primary);
    void applyViewMode(int index);
    void updateOverlay();
    void updateActionState();
    void chooseModelAndRetry();
    void askFollowUp();
    void saveDraftIfNeeded();
    void saveWindowPreference();

    QImage m_image;
    QVector<OcrTextBlock> m_blocks;
    QString m_recognizedText;
    QString m_result;
    QString m_initialResult;
    QRect m_savedGeometry;
    int m_opacityPercent = 92;
    int m_autoCloseMsec = 0;
    bool m_busy = false;
    bool m_programmaticChange = false;
    bool m_userEdited = false;
    bool m_draftSaved = false;
    QString m_currentModel;
    QVector<QPair<QString, QString>> m_modelOptions;

    QTabBar *m_viewTabs = nullptr;
    ScreenshotImageCanvas *m_canvas = nullptr;
    QSplitter *m_splitter = nullptr;
    QTextEdit *m_editor = nullptr;
    QLabel *m_statusLabel = nullptr;
    QSlider *m_opacitySlider = nullptr;
    QPushButton *m_copyButton = nullptr;
    QPushButton *m_writeButton = nullptr;
    QPushButton *m_replaceButton = nullptr;
    QPushButton *m_regenerateButton = nullptr;
    QPushButton *m_retryModelButton = nullptr;
    QPushButton *m_followUpButton = nullptr;

    std::function<void()> m_onRegenerate;
    std::function<void(const QString &)> m_onRetryModel;
    std::function<void(const QString &)> m_onFollowUp;
    std::function<void(const QString &)> m_onWrite;
    std::function<void(const QString &)> m_onReplace;
    std::function<ClipboardWriteResult(
        const QString &,
        const QString &
    )> m_checkedWrite;
    std::function<void(const QString &)> m_onDraft;
    std::function<void(const QString &)> m_onLiveDraft;
    std::function<void()> m_onResolved;
    std::function<void(const QRect &, int)> m_onWindowPreferenceChanged;
};

#endif // VOCEKIT_SCREENSHOT_RESULT_WINDOW_H
