#include <QtTest>

#include "../../src/ui/hub_settings_refresh_coordinator.h"

#include <QFile>

class HubSettingsRefreshCoordinatorTests : public QObject
{
    Q_OBJECT

private slots:
    void appliesRefreshStepsInOrder();
    void skipsCachedHistoryRefreshWhenInvalid();
    void handlesMissingActions();
    void hubWindowDelegatesRefreshSequence();
};

void HubSettingsRefreshCoordinatorTests::appliesRefreshStepsInOrder()
{
    QStringList order;
    HubSettingsRefreshCoordinatorActions actions;
    actions.reloadSettings = [&order]() { order << QStringLiteral("reload"); };
    actions.refreshModeGrid = [&order]() { order << QStringLiteral("modes"); };
    actions.refreshStatus = [&order]() { order << QStringLiteral("status"); };
    actions.refreshPrompts = [&order]() { order << QStringLiteral("prompts"); };
    actions.refreshFunctions = [&order]() { order << QStringLiteral("functions"); };
    actions.refreshNavigation = [&order]() { order << QStringLiteral("navigation"); };
    actions.refreshActiveFunction = [&order]() { order << QStringLiteral("active"); };
    actions.refreshOcr = [&order]() { order << QStringLiteral("ocr"); };
    actions.refreshLogs = [&order]() { order << QStringLiteral("logs"); };
    actions.historyCacheValid = [&order]() {
        order << QStringLiteral("history-check");
        return true;
    };
    actions.refreshHistory = [&order]() { order << QStringLiteral("history"); };
    actions.refreshRecentHistory = [&order]() { order << QStringLiteral("recent"); };

    HubSettingsRefreshCoordinator coordinator(actions);
    coordinator.apply();

    QCOMPARE(
        order,
        QStringList()
            << QStringLiteral("reload")
            << QStringLiteral("modes")
            << QStringLiteral("status")
            << QStringLiteral("prompts")
            << QStringLiteral("functions")
            << QStringLiteral("navigation")
            << QStringLiteral("active")
            << QStringLiteral("ocr")
            << QStringLiteral("logs")
            << QStringLiteral("history-check")
            << QStringLiteral("history")
            << QStringLiteral("recent")
    );
}

void HubSettingsRefreshCoordinatorTests::skipsCachedHistoryRefreshWhenInvalid()
{
    int historyRefreshes = 0;
    int recentRefreshes = 0;
    HubSettingsRefreshCoordinatorActions actions;
    actions.historyCacheValid = []() { return false; };
    actions.refreshHistory = [&historyRefreshes]() { ++historyRefreshes; };
    actions.refreshRecentHistory = [&recentRefreshes]() { ++recentRefreshes; };

    HubSettingsRefreshCoordinator coordinator(actions);
    coordinator.apply();

    QCOMPARE(historyRefreshes, 0);
    QCOMPARE(recentRefreshes, 1);
}

void HubSettingsRefreshCoordinatorTests::handlesMissingActions()
{
        HubSettingsRefreshCoordinatorActions actions;
        HubSettingsRefreshCoordinator coordinator(actions);
    coordinator.apply();
}

void HubSettingsRefreshCoordinatorTests::hubWindowDelegatesRefreshSequence()
{
    const QString hubPath = QFINDTESTDATA("../../src/ui/hub_window.cpp");
    const QString bundlePath = QFINDTESTDATA(
        "../../src/ui/hub_refresh_coordinator_bundle.cpp"
    );
    QVERIFY2(!hubPath.isEmpty(), "HubWindow source file not found");
    QVERIFY2(!bundlePath.isEmpty(), "Refresh bundle source file not found");
    QFile hubSource(hubPath);
    QFile bundleSource(bundlePath);
    QVERIFY(hubSource.open(QIODevice::ReadOnly));
    QVERIFY(bundleSource.open(QIODevice::ReadOnly));
    const QByteArray hubContents = hubSource.readAll();
    const QByteArray bundleContents = bundleSource.readAll();

    QVERIFY(bundleContents.contains("m_settings->apply();"));
    QVERIFY(hubContents.contains("m_refreshCoordinators->dispatchSettingsChanged("));
    QVERIFY(!hubContents.contains("m_refreshCoordinators->applySettingsChanged();"));
    QVERIFY(!hubContents.contains("m_settingsRefreshCoordinator"));
    QVERIFY(!hubContents.contains("void refreshShortcuts()"));
}

QTEST_APPLESS_MAIN(HubSettingsRefreshCoordinatorTests)

#include "hub_settings_refresh_coordinator_tests.moc"
