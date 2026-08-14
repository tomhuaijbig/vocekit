#include <QtTest>

#include "../../src/app/selection_context_feature.h"
#include "../../src/domain/selection_context_actions.h"
#include "../../src/ui/selection_context_toolbar.h"
#include "../../src/ui/selection_result_card.h"

#include <QAction>
#include <QFile>
#include <QMenu>
#include <QPushButton>
#include <QToolButton>

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

SelectionSnapshot usableSnapshot(
    const QString &text = QStringLiteral("selected text"),
    const QString &executable = QStringLiteral("editor.exe"))
{
    SelectionSnapshot snapshot;
    snapshot.text = text;
    snapshot.targetWindow = reinterpret_cast<void *>(quintptr(0x1234));
    snapshot.targetProcessId = 42;
    snapshot.targetExecutable = executable;
    snapshot.anchorRect = QRect(80, 80, 120, 24);
    snapshot.cursorPosition = QPoint(100, 100);
    snapshot.method = SelectionAcquisitionMethod::UiAutomation;
    return snapshot;
}

QAction *menuAction(QMenu *menu, const QString &id)
{
    if (!menu) {
        return nullptr;
    }
    for (QAction *action : menu->actions()) {
        if (action && action->data().toString() == id) {
            return action;
        }
    }
    return nullptr;
}

class Harness
{
public:
    Harness()
    {
        settings.selectionContext.enabled = true;
        settings.selectionContext.networkConsentAcknowledged = true;
        FunctionSettings ask;
        ask.id = QStringLiteral("ask");
        ask.name = QStringLiteral("问答");
        ask.builtIn = true;
        ask.modelId = QStringLiteral("openai:gpt-5.6-terra");
        ask.prompt = QStringLiteral("基于文字回答问题");
        settings.functions.append(ask);

        access.settingsSnapshot = [this]() { return settings; };
        access.promptSnapshot = []() { return PromptRuntimeSnapshot(); };
        access.saveVocabulary = [this](const QString &text) {
            vocabulary.append(text);
            vocabularySawPausedObserver = observerPaused;
        };
        access.blockApplication = [this](const QString &executable) {
            blockedAttempts.append(executable);
            if (!persistBlock) {
                return false;
            }
            settings.selectionContext.blockedApplications.append(
                executable.toLower()
            );
            settings.selectionContext.blockedApplications.removeDuplicates();
            return true;
        };
        access.openSettings = [this]() {
            ++openSettingsCount;
            settingsSawPausedObserver = observerPaused;
        };
        access.ensureNetworkConsent = [this](const QString &, const QString &) {
            ++consentCount;
            consentSawPausedObserver = observerPaused;
            return consentAccepted;
        };
        access.logMetadata = [this](
            const QString &eventId,
            const QString &,
            int,
            qint64) {
            events.append(eventId);
        };

        dependencies.installObserver = [this](
            SelectedTextNativeWindowHandle,
            QString *error) {
            ++installCount;
            if (!installSucceeds) {
                if (error) {
                    *error = QStringLiteral("install.failed");
                }
                return false;
            }
            installed = true;
            return true;
        };
        dependencies.uninstallObserver = [this]() {
            ++uninstallCount;
            installed = false;
        };
        dependencies.setObservationCallback = [this](
            const std::function<void(const SelectionObservation &)> &callback) {
            observation = callback;
        };
        dependencies.setObserverPaused = [this](bool value) {
            observerPaused = value;
        };
        dependencies.setObserverSurfaceVisible = [this](bool value) {
            observerSurfaceVisible = value;
        };
        dependencies.startProbe = [this](
            const SelectionProbeRequest &,
            bool,
            quint64 generation,
            const SelectionProbeRunnerCallbacks &callbacks) {
            ++probeStarts;
            if (callbacks.completed) {
                callbacks.completed(generation, nextSnapshot);
            }
        };
        dependencies.cancelProbe = [this]() { ++probeCancels; };
        dependencies.validateSelectionAsync = [](
            SelectedTextNativeWindowHandle,
            quint64 generation,
            const std::function<void(quint64, bool)> &completed) {
            completed(generation, true);
        };
        dependencies.currentProcessId = []() { return quint32(100); };
        dependencies.targetWindowValid = [](
            SelectedTextNativeWindowHandle window) { return window != nullptr; };
        dependencies.currentForegroundWindow = []() {
            return reinterpret_cast<void *>(quintptr(0x1234));
        };
        dependencies.availableGeometry = [](const QPoint &) {
            return QRect(0, 0, 1280, 800);
        };
        dependencies.copyText = [this](const QString &text) {
            copied.append(text);
            return true;
        };
        dependencies.replaceSelection = [this](
            const QString &text,
            SelectedTextNativeWindowHandle) {
            replaced.append(text);
            ClipboardWriteResult result;
            result.ok = true;
            return result;
        };
        dependencies.modelRunnerAccess.runRequest = [this](
            const ModelRequestTaskRequest &,
            const ModelDeltaCallback &delta) {
            ++modelRuns;
            if (delta) {
                delta(QStringLiteral("partial"));
            }
            ModelRequestTaskResult result;
            result.text = QStringLiteral("answer");
            return result;
        };

        nextSnapshot = usableSnapshot();
    }

