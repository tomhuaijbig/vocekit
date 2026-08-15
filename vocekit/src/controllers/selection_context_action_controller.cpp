#include "selection_context_action_controller.h"

#include "../domain/selection_context_actions.h"

#include <QPointer>

namespace {

const int kLongTextConfirmationThreshold = 12000;

QString text8(const char *value)
{
    return QString::fromUtf8(value);
}

QString actionTitle(
    const QString &actionId,
    const AppSettingsData &settings)
{
    const QString standard = selectionContextActionTitle(actionId);
    if (!standard.isEmpty()) {
        return standard;
    }
    const QString functionId = selectionContextFunctionId(actionId);
    const FunctionSettings &function = settings.function(functionId);
    return function.name.trimmed().isEmpty()
        ? text8("自定义功能")
        : function.name.trimmed();
}

} // namespace

SelectionContextActionController::SelectionContextActionController(
    SelectionContextModelRunner *runner,
    const SelectionContextActionAccess &access,
    QObject *parent)
    : QObject(parent),
      m_runner(runner),
      m_access(access)
{
}

SelectionContextActionController::~SelectionContextActionController()
{
    ++m_generation;
    m_running = false;
    m_replacePending = false;
    if (m_runner) {
        m_runner->cancel();
    }
}

void SelectionContextActionController::setSelection(
    const SelectionSnapshot &snapshot)
{
    cancelActiveWithoutRendering();
    m_snapshot = snapshot;
    m_actionId.clear();
    m_title.clear();
    m_committedText.clear();
    m_provisionalText.clear();
    m_statusText.clear();
    m_previousAnswer.clear();
    m_followUpQuestion.clear();
    m_degradedMessage.clear();
    m_pinned = false;
    m_fullTextConfirmed = false;
    m_requiresLongTextConfirmation = false;
}

void SelectionContextActionController::triggerAction(
    const QString &actionId)
{
    const QString id = actionId.trimmed();
    if (id == selectionContextActionCopy()) {
        cancelActiveWithoutRendering();
        m_actionId = id;
        if (m_snapshot.text.isEmpty() || !m_access.copyText) {
            return;
        }
        const std::function<bool(const QString &)> copy = m_access.copyText;
        QPointer<SelectionContextActionController> guard(this);
        copy(m_snapshot.text);
        if (!guard) {
            return;
        }
        logSafely(QStringLiteral("selection.action.copy"), 0);
        if (!guard) {
            return;
        }
        closeToolbarSafely();
        return;
    }
    if (id == selectionContextActionSave()) {
        cancelActiveWithoutRendering();
        m_actionId = id;
        if (m_snapshot.text.isEmpty() || !m_access.saveVocabulary) {
            return;
        }
        const std::function<void(const QString &)> save =
            m_access.saveVocabulary;
        QPointer<SelectionContextActionController> guard(this);
        save(m_snapshot.text);
        if (!guard) {
            return;
        }
        logSafely(QStringLiteral("selection.action.save"), 0);
        if (!guard) {
            return;
        }
        closeToolbarSafely();
        return;
    }

    cancelActiveWithoutRendering();
    m_actionId = id;
    m_title = selectionContextActionTitle(id);
    if (m_title.isEmpty()) {
        m_title = text8("自定义功能");
    }
    m_committedText.clear();
    m_provisionalText.clear();
    m_statusText.clear();
    m_previousAnswer.clear();
    m_followUpQuestion.clear();
    m_degradedMessage.clear();
    m_fullTextConfirmed = false;
    m_requiresLongTextConfirmation = false;
    startModelAction();
}

void SelectionContextActionController::processFullTextConfirmed()
{
    if (!m_requiresLongTextConfirmation || m_actionId.isEmpty()) {
        return;
    }
    m_fullTextConfirmed = true;
    m_requiresLongTextConfirmation = false;
    startModelAction();
}

void SelectionContextActionController::regenerate()
{
    if (m_actionId.isEmpty()) {
        return;
    }
    m_previousAnswer.clear();
    m_followUpQuestion.clear();
    startModelAction();
}

void SelectionContextActionController::submitFollowUp(
    const QString &question)
{
    const QString value = question.trimmed();
    if (value.isEmpty()
        || m_actionId.isEmpty()
        || m_committedText.isEmpty()) {
        return;
    }
    m_previousAnswer = m_committedText;
    m_followUpQuestion = value;
    startModelAction();
}

void SelectionContextActionController::copyResult()
{
    if (m_committedText.isEmpty() || !m_access.copyText) {
        return;
    }
    const std::function<bool(const QString &)> copy = m_access.copyText;
    QPointer<SelectionContextActionController> guard(this);
    const bool copied = copy(m_committedText);
    if (!guard) {
        return;
    }
    setStatus(copied ? text8("已复制结果") : text8("复制失败"));
}

