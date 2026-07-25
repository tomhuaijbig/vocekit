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