    SelectionContextFeature *create(QObject *parent = nullptr)
    {
        return new SelectionContextFeature(access, dependencies, parent);
    }

    void showSelection(SelectionContextFeature *feature)
    {
        feature->triggerFallbackShortcut();
        QCoreApplication::processEvents();
    }

    SelectionContextToolbar *toolbar(SelectionContextFeature *feature)
    {
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            SelectionContextToolbar *bar =
                qobject_cast<SelectionContextToolbar *>(widget);
            if (bar
                && bar->property("selectionContextFeatureOwner")
                    .toULongLong() == quintptr(feature)) {
                return bar;
            }
        }
        return nullptr;
    }

    SelectionResultCard *visibleCard(SelectionContextFeature *feature)
    {
        SelectionResultCard *pinnedFallback = nullptr;
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            SelectionResultCard *card =
                qobject_cast<SelectionResultCard *>(widget);
            if (card
                && card->property("selectionContextFeatureOwner")
                    .toULongLong() == quintptr(feature)
                && card->isVisible()) {
                if (!card->state().pinned) {
                    return card;
                }
                pinnedFallback = card;
            }
        }
        return pinnedFallback;
    }

    void triggerModelAction(SelectionContextFeature *feature)
    {
        SelectionContextToolbar *bar = toolbar(feature);
        QVERIFY(bar);
        QToolButton *button = bar->findChild<QToolButton *>(
            QStringLiteral("selectionActionAiSearchButton")
        );
        QVERIFY(button);
        button->click();
        QTRY_VERIFY(visibleCard(feature));
    }

    AppSettingsData settings;
    SelectionSnapshot nextSnapshot;
    SelectionContextFeatureAccess access;
    SelectionContextFeatureDependencies dependencies;
    std::function<void(const SelectionObservation &)> observation;
    QStringList vocabulary;
    QStringList copied;
    QStringList replaced;
    QStringList blockedAttempts;
    QStringList events;
    bool installSucceeds = true;
    bool installed = false;
    bool observerPaused = false;
    bool observerSurfaceVisible = false;
    bool persistBlock = true;
    bool consentAccepted = true;
    bool vocabularySawPausedObserver = false;
    bool settingsSawPausedObserver = false;
    bool consentSawPausedObserver = false;
    int installCount = 0;
    int uninstallCount = 0;
    int probeStarts = 0;
    int probeCancels = 0;
    int modelRuns = 0;
    int consentCount = 0;
    int openSettingsCount = 0;
};

} // namespace