void SelectionContextActionController::replaceResult()
{
    if (m_pinned || m_committedText.isEmpty()) {
        return;
    }
    if (!m_snapshot.targetWindow || !m_access.validateSelectionAsync) {
        setStatus(text8("原选区已不可用，无法替换。"));
        return;
    }

    if (m_runner) {
        m_runner->cancel();
    }
    m_running = false;
    m_replacePending = true;
    const quint64 generation = ++m_generation;
    const SelectedTextNativeWindowHandle window = m_snapshot.targetWindow;
    const std::function<void(
        SelectedTextNativeWindowHandle,
        quint64,
        const std::function<void(quint64, bool)> &)> validate =
            m_access.validateSelectionAsync;
    const QPointer<SelectionContextActionController> guard(this);
    const std::function<void(quint64, bool)> completed =
        [guard, generation, window](quint64 deliveredGeneration, bool valid) {
            if (!guard
                || deliveredGeneration != generation
                || guard->m_generation != generation
                || !guard->m_replacePending) {
                return;
            }
            guard->m_replacePending = false;
            if (!valid) {
                guard->setStatus(text8("原选区已变化，未执行替换。"));
                return;
            }
            if (!guard->m_access.replaceSelection) {
                guard->setStatus(text8("替换失败：写入功能不可用。"));
                return;
            }
            const QString resultText = guard->m_committedText;
            const std::function<ClipboardWriteResult(
                const QString &,
                SelectedTextNativeWindowHandle)> replace =
                    guard->m_access.replaceSelection;
            QPointer<SelectionContextActionController> replaceGuard(guard);
            const ClipboardWriteResult result = replace(resultText, window);
            if (!replaceGuard) {
                return;
            }
            replaceGuard->setStatus(
                result.ok
                    ? text8("已替换原选区")
                    : text8("替换失败，结果仍保留在此处。")
            );
        };
    validate(window, generation, completed);
    if (!guard) {
        return;
    }
}

void SelectionContextActionController::setPinned(bool pinned)
{
    m_pinned = pinned;
    renderCurrent();
}

void SelectionContextActionController::cancel()
{
    const bool hadWork = m_running
        || m_requiresLongTextConfirmation
        || m_replacePending;
    cancelActiveWithoutRendering();
    m_requiresLongTextConfirmation = false;
    if (hadWork) {
        m_statusText = text8("已取消");
        QPointer<SelectionContextActionController> guard(this);
        if (!renderCurrent() || !guard) {
            return;
        }
        logSafely(QStringLiteral("selection.action.cancelled"), -1);
    }
}

void SelectionContextActionController::close()
{
    if (m_pinned) {
        return;
    }
    const QPointer<SelectionContextActionController> guard(this);
    cancel();
    if (!guard) {
        return;
    }
    closeToolbarSafely();
}

void SelectionContextActionController::startModelAction()
{
    if (m_actionId.isEmpty() || m_snapshot.text.isEmpty()) {
        return;
    }
    cancelActiveWithoutRendering();

    if (m_snapshot.text.size() > kLongTextConfirmationThreshold
        && !m_fullTextConfirmed) {
        m_requiresLongTextConfirmation = true;
        m_running = false;
        m_statusText = text8("文字较长，请确认是否发送完整原文。");
        renderCurrent();
        return;
    }
    m_requiresLongTextConfirmation = false;

    AppSettingsData settings;
    QPointer<SelectionContextActionController> guard(this);
    if (m_access.settingsSnapshot) {
        const std::function<AppSettingsData()> settingsSnapshot =
            m_access.settingsSnapshot;
        settings = settingsSnapshot();
        if (!guard) {
            return;
        }
    }
    PromptRuntimeSnapshot prompts;
    if (m_access.promptSnapshot) {
        const std::function<PromptRuntimeSnapshot()> promptSnapshot =
            m_access.promptSnapshot;
        prompts = promptSnapshot();
        if (!guard) {
            return;
        }
    }
    QVector<ModelOption> availableModels;
    if (m_access.modelOptionsSnapshot) {
        const std::function<QVector<ModelOption>()> modelOptionsSnapshot =
            m_access.modelOptionsSnapshot;
        availableModels = modelOptionsSnapshot();
        if (!guard) {
            return;
        }
    }
    m_title = actionTitle(m_actionId, settings);

    SelectionContextModelRequestInput input;
    input.actionId = m_actionId;
    input.selectedText = m_snapshot.text;
    input.previousAnswer = m_previousAnswer;
    input.followUpQuestion = m_followUpQuestion;
    input.settings = settings;
    input.prompts = prompts;
    input.modelOptions = availableModels;
    const SelectionContextModelRequest built =
        buildSelectionContextModelRequest(input);
    if (!built.valid) {
        m_running = false;
        m_statusText = built.errorMessage;
        renderCurrent();
        return;
    }

    if (!settings.selectionContext.networkConsentAcknowledged) {
        if (!m_access.ensureNetworkConsent) {
            m_statusText = text8("未获得数据传输授权。");
            renderCurrent();
            return;
        }
        const std::function<bool(const QString &, const QString &)> consent =
            m_access.ensureNetworkConsent;
        const bool accepted = consent(
            m_actionId,
            built.modelRequest.modelId
        );
        if (!guard) {
            return;
        }
        if (!accepted) {
            m_running = false;
            m_statusText = text8("已取消数据传输，未发送选中文字。");
            renderCurrent();
            return;
        }
    }

    m_degradedMessage = built.degradedMessage;
    m_committedText.clear();
    m_provisionalText.clear();
    m_statusText = m_degradedMessage;
    m_running = true;
    const quint64 generation = ++m_generation;
    m_elapsed.restart();
    if (!closeToolbarSafely() || !guard) {
        return;
    }
    if (!logSafely(QStringLiteral("selection.action.started"), 0)
        || !guard) {
        return;
    }
    if (!renderCurrent() || !guard) {
        return;
    }
    if (!m_runner) {
        m_running = false;
        m_statusText = text8("模型运行器不可用。");
        renderCurrent();
        return;
    }

    SelectionContextModelRunnerCallbacks callbacks;
    callbacks.delta = [guard, generation](
        const ExecutionId &executionId,
        const QString &delta) {
        if (guard) {
            guard->handleDelta(generation, executionId, delta);
        }
    };
    callbacks.finished = [guard, generation](
        const ExecutionId &executionId,
        const ModelRequestTaskResult &result) {
        if (guard) {
            guard->handleFinished(generation, executionId, result);
        }
    };
    m_executionId = m_runner->start(built.modelRequest, callbacks);
    if (!m_executionId.isValid()) {
        m_running = false;
        m_statusText = text8("模型任务未能启动。");
        renderCurrent();
    }
}

