#include <QtTest>

#include "../../src/ui/hub_content_refresh_coordinator.h"

#include <QFile>

class HubContentRefreshCoordinatorTests : public QObject
{
    Q_OBJECT

private slots:
    void appliesHistoryRefreshInOrder();
    void skipsHistoryPageWhenNotCreated();
    void appliesVocabularyRefreshInOrder();
    void handlesMissingActions();
    void hubWindowDelegatesContentRefreshes();
};

void HubContentRefreshCoordinatorTests::appliesHistoryRefreshInOrder()
{
    QStringList calls;
    bool receivedReset = false;
    HubContentRefreshCoordinatorActions actions;
    actions.invalidateHistoryCache = [&calls]() {
        calls << QStringLiteral("invalidate");
    };
    actions.refreshRecentHistory = [&calls]() {
        calls << QStringLiteral("recent");
    };
    actions.historyPageCreated = []() { return true; };
    actions.refreshHistory = [&calls, &receivedReset](bool resetRequired) {
        calls << QStringLiteral("history");
        receivedReset = resetRequired;
    };
    HubContentRefreshCoordinator coordinator(actions);

    HistoryChangeSet change;
    change.resetRequired = true;
    coordinator.applyHistoryChanged(change);

    QCOMPARE(
        calls,
        QStringList()
            << QStringLiteral("invalidate")
            << QStringLiteral("recent")
            << QStringLiteral("history")
    );
    QVERIFY(receivedReset);
}

void HubContentRefreshCoordinatorTests::skipsHistoryPageWhenNotCreated()
{
    QStringList calls;
    HubContentRefreshCoordinatorActions actions;
    actions.invalidateHistoryCache = [&calls]() {
        calls << QStringLiteral("invalidate");
    };
    actions.refreshRecentHistory = [&calls]() {
        calls << QStringLiteral("recent");
    };
    actions.historyPageCreated = []() { return false; };
    actions.refreshHistory = [&calls](bool) {
        calls << QStringLiteral("history");
    };
    HubContentRefreshCoordinator coordinator(actions);

    coordinator.applyHistoryChanged(HistoryChangeSet());

    QCOMPARE(
        calls,
        QStringList()
            << QStringLiteral("invalidate")
            << QStringLiteral("recent")
    );
}

void HubContentRefreshCoordinatorTests::appliesVocabularyRefreshInOrder()
{
    QStringList calls;
    HubContentRefreshCoordinatorActions actions;
    actions.refreshVocabulary = [&calls]() {
        calls << QStringLiteral("vocabulary");
    };
    actions.refreshActiveFunction = [&calls]() {
        calls << QStringLiteral("active-function");
    };
    HubContentRefreshCoordinator coordinator(actions);

    coordinator.applyVocabularyChanged(VocabularyChangeSet());

    QCOMPARE(
        calls,
        QStringList()
            << QStringLiteral("vocabulary")
            << QStringLiteral("active-function")
    );
}

void HubContentRefreshCoordinatorTests::handlesMissingActions()
{
    HubContentRefreshCoordinatorActions actions;
    HubContentRefreshCoordinator coordinator(actions);

    coordinator.applyHistoryChanged(HistoryChangeSet());
    coordinator.applyVocabularyChanged(VocabularyChangeSet());
}

void HubContentRefreshCoordinatorTests::hubWindowDelegatesContentRefreshes()
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

    QVERIFY(bundleContents.contains("m_content->applyHistoryChanged(change);"));
    QVERIFY(bundleContents.contains("m_content->applyVocabularyChanged(change);"));
    QVERIFY(hubContents.contains("m_refreshCoordinators->dispatchHistoryChanged("));
    QVERIFY(hubContents.contains("m_refreshCoordinators->dispatchVocabularyChanged("));
    QVERIFY(!hubContents.contains("m_contentRefreshCoordinator"));
}

QTEST_APPLESS_MAIN(HubContentRefreshCoordinatorTests)

#include "hub_content_refresh_coordinator_tests.moc"