class SelectionContextFeatureTests : public QObject
{
    Q_OBJECT

private slots:
    void startAndStopOwnExactlyOneObserverAndRunner();
    void disabledAutomaticModeInstallsHooksOnlyForAnActiveFallbackSession();
    void observerInstallFailureStillAllowsTheFallbackHotkeyAndCloseButton();
    void lockOrSuspendCancelsProbesAndSessionsAndUnlockRestartsOnlyWhenEnabled();
    void refreshAppliesSettingsWithoutRecreatingVisiblePinnedResult();
    void pinDetachesCurrentCardAndNextActionUsesANewCardUpToThreePinned();
    void moreMenuListsEachCustomFunctionOnceAndRoutesStableIds();
    void builtInAndCanvasFunctionsDoNotAppearInMoreMenu();
    void blockCurrentApplicationPersistsAndFailedPersistenceLeavesSurface();
    void openSettingsAndVocabularyInteractionsSuppressNativeCandidates();
    void consentPromptSuppressesObserverAndDeclineSendsNothing();
    void replaceRevalidatesTheOriginalWindowSelection();
    void runtimeCompositionRoutesOnlyToolbarHotkeyAndUsesLocalVocabulary();
    void runtimeCompositionRefreshesAndStopsFeatureInOrder();
    void closingAResultReleasesTheCoordinatorSession();
    void teardownStopsHooksAndCancelsWork();
};

void SelectionContextFeatureTests::startAndStopOwnExactlyOneObserverAndRunner()
{
    Harness h;
    QScopedPointer<SelectionContextFeature> feature(h.create());
    feature->start();
    feature->start();
    QCOMPARE(h.installCount, 1);
    feature->stop();
    feature->stop();
    QCOMPARE(h.uninstallCount, 1);
}

void SelectionContextFeatureTests::
disabledAutomaticModeInstallsHooksOnlyForAnActiveFallbackSession()
{
    Harness h;
    h.settings.selectionContext.enabled = false;
    QScopedPointer<SelectionContextFeature> feature(h.create());
    feature->start();
    QCOMPARE(h.installCount, 0);

    h.showSelection(feature.data());
    QCOMPARE(h.probeStarts, 1);
    QCOMPARE(h.installCount, 1);
    QVERIFY(h.toolbar(feature.data())->isVisible());

    QToolButton *close = h.toolbar(feature.data())->findChild<QToolButton *>(
        QStringLiteral("selectionToolbarCloseButton")
    );
    QVERIFY(close);
    close->click();
    QTRY_VERIFY(!h.toolbar(feature.data())->isVisible());
    QCOMPARE(h.uninstallCount, 1);
}

void SelectionContextFeatureTests::
observerInstallFailureStillAllowsTheFallbackHotkeyAndCloseButton()
{
    Harness h;
    h.settings.selectionContext.enabled = false;
    h.installSucceeds = false;
    QScopedPointer<SelectionContextFeature> feature(h.create());
    feature->start();
    h.showSelection(feature.data());
    QVERIFY(h.toolbar(feature.data())->isVisible());
    QCOMPARE(h.installCount, 1);

    QToolButton *close = h.toolbar(feature.data())->findChild<QToolButton *>(
        QStringLiteral("selectionToolbarCloseButton")
    );
    QVERIFY(close);
    close->click();
    QTRY_VERIFY(!h.toolbar(feature.data())->isVisible());
}

void SelectionContextFeatureTests::
lockOrSuspendCancelsProbesAndSessionsAndUnlockRestartsOnlyWhenEnabled()
{
    Harness h;
    QScopedPointer<SelectionContextFeature> feature(h.create());
    feature->start();
    h.showSelection(feature.data());
    QVERIFY(h.toolbar(feature.data())->isVisible());
    QVERIFY(h.observation);

    SelectionObservation unavailable;
    unavailable.reason = SelectionObservationReason::SystemUnavailable;
    h.observation(unavailable);
    QCoreApplication::processEvents();
    QVERIFY(!h.toolbar(feature.data())->isVisible());
    QVERIFY(h.probeCancels > 0);
    QCOMPARE(h.uninstallCount, 1);

    SelectionObservation available;
    available.reason = SelectionObservationReason::SystemAvailable;
    h.observation(available);
    QCOMPARE(h.installCount, 2);

    h.settings.selectionContext.enabled = false;
    feature->refresh();
    h.observation(unavailable);
    h.observation(available);
    QCOMPARE(h.installCount, 2);
}

