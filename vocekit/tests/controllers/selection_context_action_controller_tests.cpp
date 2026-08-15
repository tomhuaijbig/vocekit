#include <QtTest>

#include "../../src/controllers/selection_context_action_controller.h"

#include <QAtomicInt>
#include <QMutex>
#include <QMutexLocker>
#include <QPointer>
#include <QThread>

ModelRequestTaskResult runModelProviderRequestTask(
    const ModelRequestTaskRequest &request,
    const ModelDeltaCallback &)
{
    ModelRequestTaskResult result;
    result.executionId = request.cancellation.executionId();
    result.errorMessage = QStringLiteral("default runner must not be used");
    return result;
}

namespace {

SelectedTextNativeWindowHandle windowHandle(quintptr value)
{
    return reinterpret_cast<SelectedTextNativeWindowHandle>(value);
}

QString text8(const char *value)
{
    return QString::fromUtf8(value);
}

FunctionSettings function(
    const QString &id,
    const QString &model,
    const QString &promptId)
{
    FunctionSettings value;
    value.id = id;
    value.name = id;
    value.modelId = model;
    value.promptId = promptId;
    value.builtIn = true;
    return value;
}

SelectionSnapshot snapshot(const QString &text)
{
    SelectionSnapshot value;
    value.text = text;
    value.targetWindow = windowHandle(19);
    value.targetProcessId = 91;
    value.targetExecutable = QStringLiteral("notepad.exe");
    value.anchorRect = QRect(10, 20, 80, 24);
    return value;
}

struct LogEntry
{
    QString eventId;
    QString actionId;
    int textLength = 0;
    qint64 elapsedMs = -1;
};

struct Harness
{
    AppSettingsData settings;
    PromptRuntimeSnapshot prompts;
    QVector<ModelOption> availableModels;
    int modelOptionsSnapshotCalls = 0;
    QAtomicInt providerCalls;
    QAtomicInt cancelledCalls;
    QAtomicInt releaseFirst;
    QMutex mutex;
    QVector<ModelRequestTaskRequest> requests;
    QStringList providerDeltas;
    QString providerText = QStringLiteral("provider answer");
    QString providerError;
    bool blockFirstProvider = false;
    bool consentAllowed = true;
    int consentCalls = 0;
    QStringList sequence;
    QStringList copied;
    QStringList saved;
    int closeToolbarCalls = 0;
    QVector<SelectionResultCardState> renders;
    QVector<LogEntry> logs;
    int validateCalls = 0;
    int replaceCalls = 0;
    QString replacedText;
    SelectedTextNativeWindowHandle replacedWindow = nullptr;
    ClipboardWriteResult replaceResult;
    quint64 validateGeneration = 0;
    std::function<void(quint64, bool)> validateCompletion;

    Harness()
    {
        providerCalls.storeRelease(0);
        cancelledCalls.storeRelease(0);
        releaseFirst.storeRelease(0);
        settings.targetLanguage = text8("简体中文");
        settings.functions
            << function(
                QStringLiteral("translate"),
                QStringLiteral("translate-model"),
                QStringLiteral("translate-test")
            )
            << function(
                QStringLiteral("ask"),
                QStringLiteral("ask-model"),
                QStringLiteral("ask-test")
            );
        PromptLibraryItem translatePrompt;
        translatePrompt.id = QStringLiteral("translate-test");
        translatePrompt.content = text8("翻译提示");
        PromptLibraryItem askPrompt;
        askPrompt.id = QStringLiteral("ask-test");
        askPrompt.content = text8("问答提示");
        prompts.libraryItems << translatePrompt << askPrompt;
        replaceResult.ok = true;
    }

    SelectionContextModelRunnerAccess runnerAccess()
    {
        SelectionContextModelRunnerAccess access;
        access.runRequest = [this](
            const ModelRequestTaskRequest &request,
            const ModelDeltaCallback &delta) {
            const int call = providerCalls.fetchAndAddOrdered(1);
            {
                QMutexLocker locker(&mutex);
                requests.append(request);
                sequence.append(QStringLiteral("provider"));
            }
            if (blockFirstProvider && call == 0) {
                while (!request.cancellation.isCancellationRequested()
                       && !releaseFirst.loadAcquire()) {
                    QThread::msleep(1);
                }
                if (request.cancellation.isCancellationRequested()) {
                    cancelledCalls.ref();
                }
                delta(QStringLiteral("late-first"));
            } else {
                const QStringList deltas = providerDeltas;
                for (const QString &value : deltas) {
                    delta(value);
                }
            }
            ModelRequestTaskResult result;
            result.executionId = request.cancellation.executionId();
            result.text = call == 0 || !blockFirstProvider
                ? providerText
                : QStringLiteral("second answer");
            result.errorMessage = providerError;
            return result;
        };
        return access;
    }

