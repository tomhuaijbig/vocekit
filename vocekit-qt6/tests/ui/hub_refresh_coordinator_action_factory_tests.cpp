#include <QtTest>

#include "../../src/ui/hub_refresh_coordinator_action_factory.h"

#include <QFile>

class HubRefreshCoordinatorActionFactoryTests : public QObject
{
    Q_OBJECT

private slots:
    void mapsSettingsAndLogActions();
    void mapsSharedUiActionsToBothFlows();
    void mapsFunctionFlowRefreshActions();
    void mapsHistoryActions();
    void handlesMissingActions();
    void hubWindowUsesActionFactory();
};

void HubRefreshCoordinatorActionFactoryTests::mapsSettingsAndLogActions()
{
    int reloads = 0;
    int modeRefreshes = 0;
    LogPaginationSnapshot received;
    HubRefreshDataAccess data;
    data.reloadSettings = [&reloads]() { ++reloads; };
    data.settingsSnapshot = []() {
        AppSettingsData settings;
        settings.logInitialLoadCount = 44;
        settings.logLoadMoreCount = 55;
        return settings;
    };
    HubRefreshUiActions ui;
    ui.refreshModeGrid = [&modeRefreshes]() { ++modeRefreshes; };
    ui.updateLogPagination = [&received](
        const LogPaginationSnapshot &snapshot
    ) {
        received = snapshot;
    };

    const HubRefreshCoordinatorBundleActions actions =
        createHubRefreshCoordinatorActions(data, ui);
    actions.settings.reloadSettings();
    actions.settings.refreshModeGrid();
    actions.settings.refreshLogs();

    QCOMPARE(reloads, 1);
    QCOMPARE(modeRefreshes, 1);
    QCOMPARE(received.initialLoadCount, 44);
    QCOMPARE(received.loadMoreCount, 55);
}

void HubRefreshCoordinatorActionFactoryTests::mapsSharedUiActionsToBothFlows()
{
    int recentRefreshes = 0;
    int activeRefreshes = 0;
    HubRefreshUiActions ui;
    ui.refreshRecentHistory = [&recentRefreshes]() {
        ++recentRefreshes;
    };
    ui.refreshActiveFunction = [&activeRefreshes]() {
        ++activeRefreshes;
    };

    const HubRefreshCoordinatorBundleActions actions =
        createHubRefreshCoordinatorActions(HubRefreshDataAccess(), ui);
    actions.settings.refreshRecentHistory();
    actions.content.refreshRecentHistory();
    actions.settings.refreshActiveFunction();
    actions.content.refreshActiveFunction();

    QCOMPARE(recentRefreshes, 2);
    QCOMPARE(activeRefreshes, 2);
}

void HubRefreshCoordinatorActionFactoryTests::mapsFunctionFlowRefreshActions()
{
    QStringList receivedIds;
    int activeFunctionRefreshes = 0;
    int canvasRefreshes = 0;
    int runtimeRefreshes = 0;
    int hotkeyRefreshes = 0;
    HubRefreshDataAccess data;
    data.reloadFunctionFlows = [&receivedIds](const QStringList &ids) {
        receivedIds = ids;
    };
    HubRefreshUiActions ui;
    ui.refreshActiveFunction = [&activeFunctionRefreshes]() {
        ++activeFunctionRefreshes;
    };
    ui.refreshActiveCanvas = [&canvasRefreshes]() {
        ++canvasRefreshes;
    };
    ui.refreshRuntime = [&runtimeRefreshes](const QStringList &) {
        ++runtimeRefreshes;
    };
    ui.refreshHotkeys = [&hotkeyRefreshes](const QStringList &) {
        ++hotkeyRefreshes;
    };

    const HubRefreshCoordinatorBundleActions actions =
        createHubRefreshCoordinatorActions(data, ui);
    actions.reloadFunctionFlows(
        QStringList() << QStringLiteral("custom_1")
    );
    actions.refreshActiveFunction();
    actions.refreshActiveCanvas();
    const QStringList ids =
        QStringList() << QStringLiteral("custom_1");
    actions.refreshRuntime(ids);
    actions.refreshHotkeys(ids);

    QCOMPARE(receivedIds, QStringList() << QStringLiteral("custom_1"));
    QCOMPARE(activeFunctionRefreshes, 1);
    QCOMPARE(canvasRefreshes, 1);
    QCOMPARE(runtimeRefreshes, 1);
    QCOMPARE(hotkeyRefreshes, 1);
}

void HubRefreshCoordinatorActionFactoryTests::mapsHistoryActions()
{
    int invalidations = 0;
    QList<bool> refreshModes;
    HubRefreshDataAccess data;
    data.historyCacheValid = []() { return true; };
    data.historyPageCreated = []() { return true; };
    data.invalidateHistoryCache = [&invalidations]() {
        ++invalidations;
    };
    data.refreshHistory = [&refreshModes](bool resetRequired) {
        refreshModes << resetRequired;
    };

    const HubRefreshCoordinatorBundleActions actions =
        createHubRefreshCoordinatorActions(data, HubRefreshUiActions());
    QVERIFY(actions.settings.historyCacheValid());
    QVERIFY(actions.content.historyPageCreated());
    actions.settings.refreshHistory();
    actions.content.invalidateHistoryCache();
    actions.content.refreshHistory(true);

    QCOMPARE(invalidations, 1);
    QCOMPARE(refreshModes, QList<bool>() << false << true);
}

void HubRefreshCoordinatorActionFactoryTests::handlesMissingActions()
{
    const HubRefreshCoordinatorBundleActions actions =
        createHubRefreshCoordinatorActions(
            HubRefreshDataAccess(),
            HubRefreshUiActions()
        );

    QVERIFY(!actions.settings.refreshLogs);
    QVERIFY(!actions.settings.historyCacheValid);
    QVERIFY(!actions.content.historyPageCreated);
}

void HubRefreshCoordinatorActionFactoryTests::hubWindowUsesActionFactory()
{
    const QString sourcePath = QFINDTESTDATA("../../src/ui/hub_window.cpp");
    QVERIFY2(!sourcePath.isEmpty(), "HubWindow source file not found");
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();

    QVERIFY(contents.contains("HubRefreshUiActions uiActions;"));
    QVERIFY(contents.contains("createHubRefreshDataAccess("));
    QVERIFY(contents.contains("createHubRefreshCoordinatorActions("));
    QVERIFY(!contents.contains("actions.settings.reloadSettings"));
    QVERIFY(!contents.contains("actions.content.invalidateHistoryCache"));
}

QTEST_APPLESS_MAIN(HubRefreshCoordinatorActionFactoryTests)

#include "hub_refresh_coordinator_action_factory_tests.moc"