void SelectionContextFeatureTests::
refreshAppliesSettingsWithoutRecreatingVisiblePinnedResult()
{
    Harness h;
    QScopedPointer<SelectionContextFeature> feature(h.create());
    feature->start();
    h.showSelection(feature.data());
    h.triggerModelAction(feature.data());
    SelectionResultCard *card = h.visibleCard(feature.data());
    QVERIFY(card);
    QToolButton *pin = card->findChild<QToolButton *>(
        QStringLiteral("selectionResultPinButton")
    );
    QVERIFY(pin);
    pin->click();
    QCOMPARE(feature->pinnedResultCount(), 1);

    h.settings.selectionContext.actionOrder = QStringList()
        << selectionContextActionCopy()
        << selectionContextActionAiSearch();
    feature->refresh();
    QCOMPARE(feature->pinnedResultCount(), 1);
    QVERIFY(card->isVisible());
}

void SelectionContextFeatureTests::
pinDetachesCurrentCardAndNextActionUsesANewCardUpToThreePinned()
{
    Harness h;
    QScopedPointer<SelectionContextFeature> feature(h.create());
    feature->start();
    QList<SelectionResultCard *> pinned;
    for (int i = 0; i < 3; ++i) {
        h.nextSnapshot = usableSnapshot(QStringLiteral("text-%1").arg(i));
        h.showSelection(feature.data());
        h.triggerModelAction(feature.data());
        SelectionResultCard *card = h.visibleCard(feature.data());
        QVERIFY(card);
        QVERIFY(!pinned.contains(card));
        pinned.append(card);
        QToolButton *pin = card->findChild<QToolButton *>(
            QStringLiteral("selectionResultPinButton")
        );
        QVERIFY(pin);
        QVERIFY(pin->isEnabled());
        pin->click();
        QCOMPARE(feature->pinnedResultCount(), i + 1);
    }

    h.nextSnapshot = usableSnapshot(QStringLiteral("fourth"));
    h.showSelection(feature.data());
    h.triggerModelAction(feature.data());
    SelectionResultCard *current = h.visibleCard(feature.data());
    QVERIFY(current);
    QVERIFY(!pinned.contains(current));
    QToolButton *pin = current->findChild<QToolButton *>(
        QStringLiteral("selectionResultPinButton")
    );
    QVERIFY(pin);
    QVERIFY(!pin->isEnabled());
}

void SelectionContextFeatureTests::
moreMenuListsEachCustomFunctionOnceAndRoutesStableIds()
{
    Harness h;
    FunctionSettings custom;
    custom.id = QStringLiteral("custom-one");
    custom.name = QStringLiteral("自定义一");
    custom.executionMode = FunctionExecutionMode::Classic;
    custom.modelId = QStringLiteral("openai:gpt-5.6-terra");
    custom.prompt = QStringLiteral("处理：{selected_text}");
    h.settings.functions << custom << custom;

    QScopedPointer<SelectionContextFeature> feature(h.create());
    feature->start();
    h.showSelection(feature.data());
    QMenu *menu = h.toolbar(feature.data())->findChild<QMenu *>(
        QStringLiteral("selectionContextMoreMenu")
    );
    QVERIFY(menu);
    const QString actionId = selectionContextActionForFunction(custom.id);
    int count = 0;
    for (QAction *action : menu->actions()) {
        count += action && action->data().toString() == actionId ? 1 : 0;
    }
    QCOMPARE(count, 1);
    QAction *action = menuAction(menu, actionId);
    QVERIFY(action);
    action->trigger();
    QTRY_COMPARE(h.modelRuns, 1);
}

void SelectionContextFeatureTests::
builtInAndCanvasFunctionsDoNotAppearInMoreMenu()
{
    Harness h;
    FunctionSettings builtIn;
    builtIn.id = QStringLiteral("translate");
    builtIn.name = QStringLiteral("翻译");
    builtIn.builtIn = true;
    FunctionSettings canvas;
    canvas.id = QStringLiteral("canvas-one");
    canvas.name = QStringLiteral("画布一");
    canvas.executionMode = FunctionExecutionMode::Canvas;
    h.settings.functions << builtIn << canvas;

    QScopedPointer<SelectionContextFeature> feature(h.create());
    feature->start();
    h.showSelection(feature.data());
    QMenu *menu = h.toolbar(feature.data())->findChild<QMenu *>(
        QStringLiteral("selectionContextMoreMenu")
    );
    QVERIFY(menu);
    QVERIFY(!menuAction(menu, selectionContextActionForFunction(builtIn.id)));
    QVERIFY(!menuAction(menu, selectionContextActionForFunction(canvas.id)));
}