void SelectionContextActionController::cancelActiveWithoutRendering()
{
    if (m_runner && m_runner->isRunning()) {
        m_runner->cancel();
    }
    ++m_generation;
    m_running = false;
    m_replacePending = false;
    m_executionId = ExecutionId();
}

void SelectionContextActionController::handleDelta(
    quint64 generation,
    const ExecutionId &executionId,
    const QString &delta)
{
    if (!m_running
        || generation != m_generation
        || executionId != m_executionId
        || delta.isEmpty()) {
        return;
    }
    m_provisionalText += delta;
    renderCurrent();
}

void SelectionContextActionController::handleFinished(
    quint64 generation,
    const ExecutionId &executionId,
    const ModelRequestTaskResult &result)
{
    if (!m_running
        || generation != m_generation
        || executionId != m_executionId) {
        return;
    }
    m_running = false;
    m_executionId = ExecutionId();
    if (!result.errorMessage.isEmpty()) {
        m_statusText = result.errorMessage;
    } else {
        m_committedText = result.text.isEmpty()
            ? m_provisionalText
            : result.text;
        m_provisionalText.clear();
        m_statusText = m_degradedMessage;
    }
    const qint64 elapsedMs = m_elapsed.isValid() ? m_elapsed.elapsed() : -1;
    const QPointer<SelectionContextActionController> guard(this);
    if (!renderCurrent() || !guard) {
        return;
    }
    logSafely(
        result.errorMessage.isEmpty()
            ? QStringLiteral("selection.action.completed")
            : QStringLiteral("selection.action.failed"),
        elapsedMs
    );
}

SelectionResultCardState
SelectionContextActionController::currentState() const
{
    SelectionResultCardState state;
    state.actionId = m_actionId;
    state.title = m_title;
    state.committedText = m_committedText;
    state.provisionalText = m_provisionalText;
    state.statusText = m_statusText;
    state.running = m_running;
    state.pinned = m_pinned;
    state.replaceEnabled = !m_running
        && !m_pinned
        && !m_committedText.isEmpty()
        && m_snapshot.targetWindow;
    state.requiresLongTextConfirmation =
        m_requiresLongTextConfirmation;
    return state;
}

bool SelectionContextActionController::renderCurrent()
{
    if (!m_access.renderResult) {
        return true;
    }
    const std::function<void(const SelectionResultCardState &)> render =
        m_access.renderResult;
    const SelectionResultCardState state = currentState();
    const QPointer<SelectionContextActionController> guard(this);
    render(state);
    return bool(guard);
}

bool SelectionContextActionController::closeToolbarSafely()
{
    if (!m_access.closeToolbar) {
        return true;
    }
    const std::function<void()> closeToolbar = m_access.closeToolbar;
    const QPointer<SelectionContextActionController> guard(this);
    closeToolbar();
    return bool(guard);
}

bool SelectionContextActionController::logSafely(
    const QString &eventId,
    qint64 elapsedMs)
{
    if (!m_access.logMetadata) {
        return true;
    }
    const std::function<void(
        const QString &,
        const QString &,
        int,
        qint64)> log = m_access.logMetadata;
    const QString actionId = m_actionId;
    const int textLength = m_snapshot.text.size();
    const QPointer<SelectionContextActionController> guard(this);
    log(eventId, actionId, textLength, elapsedMs);
    return bool(guard);
}

void SelectionContextActionController::setStatus(const QString &status)
{
    m_statusText = status;
    renderCurrent();
}
