#include <QtTest>

#include "../../src/ui/hub_refresh_coordinator_bundle.h"

#include <QFile>

class HubRefreshCoordinatorBundleTests : public QObject
{
    Q_OBJECT

private slots:
    void dispatchesSettingsWithoutEventCenter();
    void dispatchesSettingsThroughEventCenter();
    void receivesSettingsEvents();
    void draftAndEditorChangesOnlyRefreshTheAffectedCanvas();
    void publishedAndModeChangesRefreshExpectedTargets();
    void coalescedDefinitionsAndModeRefreshEachTargetOnce();
    void unknownAndModeStillRefreshRuntimeOnce();
    void unknownAndPublishedStillRefreshRuntimeOnce();
    void functionScopedChangesWithoutIdsUseFullRefresh_data();
    void functionScopedChangesWithoutIdsUseFullRefresh();
    void duplicateFunctionIdsAreNormalizedForEveryAction();
    void definitionsAndUnknownChangesKeepConservativeRefreshSemantics();
    void dispatchesHistoryWithoutEventCenter();
    void dispatchesHistoryThroughEventCenter();
    void dispatchesVocabularyChanges();
    void replacesEventCenterWithoutDuplicateCallbacks();
    void hubWindowUsesCoordinatorBundle();
};

void HubRefreshCoordinatorBundleTests::dispatchesSettingsWithoutEventCenter()
{
    int reloads = 0;
    HubRefreshCoordinatorBundleActions actions;
    actions.settings.reloadSettings = [&reloads]() { ++reloads; };
    HubRefreshCoordinatorBundle bundle(actions);

    bundle.dispatchSettingsChanged(QStringList(), QStringList());

    QCOMPARE(reloads, 1);
}

void HubRefreshCoordinatorBundleTests::dispatchesSettingsThroughEventCenter()
{
    int reloads = 0;
    HubRefreshCoordinatorBundleActions actions;
    actions.settings.reloadSettings = [&reloads]() { ++reloads; };
    HubRefreshCoordinatorBundle bundle(actions);
    ApplicationEvents events;
    bundle.setApplicationEvents(&events);

    bundle.dispatchSettingsChanged(
        QStringList() << QStringLiteral("floatingBarEnabled"),
        QStringList()
    );

    QCOMPARE(reloads, 1);
}

void HubRefreshCoordinatorBundleTests::receivesSettingsEvents()
{
    int reloads = 0;
    HubRefreshCoordinatorBundleActions actions;
    actions.settings.reloadSettings = [&reloads]() { ++reloads; };
    HubRefreshCoordinatorBundle bundle(actions);
    ApplicationEvents events;
    bundle.setApplicationEvents(&events);

    events.publishSettingsChanged(SettingsChangeSet());

    QCOMPARE(reloads, 1);
}

void HubRefreshCoordinatorBundleTests::draftAndEditorChangesOnlyRefreshTheAffectedCanvas()
{
    int fullReloads = 0;
    int canvasRefreshes = 0;
    int runtimeReloads = 0;
    int hotkeyRegistrations = 0;
    QList<QStringList> reloadedFunctions;
    HubRefreshCoordinatorBundleActions actions;
    actions.settings.reloadSettings = [&fullReloads]() { ++fullReloads; };
    actions.reloadFunctionFlows = [&reloadedFunctions](
        const QStringList &ids
    ) {
        reloadedFunctions << ids;
    };
    actions.refreshActiveCanvas = [&canvasRefreshes]() {
        ++canvasRefreshes;
    };
    actions.refreshRuntime = [&runtimeReloads](const QStringList &) {
        ++runtimeReloads;
    };
    actions.refreshHotkeys = [&hotkeyRegistrations](const QStringList &) {
        ++hotkeyRegistrations;
    };
    HubRefreshCoordinatorBundle bundle(actions);

    SettingsChangeSet draft;
    draft.keys << functionFlowDraftSettingsKey();
    draft.functionIds << QStringLiteral("custom_1");
    bundle.apply(draft);

    SettingsChangeSet editor;
    editor.keys << functionFlowEditorStateSettingsKey();
    editor.functionIds << QStringLiteral("custom_2");
    bundle.apply(editor);

    QCOMPARE(fullReloads, 0);
    QCOMPARE(canvasRefreshes, 2);
    QCOMPARE(runtimeReloads, 0);
    QCOMPARE(hotkeyRegistrations, 0);
    QCOMPARE(reloadedFunctions.size(), 2);
    QCOMPARE(
        reloadedFunctions.at(0),
        QStringList() << QStringLiteral("custom_1")
    );
    QCOMPARE(
        reloadedFunctions.at(1),
        QStringList() << QStringLiteral("custom_2")
    );
}