void SelectionContextFeatureTests::
blockCurrentApplicationPersistsAndFailedPersistenceLeavesSurface()
{
    Harness h;
    QScopedPointer<SelectionContextFeature> feature(h.create());
    feature->start();
    h.showSelection(feature.data());
    SelectionContextToolbar *bar = h.toolbar(feature.data());
    QMenu *menu = bar->findChild<QMenu *>(
        QStringLiteral("selectionContextMoreMenu")
    );
    QAction *block = menuAction(menu, selectionContextMenuBlockApplication());
    QVERIFY(block);

    h.persistBlock = false;
    block->trigger();
    QVERIFY(bar->isVisible());
    QVERIFY(h.settings.selectionContext.blockedApplications.isEmpty());

    h.persistBlock = true;
    block->trigger();
    QTRY_VERIFY(!bar->isVisible());
    QCOMPARE(
        h.settings.selectionContext.blockedApplications,
        QStringList() << QStringLiteral("editor.exe")
    );
}

void SelectionContextFeatureTests::
openSettingsAndVocabularyInteractionsSuppressNativeCandidates()
{
    Harness h;
    QScopedPointer<SelectionContextFeature> feature(h.create());
    feature->start();
    h.showSelection(feature.data());

    QMenu *menu = h.toolbar(feature.data())->findChild<QMenu *>(
        QStringLiteral("selectionContextMoreMenu")
    );
    QAction *settings = menuAction(menu, selectionContextMenuOpenSettings());
    QVERIFY(settings);
    settings->trigger();
    QCOMPARE(h.openSettingsCount, 1);
    QVERIFY(h.settingsSawPausedObserver);

    h.showSelection(feature.data());
    QToolButton *save = h.toolbar(feature.data())->findChild<QToolButton *>(
        QStringLiteral("selectionActionSaveButton")
    );
    QVERIFY(save);
    save->click();
    QCOMPARE(h.vocabulary, QStringList() << QStringLiteral("selected text"));
    QVERIFY(h.vocabularySawPausedObserver);
}

void SelectionContextFeatureTests::
consentPromptSuppressesObserverAndDeclineSendsNothing()
{
    Harness h;
    h.settings.selectionContext.networkConsentAcknowledged = false;
    h.consentAccepted = false;
    QScopedPointer<SelectionContextFeature> feature(h.create());
    feature->start();
    h.showSelection(feature.data());
    h.triggerModelAction(feature.data());
    QCOMPARE(h.consentCount, 1);
    QVERIFY(h.consentSawPausedObserver);
    QCOMPARE(h.modelRuns, 0);
}

void SelectionContextFeatureTests::replaceRevalidatesTheOriginalWindowSelection()
{
    Harness h;
    SelectedTextNativeWindowHandle validatedWindow = nullptr;
    int validations = 0;
    h.dependencies.validateSelectionAsync = [
        &validatedWindow,
        &validations
    ](
        SelectedTextNativeWindowHandle window,
        quint64 generation,
        const std::function<void(quint64, bool)> &completed) {
        ++validations;
        validatedWindow = window;
        completed(generation, true);
    };
    QScopedPointer<SelectionContextFeature> feature(h.create());
    feature->start();
    h.showSelection(feature.data());
    h.triggerModelAction(feature.data());
    QTRY_COMPARE(h.modelRuns, 1);
    SelectionResultCard *card = h.visibleCard(feature.data());
    QVERIFY(card);
    QTRY_COMPARE(card->state().committedText, QStringLiteral("answer"));
    QPushButton *replace = card->findChild<QPushButton *>(
        QStringLiteral("selectionResultReplaceButton")
    );
    QVERIFY(replace);
    QVERIFY(replace->isEnabled());
    replace->click();
    QCOMPARE(validations, 1);
    QCOMPARE(validatedWindow, h.nextSnapshot.targetWindow);
    QCOMPARE(h.replaced, QStringList() << QStringLiteral("answer"));
}