    SelectionContextActionAccess actionAccess()
    {
        SelectionContextActionAccess access;
        access.copyText = [this](const QString &value) {
            copied.append(value);
            return true;
        };
        access.saveVocabulary = [this](const QString &value) {
            saved.append(value);
        };
        access.validateSelectionAsync = [this](
            SelectedTextNativeWindowHandle,
            quint64 generation,
            const std::function<void(quint64, bool)> &completed) {
            ++validateCalls;
            validateGeneration = generation;
            validateCompletion = completed;
        };
        access.replaceSelection = [this](
            const QString &value,
            SelectedTextNativeWindowHandle window) {
            ++replaceCalls;
            replacedText = value;
            replacedWindow = window;
            return replaceResult;
        };
        access.settingsSnapshot = [this]() { return settings; };
        access.promptSnapshot = [this]() { return prompts; };
        access.modelOptionsSnapshot = [this]() {
            ++modelOptionsSnapshotCalls;
            return availableModels;
        };
        access.ensureNetworkConsent = [this](
            const QString &,
            const QString &) {
            ++consentCalls;
            QMutexLocker locker(&mutex);
            sequence.append(QStringLiteral("consent"));
            return consentAllowed;
        };
        access.renderResult = [this](const SelectionResultCardState &state) {
            renders.append(state);
        };
        access.closeToolbar = [this]() { ++closeToolbarCalls; };
        access.logMetadata = [this](
            const QString &eventId,
            const QString &actionId,
            int textLength,
            qint64 elapsedMs) {
            LogEntry entry;
            entry.eventId = eventId;
            entry.actionId = actionId;
            entry.textLength = textLength;
            entry.elapsedMs = elapsedMs;
            logs.append(entry);
        };
        return access;
    }

    ModelRequestTaskRequest requestAt(int index)
    {
        QMutexLocker locker(&mutex);
        return requests.at(index);
    }
};

} // namespace

class SelectionContextActionControllerTests : public QObject
{
    Q_OBJECT

private slots:
    void copyClosesTheToolbarAndShowsNoModelResult();
    void saveUsesTheGlobalVocabularyBridgeExactlyOnce();
    void saveAlwaysOpensTheLocalEditorAndNeverRequestsAiEvenWhenAiModeIsConfigured();
    void firstModelActionRequiresExplicitNetworkConsentBeforeProviderStart();
    void declinedNetworkConsentSendsNothingAndKeepsLocalActionsAvailable();
    void acknowledgedConsentDoesNotPromptAgain();
    void modelActionShowsDegradedBannerBeforeOfflineAiSearchDeltas();
    void modelOptionsAreSnapshottedExactlyOncePerRequest();
    void unavailableExplicitModelNeverStartsRunnerOrConsent();
    void aNewActionCancelsTheOldRunnerAndIgnoresLateCallbacks();
    void replaceRequiresOriginalWindowAndLiveSelection();
    void replaceRevalidationNeverCallsSetForegroundWindowOrChangesFocus();
    void pinnedCardNeverOffersReplace();
    void replaceUsesCheckedClipboardWriteAndKeepsResultOnFailure();
    void followUpCarriesOriginalSelectionAndPreviousAnswer();
    void regenerateStartsANewExecutionWithTheSameSnapshotAndAction();
    void textOverTwelveThousandCharactersWaitsForExplicitFullTextConsent();
    void longTextCancelMakesNoProviderCallAndNeverTruncates();
    void closeAndOutsideClickCancelOnlyAnUnpinnedRunningRequest();
    void logsContainActionLengthStateAndDurationButNeverText();
    void everyInjectedCallbackMayDestroyTheControllerSynchronously();
};