void HubRefreshCoordinatorBundleTests::
publishedAndModeChangesRefreshExpectedTargets()
{
    int activeFunctionRefreshes = 0;
    int canvasRefreshes = 0;
    int runtimeReloads = 0;
    int hotkeyRegistrations = 0;
    HubRefreshCoordinatorBundleActions actions;
    actions.reloadFunctionFlows = [](const QStringList &) {};
    actions.refreshActiveFunction = [&activeFunctionRefreshes]() {
        ++activeFunctionRefreshes;
    };
    actions.refreshActiveCanvas = [&canvasRefreshes]() {
        ++canvasRefreshes;
    };
    actions.refreshRuntime = [&runtimeReloads](const QStringList &) {
        ++runtimeReloads;
    };
    actions.refreshHotkeys = [&hotkeyRegistrations](const QStringList &) {
        ++hotkeyRegistrations;
    };
    HubRefreshCoordinatorBundle bundle(actions);

    SettingsChangeSet published;
    published.keys << functionFlowPublishedSettingsKey();
    published.functionIds << QStringLiteral("custom_1");
    bundle.apply(published);

    QCOMPARE(activeFunctionRefreshes, 0);
    QCOMPARE(canvasRefreshes, 1);
    QCOMPARE(runtimeReloads, 1);
    QCOMPARE(hotkeyRegistrations, 1);

    SettingsChangeSet mode;
    mode.keys << functionExecutionModeSettingsKey();
    mode.functionIds << QStringLiteral("custom_1");
    bundle.apply(mode);

    QCOMPARE(activeFunctionRefreshes, 1);
    QCOMPARE(canvasRefreshes, 2);
    QCOMPARE(runtimeReloads, 2);
    QCOMPARE(hotkeyRegistrations, 2);
}

void HubRefreshCoordinatorBundleTests::
coalescedDefinitionsAndModeRefreshEachTargetOnce()
{
    int fullReloads = 0;
    int activeFunctionRefreshes = 0;
    int narrowReloads = 0;
    int canvasRefreshes = 0;
    int runtimeRefreshes = 0;
    int hotkeyRefreshes = 0;
    HubRefreshCoordinatorBundleActions actions;
    actions.settings.reloadSettings =
        [&]() { ++fullReloads; };
    actions.settings.refreshActiveFunction =
        [&]() { ++activeFunctionRefreshes; };
    actions.reloadFunctionFlows =
        [&](const QStringList &) { ++narrowReloads; };
    actions.refreshActiveFunction =
        [&]() { ++activeFunctionRefreshes; };
    actions.refreshActiveCanvas =
        [&]() { ++canvasRefreshes; };
    actions.refreshRuntime =
        [&](const QStringList &) { ++runtimeRefreshes; };
    actions.refreshHotkeys =
        [&](const QStringList &) { ++hotkeyRefreshes; };
    HubRefreshCoordinatorBundle bundle(actions);

    SettingsChangeSet change;
    change.keys
        << functionDefinitionsSettingsKey()
        << functionExecutionModeSettingsKey();
    change.functionIds << QStringLiteral("custom_1");
    bundle.apply(change);

    QCOMPARE(fullReloads, 1);
    QCOMPARE(activeFunctionRefreshes, 1);
    QCOMPARE(narrowReloads, 0);
    QCOMPARE(canvasRefreshes, 0);
    QCOMPARE(runtimeRefreshes, 1);
    QCOMPARE(hotkeyRefreshes, 1);
}