void SelectionContextFeatureTests::
runtimeCompositionRoutesOnlyToolbarHotkeyAndUsesLocalVocabulary()
{
    const QString sourcePath = QFINDTESTDATA(
        "../../src/app/vocekit_application_runtime.cpp"
    );
    QVERIFY(!sourcePath.isEmpty());
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();
    const int selectionBranch = contents.indexOf(
        "id == QStringLiteral(\"selection_toolbar\")"
    );
    const int dispatch = contents.indexOf(
        "dispatchRegisteredHotkeyPress",
        selectionBranch
    );
    QVERIFY(selectionBranch >= 0);
    QVERIFY(dispatch > selectionBranch);
    QVERIFY(contents.mid(selectionBranch, dispatch - selectionBranch)
        .contains("selectionContextFeature.triggerFallbackShortcut"));
    QVERIFY(contents.contains("voice.addVocabularyLocallyForFlow"));
    QVERIFY(contents.contains("QStringLiteral(\"__global\")"));
}

void SelectionContextFeatureTests::
runtimeCompositionRefreshesAndStopsFeatureInOrder()
{
    const QString sourcePath = QFINDTESTDATA(
        "../../src/app/vocekit_application_runtime.cpp"
    );
    QVERIFY(!sourcePath.isEmpty());
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();
    QVERIFY(contents.contains("selectionContextFeature.refresh();"));
    QVERIFY(contents.contains("networkConsentAcknowledged = true"));
    QVERIFY(contents.contains("replaceNonFlowSettingsAndSave(next"));
    const int stop = contents.lastIndexOf("selectionContextFeature.stop();");
    const int clearHotkey = contents.lastIndexOf(
        "hotkeys.setCallback(std::function<void(const QString &)>());"
    );
    QVERIFY(stop >= 0);
    QVERIFY(clearHotkey > stop);
}

void SelectionContextFeatureTests::closingAResultReleasesTheCoordinatorSession()
{
    Harness h;
    QScopedPointer<SelectionContextFeature> feature(h.create());
    feature->start();
    h.showSelection(feature.data());
    h.triggerModelAction(feature.data());
    SelectionResultCard *card = h.visibleCard(feature.data());
    QVERIFY(card);
    QPushButton *close = card->findChild<QPushButton *>(
        QStringLiteral("selectionResultCloseButton")
    );
    QVERIFY(close);
    close->click();
    QTRY_VERIFY(!card->isVisible());

    const int probesBeforeRetry = h.probeStarts;
    h.showSelection(feature.data());
    QCOMPARE(h.probeStarts, probesBeforeRetry + 1);
    QVERIFY(h.toolbar(feature.data())->isVisible());

    h.triggerModelAction(feature.data());
    card = h.visibleCard(feature.data());
    QVERIFY(card);
    QToolButton *pin = card->findChild<QToolButton *>(
        QStringLiteral("selectionResultPinButton")
    );
    QVERIFY(pin);
    pin->click();
    QCOMPARE(feature->pinnedResultCount(), 1);
    close = card->findChild<QPushButton *>(
        QStringLiteral("selectionResultCloseButton")
    );
    QVERIFY(close);
    close->click();
    QTRY_COMPARE(feature->pinnedResultCount(), 0);
    const int probesBeforePinnedRetry = h.probeStarts;
    h.showSelection(feature.data());
    QCOMPARE(h.probeStarts, probesBeforePinnedRetry + 1);
    QVERIFY(h.toolbar(feature.data())->isVisible());
}

void SelectionContextFeatureTests::teardownStopsHooksAndCancelsWork()
{
    Harness h;
    SelectionContextFeature *feature = h.create();
    feature->start();
    h.showSelection(feature);
    h.triggerModelAction(feature);
    delete feature;
    QVERIFY(!h.installed);
    QVERIFY(h.uninstallCount >= 1);
    QVERIFY(h.probeCancels >= 1);
}

QTEST_MAIN(SelectionContextFeatureTests)

#include "selection_context_feature_tests.moc"