void SelectionContextActionControllerTests::
copyClosesTheToolbarAndShowsNoModelResult()
{
    Harness h;
    SelectionContextModelRunner runner(h.runnerAccess());
    SelectionContextActionController controller(&runner, h.actionAccess());
    controller.setSelection(snapshot(QStringLiteral("copy me")));
    controller.triggerAction(QStringLiteral("copy"));
    QCOMPARE(h.copied, QStringList() << QStringLiteral("copy me"));
    QCOMPARE(h.closeToolbarCalls, 1);
    QCOMPARE(h.renders.size(), 0);
    QCOMPARE(h.providerCalls.loadAcquire(), 0);
    QCOMPARE(h.consentCalls, 0);
}

void SelectionContextActionControllerTests::
saveUsesTheGlobalVocabularyBridgeExactlyOnce()
{
    Harness h;
    SelectionContextModelRunner runner(h.runnerAccess());
    SelectionContextActionController controller(&runner, h.actionAccess());
    controller.setSelection(snapshot(QStringLiteral("save me")));
    controller.triggerAction(QStringLiteral("save"));
    QCOMPARE(h.saved, QStringList() << QStringLiteral("save me"));
    QCOMPARE(h.providerCalls.loadAcquire(), 0);
    QCOMPARE(h.consentCalls, 0);
}

void SelectionContextActionControllerTests::
saveAlwaysOpensTheLocalEditorAndNeverRequestsAiEvenWhenAiModeIsConfigured()
{
    Harness h;
    h.settings.vocabularyAddMode = QStringLiteral("ai");
    SelectionContextModelRunner runner(h.runnerAccess());
    SelectionContextActionController controller(&runner, h.actionAccess());
    controller.setSelection(snapshot(QStringLiteral("local only")));
    controller.triggerAction(QStringLiteral("save"));
    QCOMPARE(h.saved.size(), 1);
    QCOMPARE(h.consentCalls, 0);
    QCOMPARE(h.providerCalls.loadAcquire(), 0);
}

void SelectionContextActionControllerTests::
firstModelActionRequiresExplicitNetworkConsentBeforeProviderStart()
{
    Harness h;
    SelectionContextModelRunner runner(h.runnerAccess());
    SelectionContextActionController controller(&runner, h.actionAccess());
    controller.setSelection(snapshot(QStringLiteral("explain me")));
    controller.triggerAction(QStringLiteral("explain"));
    QTRY_COMPARE(h.providerCalls.loadAcquire(), 1);
    QMutexLocker locker(&h.mutex);
    QCOMPARE(h.sequence.size(), 2);
    QCOMPARE(h.sequence.at(0), QStringLiteral("consent"));
    QCOMPARE(h.sequence.at(1), QStringLiteral("provider"));
}

void SelectionContextActionControllerTests::
declinedNetworkConsentSendsNothingAndKeepsLocalActionsAvailable()
{
    Harness h;
    h.consentAllowed = false;
    SelectionContextModelRunner runner(h.runnerAccess());
    SelectionContextActionController controller(&runner, h.actionAccess());
    controller.setSelection(snapshot(QStringLiteral("private")));
    controller.triggerAction(QStringLiteral("explain"));
    QCOMPARE(h.consentCalls, 1);
    QCOMPARE(h.providerCalls.loadAcquire(), 0);
    QVERIFY(!h.renders.isEmpty());
    QVERIFY(h.renders.constLast().statusText.contains(text8("取消")));
    controller.triggerAction(QStringLiteral("copy"));
    QCOMPARE(h.copied, QStringList() << QStringLiteral("private"));
}

void SelectionContextActionControllerTests::
acknowledgedConsentDoesNotPromptAgain()
{
    Harness h;
    h.settings.selectionContext.networkConsentAcknowledged = true;
    SelectionContextModelRunner runner(h.runnerAccess());
    SelectionContextActionController controller(&runner, h.actionAccess());
    controller.setSelection(snapshot(QStringLiteral("known")));
    controller.triggerAction(QStringLiteral("translate"));
    QTRY_COMPARE(h.providerCalls.loadAcquire(), 1);
    QCOMPARE(h.consentCalls, 0);
}

