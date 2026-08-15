#ifndef VOCEKIT_SELECTION_CONTEXT_ACTION_CONTROLLER_H
#define VOCEKIT_SELECTION_CONTEXT_ACTION_CONTROLLER_H

#include "../config/app_settings_data.h"
#include "../domain/prompt_runtime_library.h"
#include "../input/selection_snapshot.h"
#include "../output/clipboard_writer.h"
#include "../tasks/selection_context_model_request.h"
#include "../tasks/selection_context_model_runner.h"
#include "../ui/selection_result_card.h"

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>

#include <functional>

struct SelectionContextActionAccess
{
    std::function<bool(const QString &)> copyText;
    std::function<void(const QString &, const QString &)>
        openVocabularyEditor;
    std::function<void(
        SelectedTextNativeWindowHandle window,
        quint64 generation,
        const std::function<void(quint64, bool)> &completed
    )> validateSelectionAsync;
    std::function<ClipboardWriteResult(
        const QString &,
        SelectedTextNativeWindowHandle
    )> replaceSelection;
    std::function<AppSettingsData()> settingsSnapshot;
    std::function<PromptRuntimeSnapshot()> promptSnapshot;
    std::function<QVector<ModelOption>()> modelOptionsSnapshot;
    std::function<bool(
        const QString &actionId,
        const QString &modelId
    )> ensureNetworkConsent;
    std::function<void(const SelectionResultCardState &)> renderResult;
    std::function<void()> closeToolbar;
    std::function<void(
        const QString &eventId,
        const QString &actionId,
        int textLength,
        qint64 elapsedMs
    )> logMetadata;
};

class SelectionContextActionController : public QObject
{
    Q_OBJECT

public:
    explicit SelectionContextActionController(
        SelectionContextModelRunner *runner,
        const SelectionContextActionAccess &access,
        QObject *parent = nullptr
    );
    ~SelectionContextActionController() override;

    void setSelection(const SelectionSnapshot &snapshot);
    void triggerAction(const QString &actionId);
    void processFullTextConfirmed();
    void regenerate();
    void submitFollowUp(const QString &question);
    void copyResult();
    void replaceResult();
    void setPinned(bool pinned);
    void cancel();
    void close();

private:
    void startModelAction();
    void cancelActiveWithoutRendering();
    void handleDelta(
        quint64 generation,
        const ExecutionId &executionId,
        const QString &delta
    );
    void handleFinished(
        quint64 generation,
        const ExecutionId &executionId,
        const ModelRequestTaskResult &result
    );
    SelectionResultCardState currentState() const;
    bool renderCurrent();
    bool closeToolbarSafely();
    bool logSafely(const QString &eventId, qint64 elapsedMs);
    void reportCopyFailure(const AppSettingsData &settings);
    void setStatus(const QString &status);

    QPointer<SelectionContextModelRunner> m_runner;
    SelectionContextActionAccess m_access;
    SelectionSnapshot m_snapshot;
    QString m_actionId;
    QString m_title;
    QString m_committedText;
    QString m_provisionalText;
    QString m_statusText;
    QString m_previousAnswer;
    QString m_followUpQuestion;
    QString m_degradedMessage;
    ExecutionId m_executionId;
    QElapsedTimer m_elapsed;
    quint64 m_generation = 0;
    bool m_running = false;
    bool m_pinned = false;
    bool m_fullTextConfirmed = false;
    bool m_requiresLongTextConfirmation = false;
    bool m_replacePending = false;
};

#endif // VOCEKIT_SELECTION_CONTEXT_ACTION_CONTROLLER_H
