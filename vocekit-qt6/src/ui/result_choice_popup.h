#ifndef VOCEKIT_RESULT_CHOICE_POPUP_H
#define VOCEKIT_RESULT_CHOICE_POPUP_H

#include "../output/clipboard_writer.h"

#include <QMap>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <functional>

class QCloseEvent;
class QHBoxLayout;
class QLabel;
class QPushButton;
class QTextEdit;

struct ResultPopupWindowPreferences
{
    int opacityPercent = 100;
    bool hasGeometry = false;
    QRect geometry;
};

// 大模型结果操作窗口。声明保持独立，具体界面和设置读写位于 cpp。
class ResultChoicePopup : public QWidget
{
public:
    ResultChoicePopup(
        const ResultPopupWindowPreferences &preferences,
        const QString &title,
        const QString &result,
        ClipboardWindowHandle targetWindow,
        bool hasSelection,
        int autoCloseMsec,
        QWidget *parent = nullptr
    );

    void setActionOrder(const QStringList &actionIds);
    void setOpacityPercent(int percent);
    void setResolvedCallback(
        const std::function<void(const QString &)> &onResolved
    );
    void setCheckedWriteCallback(
        const std::function<ClipboardWriteResult(
            const QString &,
            const QString &,
            ClipboardWindowHandle,
            bool
        )> &onWrite
    );
    void setActionCallbacks(
        const std::function<void()> &onRegenerate,
        const std::function<void(const QString &)> &onRetryModel,
        const std::function<void(const QString &)> &onFollowUp
    );
    void setCancellationCallback(
        const std::function<void()> &onCancellation
    );
    void setDraftCallback(
        const std::function<void(const QString &)> &onDraft
    );
    void setLiveDraftCallback(
        const std::function<void(const QString &)> &onLiveDraft
    );
    void setVocabularyCallback(
        const std::function<void(const QString &, const QString &)> &onVocabulary
    );
    void setWindowPreferenceCallback(
        const std::function<void(const QRect &)> &onChanged
    );
    void setCurrentModel(const QString &model);
    QString currentModel() const;
    void setHasSelection(bool hasSelection);
    void setResultText(const QString &result, bool resetDraftState = true);
    void appendResultText(const QString &text);
    QString resultText() const;
    void setBusy(bool busy, const QString &hint = QString());
    void setAutoCloseMsec(int autoCloseMsec);
    void showNearBottom();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    QPushButton *popupButton(const QString &text, bool primary);
    void updateActionState();
    void chooseModelAndRetry();
    void askFollowUp();
    void showExpandedResult();
    void syncResultFromEditor();
    bool shouldSaveDraft() const;
    void saveDraftIfNeeded();
    void saveGeometryPreference();
    void resolveResult(const QString &action);
    void scheduleAutoClose();

    ResultPopupWindowPreferences m_preferences;
    QString m_result;
    QString m_initialResult;
    ClipboardWindowHandle m_targetWindow = nullptr;
    bool m_hasSelection = false;
    int m_autoCloseMsec = 0;
    bool m_busy = false;
    bool m_programmaticTextChange = false;
    bool m_userEdited = false;
    bool m_draftSaved = false;
    QString m_currentModel;
    QTextEdit *m_editor = nullptr;
    QLabel *m_hint = nullptr;
    QPushButton *m_copyButton = nullptr;
    QPushButton *m_writeButton = nullptr;
    QPushButton *m_replaceButton = nullptr;
    QPushButton *m_regenerateButton = nullptr;
    QPushButton *m_retryModelButton = nullptr;
    QPushButton *m_followUpButton = nullptr;
    QPushButton *m_expandButton = nullptr;
    QPushButton *m_vocabularyButton = nullptr;
    QPushButton *m_closeButton = nullptr;
    QHBoxLayout *m_advancedLayout = nullptr;
    QHBoxLayout *m_resultLayout = nullptr;
    QMap<QString, QPushButton *> m_actionButtons;
    bool m_resolved = false;
    quint64 m_autoCloseGeneration = 0;
    std::function<void()> m_onRegenerate;
    std::function<void(const QString &)> m_onRetryModel;
    std::function<void(const QString &)> m_onFollowUp;
    std::function<void()> m_onCancellation;
    std::function<void(const QString &)> m_onDraft;
    std::function<void(const QString &)> m_onLiveDraft;
    std::function<void(const QString &, const QString &)> m_onVocabulary;
    std::function<void(const QRect &)> m_onWindowPreferenceChanged;
    std::function<void(const QString &)> m_onResolved;
    std::function<ClipboardWriteResult(
        const QString &,
        const QString &,
        ClipboardWindowHandle,
        bool
    )> m_onCheckedWrite;
};

#endif // VOCEKIT_RESULT_CHOICE_POPUP_H