void SelectionContextActionControllerTests::
modelActionShowsDegradedBannerBeforeOfflineAiSearchDeltas()
{
    Harness h;
    h.providerDeltas << QStringLiteral("part");
    SelectionContextModelRunner runner(h.runnerAccess());
    SelectionContextActionController controller(&runner, h.actionAccess());
    controller.setSelection(snapshot(QStringLiteral("subject")));
    controller.triggerAction(QStringLiteral("ai-search"));
    QVERIFY(!h.renders.isEmpty());
    QCOMPARE(
        h.renders.constFirst().statusText,
        text8("未进行联网搜索，已使用普通 AI 解答")
    );
    QTRY_VERIFY(h.renders.constLast().committedText
                == QStringLiteral("provider answer"));
}

void SelectionContextActionControllerTests::
modelOptionsAreSnapshottedExactlyOncePerRequest()
{
    Harness h;
    h.settings.selectionContext.networkConsentAcknowledged = true;
    SelectionContextActionCustomization explain =
        h.settings.selectionContext.actionCustomizations.value(
            QStringLiteral("explain")
        );
    explain.modelId = QStringLiteral("openai:gpt-5.6-sol");
    explain.promptOverride = text8("分三层解释");
    h.settings.selectionContext.actionCustomizations.insert(
        QStringLiteral("explain"),
        explain
    );
    h.availableModels << ModelOption{
        QStringLiteral("openai:gpt-5.6-sol"),
        QStringLiteral("GPT-5.6 Sol"),
        QStringLiteral("OpenAI")
    };
    SelectionContextModelRunner runner(h.runnerAccess());
    SelectionContextActionController controller(&runner, h.actionAccess());
    controller.setSelection(snapshot(QStringLiteral("subject")));

    controller.triggerAction(QStringLiteral("explain"));

    QTRY_COMPARE(h.providerCalls.loadAcquire(), 1);
    QCOMPARE(h.modelOptionsSnapshotCalls, 1);
    QCOMPARE(
        h.requestAt(0).modelId,
        QStringLiteral("openai:gpt-5.6-sol")
    );
    QVERIFY(h.requestAt(0).userPrompt.contains(text8("分三层解释")));
}

void SelectionContextActionControllerTests::
unavailableExplicitModelNeverStartsRunnerOrConsent()
{
    Harness h;
    SelectionContextActionCustomization explain =
        h.settings.selectionContext.actionCustomizations.value(
            QStringLiteral("explain")
        );
    explain.modelId = QStringLiteral("custom:removed-model");
    explain.promptOverride = text8("私密指令不能进入状态消息");
    h.settings.selectionContext.actionCustomizations.insert(
        QStringLiteral("explain"),
        explain
    );
    h.availableModels << ModelOption{
        QStringLiteral("openai:gpt-5.6-sol"),
        QStringLiteral("GPT-5.6 Sol"),
        QStringLiteral("OpenAI")
    };
    SelectionContextModelRunner runner(h.runnerAccess());
    SelectionContextActionController controller(&runner, h.actionAccess());
    controller.setSelection(snapshot(QStringLiteral("private selection")));

    controller.triggerAction(QStringLiteral("explain"));

    QCOMPARE(h.modelOptionsSnapshotCalls, 1);
    QCOMPARE(h.providerCalls.loadAcquire(), 0);
    QCOMPARE(h.consentCalls, 0);
    QVERIFY(!h.renders.isEmpty());
    QVERIFY(h.renders.constLast().statusText.contains(text8("模型")));
    QVERIFY(!h.renders.constLast().statusText.contains(explain.promptOverride));
}

void SelectionContextActionControllerTests::
aNewActionCancelsTheOldRunnerAndIgnoresLateCallbacks()
{
    Harness h;
    h.blockFirstProvider = true;
    SelectionContextModelRunner runner(h.runnerAccess());
    SelectionContextActionController controller(&runner, h.actionAccess());
    controller.setSelection(snapshot(QStringLiteral("subject")));
    controller.triggerAction(QStringLiteral("explain"));
    QTRY_COMPARE(h.providerCalls.loadAcquire(), 1);
    controller.triggerAction(QStringLiteral("translate"));
    QTRY_COMPARE(h.cancelledCalls.loadAcquire(), 1);
    QTRY_COMPARE(h.providerCalls.loadAcquire(), 2);
    QTRY_VERIFY(!h.renders.isEmpty()
                && h.renders.constLast().committedText
                    == QStringLiteral("second answer"));
    for (const SelectionResultCardState &state : h.renders) {
        QVERIFY(!state.committedText.contains(QStringLiteral("late-first")));
        QVERIFY(!state.provisionalText.contains(QStringLiteral("late-first")));
    }
}