void HubRefreshCoordinatorBundleTests::
unknownAndModeStillRefreshRuntimeOnce()
{
    int fullReloads = 0;
    int activeFunctionRefreshes = 0;
    int narrowReloads = 0;
    int canvasRefreshes = 0;
    int runtimeRefreshes = 0;
    int hotkeyRefreshes = 0;
    const QStringList expectedIds =
        QStringList() << QStringLiteral("custom_1");
    HubRefreshCoordinatorBundleActions actions;
    actions.settings.reloadSettings =
        [&]() { ++fullReloads; };
    actions.settings.refreshActiveFunction =
        [&]() { ++activeFunctionRefreshes; };
    actions.reloadFunctionFlows =
        [&](const QStringList &) { ++narrowReloads; };
    actions.refreshActiveFunction =
        [&]() { ++activeFunctionRefreshes; };
    actions.refreshActiveCanvas =
        [&]() { ++canvasRefreshes; };
    actions.refreshRuntime =
        [&](const QStringList &ids) {
            QCOMPARE(ids, expectedIds);
            ++runtimeRefreshes;
        };
    actions.refreshHotkeys =
        [&](const QStringList &ids) {
            QCOMPARE(ids, expectedIds);
            ++hotkeyRefreshes;
        };
    HubRefreshCoordinatorBundle bundle(actions);

    SettingsChangeSet change;
    change.keys
        << QStringLiteral("speechProvider")
        << functionExecutionModeSettingsKey();
    change.functionIds = expectedIds;
    bundle.apply(change);

    QCOMPARE(fullReloads, 1);
    QCOMPARE(activeFunctionRefreshes, 1);
    QCOMPARE(narrowReloads, 0);
    QCOMPARE(canvasRefreshes, 0);
    QCOMPARE(runtimeRefreshes, 1);
    QCOMPARE(hotkeyRefreshes, 1);
}

void HubRefreshCoordinatorBundleTests::
unknownAndPublishedStillRefreshRuntimeOnce()
{
    int fullReloads = 0;
    int activeFunctionRefreshes = 0;
    int narrowReloads = 0;
    int canvasRefreshes = 0;
    int runtimeRefreshes = 0;
    int hotkeyRefreshes = 0;
    const QStringList expectedIds =
        QStringList() << QStringLiteral("custom_2");
    HubRefreshCoordinatorBundleActions actions;
    actions.settings.reloadSettings =
        [&]() { ++fullReloads; };
    actions.settings.refreshActiveFunction =
        [&]() { ++activeFunctionRefreshes; };
    actions.reloadFunctionFlows =
        [&](const QStringList &) { ++narrowReloads; };
    actions.refreshActiveFunction =
        [&]() { ++activeFunctionRefreshes; };
    actions.refreshActiveCanvas =
        [&]() { ++canvasRefreshes; };
    actions.refreshRuntime =
        [&](const QStringList &ids) {
            QCOMPARE(ids, expectedIds);
            ++runtimeRefreshes;
        };
    actions.refreshHotkeys =
        [&](const QStringList &ids) {
            QCOMPARE(ids, expectedIds);
            ++hotkeyRefreshes;
        };
    HubRefreshCoordinatorBundle bundle(actions);

    SettingsChangeSet change;
    change.keys
        << QStringLiteral("speechProvider")
        << functionFlowPublishedSettingsKey();
    change.functionIds = expectedIds;
    bundle.apply(change);

    QCOMPARE(fullReloads, 1);
    QCOMPARE(activeFunctionRefreshes, 1);
    QCOMPARE(narrowReloads, 0);
    QCOMPARE(canvasRefreshes, 0);
    QCOMPARE(runtimeRefreshes, 1);
    QCOMPARE(hotkeyRefreshes, 1);
}

void HubRefreshCoordinatorBundleTests::
functionScopedChangesWithoutIdsUseFullRefresh_data()
{
    QTest::addColumn<QString>("key");
    QTest::addColumn<bool>("runtimeExpected");

    QTest::newRow("draft")
        << functionFlowDraftSettingsKey()
        << false;
    QTest::newRow("published")
        << functionFlowPublishedSettingsKey()
        << true;
    QTest::newRow("mode")
        << functionExecutionModeSettingsKey()
        << true;
}

void HubRefreshCoordinatorBundleTests::
functionScopedChangesWithoutIdsUseFullRefresh()
{
    QFETCH(QString, key);
    QFETCH(bool, runtimeExpected);

    int fullReloads = 0;
    int activeFunctionRefreshes = 0;
    int narrowReloads = 0;
    int canvasRefreshes = 0;
    int runtimeRefreshes = 0;
    int hotkeyRefreshes = 0;
    HubRefreshCoordinatorBundleActions actions;
    actions.settings.reloadSettings =
        [&]() { ++fullReloads; };
    actions.settings.refreshActiveFunction =
        [&]() { ++activeFunctionRefreshes; };
    actions.reloadFunctionFlows =
        [&](const QStringList &) { ++narrowReloads; };
    actions.refreshActiveFunction =
        [&]() { ++activeFunctionRefreshes; };
    actions.refreshActiveCanvas =
        [&]() { ++canvasRefreshes; };
    actions.refreshRuntime =
        [&](const QStringList &ids) {
            QVERIFY(ids.isEmpty());
            ++runtimeRefreshes;
        };
    actions.refreshHotkeys =
        [&](const QStringList &ids) {
            QVERIFY(ids.isEmpty());
            ++hotkeyRefreshes;
        };
    HubRefreshCoordinatorBundle bundle(actions);

    SettingsChangeSet change;
    change.keys << key;
    bundle.apply(change);

    QCOMPARE(fullReloads, 1);
    QCOMPARE(activeFunctionRefreshes, 1);
    QCOMPARE(narrowReloads, 0);
    QCOMPARE(canvasRefreshes, 0);
    QCOMPARE(runtimeRefreshes, runtimeExpected ? 1 : 0);
    QCOMPARE(hotkeyRefreshes, runtimeExpected ? 1 : 0);
}

void HubRefreshCoordinatorBundleTests::
duplicateFunctionIdsAreNormalizedForEveryAction()
{
    int fullReloads = 0;
    int activeFunctionRefreshes = 0;
    int canvasRefreshes = 0;
    QList<QStringList> reloadedIds;
    QList<QStringList> runtimeIds;
    QList<QStringList> hotkeyIds;
    const QStringList expectedIds =
        QStringList()
            << QStringLiteral("custom_1")
            << QStringLiteral("custom_2");
    HubRefreshCoordinatorBundleActions actions;
    actions.settings.reloadSettings =
        [&]() { ++fullReloads; };
    actions.reloadFunctionFlows =
        [&](const QStringList &ids) { reloadedIds << ids; };
    actions.refreshActiveFunction =
        [&]() { ++activeFunctionRefreshes; };
    actions.refreshActiveCanvas =
        [&]() { ++canvasRefreshes; };
    actions.refreshRuntime =
        [&](const QStringList &ids) { runtimeIds << ids; };
    actions.refreshHotkeys =
        [&](const QStringList &ids) { hotkeyIds << ids; };
    HubRefreshCoordinatorBundle bundle(actions);

    SettingsChangeSet change;
    change.keys << functionExecutionModeSettingsKey();
    change.functionIds
        << QStringLiteral(" custom_1 ")
        << QStringLiteral("custom_1")
        << QString()
        << QStringLiteral("  ")
        << QStringLiteral("custom_2")
        << QStringLiteral(" custom_2 ");
    bundle.apply(change);

    QCOMPARE(fullReloads, 0);
    QCOMPARE(activeFunctionRefreshes, 1);
    QCOMPARE(canvasRefreshes, 1);
    QCOMPARE(reloadedIds, QList<QStringList>() << expectedIds);
    QCOMPARE(runtimeIds, QList<QStringList>() << expectedIds);
    QCOMPARE(hotkeyIds, QList<QStringList>() << expectedIds);
}

void HubRefreshCoordinatorBundleTests::definitionsAndUnknownChangesKeepConservativeRefreshSemantics()
{
    int fullReloads = 0;
    int runtimeReloads = 0;
    int hotkeyRegistrations = 0;
    int narrowReloads = 0;
    HubRefreshCoordinatorBundleActions actions;
    actions.settings.reloadSettings = [&fullReloads]() { ++fullReloads; };
    actions.reloadFunctionFlows = [&narrowReloads](const QStringList &) {
        ++narrowReloads;
    };
    actions.refreshRuntime = [&runtimeReloads](const QStringList &) {
        ++runtimeReloads;
    };
    actions.refreshHotkeys = [&hotkeyRegistrations](const QStringList &) {
        ++hotkeyRegistrations;
    };
    HubRefreshCoordinatorBundle bundle(actions);

    SettingsChangeSet definitions;
    definitions.keys << functionDefinitionsSettingsKey();
    definitions.functionIds << QStringLiteral("custom_1");
    bundle.apply(definitions);
    QCOMPARE(fullReloads, 1);
    QCOMPARE(runtimeReloads, 1);
    QCOMPARE(hotkeyRegistrations, 1);
    QCOMPARE(narrowReloads, 0);

    SettingsChangeSet unknown;
    unknown.keys << QStringLiteral("speechProvider");
    bundle.apply(unknown);
    QCOMPARE(fullReloads, 2);
    QCOMPARE(runtimeReloads, 1);
    QCOMPARE(hotkeyRegistrations, 1);

    bundle.apply(SettingsChangeSet());
    QCOMPARE(fullReloads, 3);
}