void SelectionContextActionControllerTests::
replaceRequiresOriginalWindowAndLiveSelection()
{
    Harness h;
    h.settings.selectionContext.networkConsentAcknowledged = true;
    SelectionContextModelRunner runner(h.runnerAccess());
    SelectionContextActionController controller(&runner, h.actionAccess());
    controller.setSelection(snapshot(QStringLiteral("original")));
    controller.triggerAction(QStringLiteral("explain"));
    QTRY_VERIFY(!h.renders.isEmpty()
                && !h.renders.constLast().committedText.isEmpty());
    controller.replaceResult();
    QCOMPARE(h.validateCalls, 1);
    QCOMPARE(h.replaceCalls, 0);
    h.validateCompletion(h.validateGeneration, true);
    QCOMPARE(h.replaceCalls, 1);
    QCOMPARE(h.replacedText, QStringLiteral("provider answer"));
    QCOMPARE(h.replacedWindow, windowHandle(19));
}

void SelectionContextActionControllerTests::
replaceRevalidationNeverCallsSetForegroundWindowOrChangesFocus()
{
    Harness h;
    h.settings.selectionContext.networkConsentAcknowledged = true;
    SelectionContextModelRunner runner(h.runnerAccess());
    SelectionContextActionController controller(&runner, h.actionAccess());
    controller.setSelection(snapshot(QStringLiteral("original")));
    controller.triggerAction(QStringLiteral("explain"));
    QTRY_VERIFY(!h.renders.isEmpty()
                && !h.renders.constLast().committedText.isEmpty());
    controller.replaceResult();
    QCOMPARE(h.validateCalls, 1);
    QCOMPARE(h.replaceCalls, 0);
    h.validateCompletion(h.validateGeneration, false);
    QCOMPARE(h.replaceCalls, 0);
    QVERIFY(h.renders.constLast().statusText.contains(text8("选区")));
}

void SelectionContextActionControllerTests::pinnedCardNeverOffersReplace()
{
    Harness h;
    h.settings.selectionContext.networkConsentAcknowledged = true;
    SelectionContextModelRunner runner(h.runnerAccess());
    SelectionContextActionController controller(&runner, h.actionAccess());
    controller.setSelection(snapshot(QStringLiteral("original")));
    controller.triggerAction(QStringLiteral("explain"));
    QTRY_VERIFY(!h.renders.isEmpty()
                && !h.renders.constLast().committedText.isEmpty());
    controller.setPinned(true);
    QVERIFY(!h.renders.constLast().replaceEnabled);
    controller.replaceResult();
    QCOMPARE(h.validateCalls, 0);
}

void SelectionContextActionControllerTests::
replaceUsesCheckedClipboardWriteAndKeepsResultOnFailure()
{
    Harness h;
    h.settings.selectionContext.networkConsentAcknowledged = true;
    h.replaceResult.ok = false;
    h.replaceResult.errorCode = QStringLiteral("flow_input_injection_failed");
    SelectionContextModelRunner runner(h.runnerAccess());
    SelectionContextActionController controller(&runner, h.actionAccess());
    controller.setSelection(snapshot(QStringLiteral("original")));
    controller.triggerAction(QStringLiteral("explain"));
    QTRY_VERIFY(!h.renders.isEmpty()
                && !h.renders.constLast().committedText.isEmpty());
    controller.replaceResult();
    h.validateCompletion(h.validateGeneration, true);
    QCOMPARE(h.replaceCalls, 1);
    QCOMPARE(h.renders.constLast().committedText,
             QStringLiteral("provider answer"));
    QVERIFY(h.renders.constLast().statusText.contains(text8("失败")));
}

void SelectionContextActionControllerTests::
followUpCarriesOriginalSelectionAndPreviousAnswer()
{
    Harness h;
    h.settings.selectionContext.networkConsentAcknowledged = true;
    SelectionContextModelRunner runner(h.runnerAccess());
    SelectionContextActionController controller(&runner, h.actionAccess());
    controller.setSelection(snapshot(QStringLiteral("original selection")));
    controller.triggerAction(QStringLiteral("explain"));
    QTRY_COMPARE(h.providerCalls.loadAcquire(), 1);
    QTRY_VERIFY(!h.renders.isEmpty()
                && h.renders.constLast().committedText
                    == QStringLiteral("provider answer"));
    controller.submitFollowUp(QStringLiteral("next question"));
    QTRY_COMPARE(h.providerCalls.loadAcquire(), 2);
    const QString prompt = h.requestAt(1).userPrompt;
    QVERIFY(prompt.contains(QStringLiteral("original selection")));
    QVERIFY(prompt.contains(QStringLiteral("provider answer")));
    QVERIFY(prompt.contains(QStringLiteral("next question")));
}

void SelectionContextActionControllerTests::
regenerateStartsANewExecutionWithTheSameSnapshotAndAction()
{
    Harness h;
    h.settings.selectionContext.networkConsentAcknowledged = true;
    SelectionContextModelRunner runner(h.runnerAccess());
    SelectionContextActionController controller(&runner, h.actionAccess());
    controller.setSelection(snapshot(QStringLiteral("same selection")));
    controller.triggerAction(QStringLiteral("translate"));
    QTRY_COMPARE(h.providerCalls.loadAcquire(), 1);
    controller.regenerate();
    QTRY_COMPARE(h.providerCalls.loadAcquire(), 2);
    QCOMPARE(h.requestAt(0).userPrompt, h.requestAt(1).userPrompt);
    QCOMPARE(h.requestAt(0).modelId, h.requestAt(1).modelId);
}

void SelectionContextActionControllerTests::
textOverTwelveThousandCharactersWaitsForExplicitFullTextConsent()
{
    Harness h;
    const QString longText(12001, QLatin1Char('x'));
    SelectionContextModelRunner runner(h.runnerAccess());
    SelectionContextActionController controller(&runner, h.actionAccess());
    controller.setSelection(snapshot(longText));
    controller.triggerAction(QStringLiteral("explain"));
    QCOMPARE(h.providerCalls.loadAcquire(), 0);
    QCOMPARE(h.consentCalls, 0);
    QVERIFY(h.renders.constLast().requiresLongTextConfirmation);
    controller.processFullTextConfirmed();
    QTRY_COMPARE(h.providerCalls.loadAcquire(), 1);
    QVERIFY(h.requestAt(0).userPrompt.contains(longText));
}

void SelectionContextActionControllerTests::
longTextCancelMakesNoProviderCallAndNeverTruncates()
{
    Harness h;
    const QString longText(12001, QLatin1Char('z'));
    SelectionContextModelRunner runner(h.runnerAccess());
    SelectionContextActionController controller(&runner, h.actionAccess());
    controller.setSelection(snapshot(longText));
    controller.triggerAction(QStringLiteral("translate"));
    controller.cancel();
    QCOMPARE(h.providerCalls.loadAcquire(), 0);
    QCOMPARE(h.consentCalls, 0);
    QVERIFY(!h.renders.constLast().requiresLongTextConfirmation);
}

void SelectionContextActionControllerTests::
closeAndOutsideClickCancelOnlyAnUnpinnedRunningRequest()
{
    Harness h;
    h.blockFirstProvider = true;
    h.settings.selectionContext.networkConsentAcknowledged = true;
    SelectionContextModelRunner runner(h.runnerAccess());
    SelectionContextActionController controller(&runner, h.actionAccess());
    controller.setSelection(snapshot(QStringLiteral("subject")));
    controller.triggerAction(QStringLiteral("explain"));
    QTRY_COMPARE(h.providerCalls.loadAcquire(), 1);
    controller.setPinned(true);
    controller.close();
    QCOMPARE(h.cancelledCalls.loadAcquire(), 0);
    controller.setPinned(false);
    controller.close();
    QTRY_COMPARE(h.cancelledCalls.loadAcquire(), 1);
}

void SelectionContextActionControllerTests::
logsContainActionLengthStateAndDurationButNeverText()
{
    Harness h;
    const QString privateText = QStringLiteral("never-log-this-body");
    h.settings.selectionContext.networkConsentAcknowledged = true;
    SelectionContextModelRunner runner(h.runnerAccess());
    SelectionContextActionController controller(&runner, h.actionAccess());
    controller.setSelection(snapshot(privateText));
    controller.triggerAction(QStringLiteral("explain"));
    QTRY_VERIFY(h.logs.size() >= 2);
    for (const LogEntry &entry : h.logs) {
        QCOMPARE(entry.actionId, QStringLiteral("explain"));
        QCOMPARE(entry.textLength, privateText.size());
        QVERIFY(!entry.eventId.contains(privateText));
        QVERIFY(entry.elapsedMs >= -1);
    }
}