void HubRefreshCoordinatorBundleTests::dispatchesHistoryWithoutEventCenter()
{
    QStringList calls;
    HubRefreshCoordinatorBundleActions actions;
    actions.content.invalidateHistoryCache = [&calls]() {
        calls << QStringLiteral("invalidate");
    };
    actions.content.refreshRecentHistory = [&calls]() {
        calls << QStringLiteral("recent");
    };
    actions.content.historyPageCreated = []() { return false; };
    HubRefreshCoordinatorBundle bundle(actions);

    bundle.dispatchHistoryChanged(
        QStringList() << QStringLiteral("local.json"),
        false
    );

    QCOMPARE(
        calls,
        QStringList()
            << QStringLiteral("invalidate")
            << QStringLiteral("recent")
    );
}

void HubRefreshCoordinatorBundleTests::dispatchesHistoryThroughEventCenter()
{
    bool receivedReset = false;
    HubRefreshCoordinatorBundleActions actions;
    actions.content.historyPageCreated = []() { return true; };
    actions.content.refreshHistory = [&receivedReset](bool resetRequired) {
        receivedReset = resetRequired;
    };
    HubRefreshCoordinatorBundle bundle(actions);
    ApplicationEvents events;
    bundle.setApplicationEvents(&events);

    bundle.dispatchHistoryChanged(QStringList(), true);

    QVERIFY(receivedReset);
}

void HubRefreshCoordinatorBundleTests::dispatchesVocabularyChanges()
{
    int refreshes = 0;
    HubRefreshCoordinatorBundleActions actions;
    actions.content.refreshVocabulary = [&refreshes]() { ++refreshes; };
    HubRefreshCoordinatorBundle bundle(actions);

    bundle.dispatchVocabularyChanged(
        QStringList() << QStringLiteral("entry-1"),
        false
    );

    QCOMPARE(refreshes, 1);
}

void HubRefreshCoordinatorBundleTests::replacesEventCenterWithoutDuplicateCallbacks()
{
    int reloads = 0;
    HubRefreshCoordinatorBundleActions actions;
    actions.settings.reloadSettings = [&reloads]() { ++reloads; };
    HubRefreshCoordinatorBundle bundle(actions);
    ApplicationEvents first;
    ApplicationEvents second;

    bundle.setApplicationEvents(&first);
    bundle.setApplicationEvents(&second);
    first.publishSettingsChanged(SettingsChangeSet());
    second.publishSettingsChanged(SettingsChangeSet());

    QCOMPARE(reloads, 1);
}

void HubRefreshCoordinatorBundleTests::hubWindowUsesCoordinatorBundle()
{
    const QString sourcePath = QFINDTESTDATA("../../src/ui/hub_window.cpp");
    QVERIFY2(!sourcePath.isEmpty(), "HubWindow source file not found");
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();

    QVERIFY(contents.contains("HubRefreshUiActions uiActions;"));
    QVERIFY(contents.contains("createHubRefreshCoordinatorActions("));
    QVERIFY(contents.contains("m_refreshCoordinators.reset("));
    QVERIFY(contents.contains("m_refreshCoordinators->setApplicationEvents(events);"));
    QVERIFY(!contents.contains("m_eventCoordinator"));
    QVERIFY(!contents.contains("m_contentRefreshCoordinator"));
    QVERIFY(!contents.contains("m_settingsRefreshCoordinator"));
    QVERIFY(!contents.contains("HubApplicationEventCoordinatorCallbacks callbacks;"));
}

QTEST_APPLESS_MAIN(HubRefreshCoordinatorBundleTests)

#include "hub_refresh_coordinator_bundle_tests.moc"