void SelectionContextActionControllerTests::
everyInjectedCallbackMayDestroyTheControllerSynchronously()
{
    {
        Harness h;
        SelectionContextModelRunner runner(h.runnerAccess());
        SelectionContextActionController *controller = nullptr;
        SelectionContextActionAccess access = h.actionAccess();
        access.copyText = [&controller](const QString &) {
            SelectionContextActionController *doomed = controller;
            controller = nullptr;
            delete doomed;
            return true;
        };
        controller = new SelectionContextActionController(&runner, access);
        QPointer<SelectionContextActionController> guard(controller);
        controller->setSelection(snapshot(QStringLiteral("delete")));
        controller->triggerAction(QStringLiteral("copy"));
        QVERIFY(guard.isNull());
    }

    {
        Harness h;
        SelectionContextModelRunner runner(h.runnerAccess());
        SelectionContextActionController *controller = nullptr;
        SelectionContextActionAccess access = h.actionAccess();
        access.saveVocabulary = [&controller](const QString &) {
            SelectionContextActionController *doomed = controller;
            controller = nullptr;
            delete doomed;
        };
        controller = new SelectionContextActionController(&runner, access);
        QPointer<SelectionContextActionController> guard(controller);
        controller->setSelection(snapshot(QStringLiteral("delete")));
        controller->triggerAction(QStringLiteral("save"));
        QVERIFY(guard.isNull());
    }

    {
        Harness h;
        SelectionContextModelRunner runner(h.runnerAccess());
        SelectionContextActionController *controller = nullptr;
        SelectionContextActionAccess access = h.actionAccess();
        access.settingsSnapshot = [&controller]() {
            SelectionContextActionController *doomed = controller;
            controller = nullptr;
            delete doomed;
            return AppSettingsData();
        };
        controller = new SelectionContextActionController(&runner, access);
        QPointer<SelectionContextActionController> guard(controller);
        controller->setSelection(snapshot(QStringLiteral("delete")));
        controller->triggerAction(QStringLiteral("explain"));
        QVERIFY(guard.isNull());
    }

    {
        Harness h;
        SelectionContextModelRunner runner(h.runnerAccess());
        SelectionContextActionController *controller = nullptr;
        SelectionContextActionAccess access = h.actionAccess();
        access.modelOptionsSnapshot = [&controller]() {
            SelectionContextActionController *doomed = controller;
            controller = nullptr;
            delete doomed;
            return QVector<ModelOption>();
        };
        controller = new SelectionContextActionController(&runner, access);
        QPointer<SelectionContextActionController> guard(controller);
        controller->setSelection(snapshot(QStringLiteral("delete")));
        controller->triggerAction(QStringLiteral("explain"));
        QVERIFY(guard.isNull());
    }

    {
        Harness h;
        SelectionContextModelRunner runner(h.runnerAccess());
        SelectionContextActionController *controller = nullptr;
        SelectionContextActionAccess access = h.actionAccess();
        access.promptSnapshot = [&controller]() {
            SelectionContextActionController *doomed = controller;
            controller = nullptr;
            delete doomed;
            return PromptRuntimeSnapshot();
        };
        controller = new SelectionContextActionController(&runner, access);
        QPointer<SelectionContextActionController> guard(controller);
        controller->setSelection(snapshot(QStringLiteral("delete")));
        controller->triggerAction(QStringLiteral("explain"));
        QVERIFY(guard.isNull());
    }

    {
        Harness h;
        SelectionContextModelRunner runner(h.runnerAccess());
        SelectionContextActionController *controller = nullptr;
        SelectionContextActionAccess access = h.actionAccess();
        access.ensureNetworkConsent = [&controller](
            const QString &,
            const QString &) {
            SelectionContextActionController *doomed = controller;
            controller = nullptr;
            delete doomed;
            return false;
        };
        controller = new SelectionContextActionController(&runner, access);
        QPointer<SelectionContextActionController> guard(controller);
        controller->setSelection(snapshot(QStringLiteral("delete")));
        controller->triggerAction(QStringLiteral("explain"));
        QVERIFY(guard.isNull());
    }

    {
        Harness h;
        SelectionContextModelRunner runner(h.runnerAccess());
        SelectionContextActionController *controller = nullptr;
        SelectionContextActionAccess access = h.actionAccess();
        access.renderResult = [&controller](
            const SelectionResultCardState &) {
            SelectionContextActionController *doomed = controller;
            controller = nullptr;
            delete doomed;
        };
        controller = new SelectionContextActionController(&runner, access);
        QPointer<SelectionContextActionController> guard(controller);
        controller->setSelection(
            snapshot(QString(12001, QLatin1Char('q')))
        );
        controller->triggerAction(QStringLiteral("explain"));
        QVERIFY(guard.isNull());
    }

    {
        Harness h;
        h.settings.selectionContext.networkConsentAcknowledged = true;
        SelectionContextModelRunner runner(h.runnerAccess());
        SelectionContextActionController *controller = nullptr;
        SelectionContextActionAccess access = h.actionAccess();
        access.closeToolbar = [&controller]() {
            SelectionContextActionController *doomed = controller;
            controller = nullptr;
            delete doomed;
        };
        controller = new SelectionContextActionController(&runner, access);
        QPointer<SelectionContextActionController> guard(controller);
        controller->setSelection(snapshot(QStringLiteral("delete")));
        controller->triggerAction(QStringLiteral("explain"));
        QVERIFY(guard.isNull());
    }

    {
        Harness h;
        h.settings.selectionContext.networkConsentAcknowledged = true;
        SelectionContextModelRunner runner(h.runnerAccess());
        SelectionContextActionController *controller = nullptr;
        SelectionContextActionAccess access = h.actionAccess();
        access.logMetadata = [&controller](
            const QString &,
            const QString &,
            int,
            qint64) {
            SelectionContextActionController *doomed = controller;
            controller = nullptr;
            delete doomed;
        };
        controller = new SelectionContextActionController(&runner, access);
        QPointer<SelectionContextActionController> guard(controller);
        controller->setSelection(snapshot(QStringLiteral("delete")));
        controller->triggerAction(QStringLiteral("explain"));
        QVERIFY(guard.isNull());
    }

    {
        Harness h;
        h.settings.selectionContext.networkConsentAcknowledged = true;
        SelectionContextModelRunner runner(h.runnerAccess());
        SelectionContextActionController *controller = nullptr;
        SelectionContextActionAccess access = h.actionAccess();
        access.validateSelectionAsync = [&controller](
            SelectedTextNativeWindowHandle,
            quint64,
            const std::function<void(quint64, bool)> &) {
            SelectionContextActionController *doomed = controller;
            controller = nullptr;
            delete doomed;
        };
        controller = new SelectionContextActionController(&runner, access);
        QPointer<SelectionContextActionController> guard(controller);
        controller->setSelection(snapshot(QStringLiteral("delete")));
        controller->triggerAction(QStringLiteral("explain"));
        QTRY_VERIFY(!h.renders.isEmpty()
                    && !h.renders.constLast().committedText.isEmpty());
        controller->replaceResult();
        QVERIFY(guard.isNull());
    }

    {
        Harness h;
        h.settings.selectionContext.networkConsentAcknowledged = true;
        SelectionContextModelRunner runner(h.runnerAccess());
        SelectionContextActionController *controller = nullptr;
        SelectionContextActionAccess access = h.actionAccess();
        access.validateSelectionAsync = [](
            SelectedTextNativeWindowHandle,
            quint64 generation,
            const std::function<void(quint64, bool)> &completed) {
            completed(generation, true);
        };
        access.replaceSelection = [&controller](
            const QString &,
            SelectedTextNativeWindowHandle) {
            SelectionContextActionController *doomed = controller;
            controller = nullptr;
            delete doomed;
            ClipboardWriteResult result;
            result.ok = true;
            return result;
        };
        controller = new SelectionContextActionController(&runner, access);
        QPointer<SelectionContextActionController> guard(controller);
        controller->setSelection(snapshot(QStringLiteral("delete")));
        controller->triggerAction(QStringLiteral("explain"));
        QTRY_VERIFY(!h.renders.isEmpty()
                    && !h.renders.constLast().committedText.isEmpty());
        controller->replaceResult();
        QVERIFY(guard.isNull());
    }
}

QTEST_MAIN(SelectionContextActionControllerTests)
#include "selection_context_action_controller_tests.moc"
