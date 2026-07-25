#include <QtTest>

#include "../../src/app/application_events.h"
#include "../../src/ui/hub_application_event_coordinator.h"

#include <QFile>
#include <QScopedPointer>

class HubApplicationEventCoordinatorTests : public QObject
{
    Q_OBJECT

private slots:
    void forwardsSubscribedChanges();
    void dispatchesSettingsThroughEventCenter();
    void dispatchesHistoryThroughEventCenter();
    void dispatchesVocabularyThroughEventCenter();
    void dispatchesSettingsDirectlyWithoutEventCenter();
    void dispatchesHistoryDirectlyWithoutEventCenter();
    void dispatchesVocabularyDirectlyWithoutEventCenter();
    void disconnectsWhenDestroyed();
    void handlesMissingCallbacks();
    void hubWindowUsesCoordinator();
};

void HubApplicationEventCoordinatorTests::forwardsSubscribedChanges()
{
    ApplicationEvents events;
    int settingsRefreshes = 0;
    HistoryChangeSet receivedHistory;
    VocabularyChangeSet receivedVocabulary;

    HubApplicationEventCoordinatorCallbacks callbacks;
    callbacks.settingsChanged = [&settingsRefreshes](const SettingsChangeSet &) {
        ++settingsRefreshes;
    };
    callbacks.historyChanged = [&receivedHistory](const HistoryChangeSet &change) {
        receivedHistory = change;
    };
    callbacks.vocabularyChanged = [&receivedVocabulary](
        const VocabularyChangeSet &change
    ) {
        receivedVocabulary = change;
    };
    HubApplicationEventCoordinator coordinator(&events, callbacks);

    SettingsChangeSet settings;
    settings.keys << QStringLiteral("speechProvider");
    events.publishSettingsChanged(settings);

    HistoryChangeSet history;
    history.recordIds << QStringLiteral("record-1");
    history.resetRequired = true;
    events.publishHistoryChanged(history);

    VocabularyChangeSet vocabulary;
    vocabulary.entryIds << QStringLiteral("entry-1");
    events.publishVocabularyChanged(vocabulary);

    QCOMPARE(settingsRefreshes, 1);
    QCOMPARE(receivedHistory.recordIds, history.recordIds);
    QVERIFY(receivedHistory.resetRequired);
    QCOMPARE(receivedVocabulary.entryIds, vocabulary.entryIds);
}

void HubApplicationEventCoordinatorTests::dispatchesHistoryThroughEventCenter()
{
    ApplicationEvents events;
    HistoryChangeSet received;
    HubApplicationEventCoordinatorCallbacks callbacks;
    callbacks.historyChanged = [&received](const HistoryChangeSet &change) {
        received = change;
    };
    HubApplicationEventCoordinator coordinator(&events, callbacks);

    coordinator.dispatchHistoryChanged(
        QStringList() << QStringLiteral("detail.json"),
        true
    );

    QCOMPARE(received.recordIds, QStringList() << QStringLiteral("detail.json"));
    QVERIFY(received.resetRequired);
}

void HubApplicationEventCoordinatorTests::dispatchesSettingsThroughEventCenter()
{
    ApplicationEvents events;
    SettingsChangeSet received;
    HubApplicationEventCoordinatorCallbacks callbacks;
    callbacks.settingsChanged = [&received](const SettingsChangeSet &change) {
        received = change;
    };
    HubApplicationEventCoordinator coordinator(&events, callbacks);

    coordinator.dispatchSettingsChanged(
        QStringList() << QStringLiteral("speechProvider"),
        QStringList() << QStringLiteral("dictate")
    );

    QCOMPARE(
        received.keys,
        QStringList() << QStringLiteral("speechProvider")
    );
    QCOMPARE(
        received.functionIds,
        QStringList() << QStringLiteral("dictate")
    );
}

void HubApplicationEventCoordinatorTests::dispatchesVocabularyThroughEventCenter()
{
    ApplicationEvents events;
    VocabularyChangeSet received;
    HubApplicationEventCoordinatorCallbacks callbacks;
    callbacks.vocabularyChanged = [&received](
        const VocabularyChangeSet &change
    ) {
        received = change;
    };
    HubApplicationEventCoordinator coordinator(&events, callbacks);

    coordinator.dispatchVocabularyChanged(
        QStringList() << QStringLiteral("entry-2"),
        false
    );

    QCOMPARE(received.entryIds, QStringList() << QStringLiteral("entry-2"));
    QVERIFY(!received.resetRequired);
}

void HubApplicationEventCoordinatorTests::dispatchesSettingsDirectlyWithoutEventCenter()
{
    SettingsChangeSet received;
    HubApplicationEventCoordinatorCallbacks callbacks;
    callbacks.settingsChanged = [&received](const SettingsChangeSet &change) {
        received = change;
    };
    HubApplicationEventCoordinator coordinator(nullptr, callbacks);

    coordinator.dispatchSettingsChanged(
        QStringList() << QStringLiteral("resultPopupOpacity"),
        QStringList() << QStringLiteral("translate")
    );

    QCOMPARE(
        received.keys,
        QStringList() << QStringLiteral("resultPopupOpacity")
    );
    QCOMPARE(
        received.functionIds,
        QStringList() << QStringLiteral("translate")
    );
}

void HubApplicationEventCoordinatorTests::dispatchesHistoryDirectlyWithoutEventCenter()
{
    HistoryChangeSet received;
    HubApplicationEventCoordinatorCallbacks callbacks;
    callbacks.historyChanged = [&received](const HistoryChangeSet &change) {
        received = change;
    };
    HubApplicationEventCoordinator coordinator(nullptr, callbacks);

    coordinator.dispatchHistoryChanged(
        QStringList() << QStringLiteral("local-history.json"),
        false
    );

    QCOMPARE(
        received.recordIds,
        QStringList() << QStringLiteral("local-history.json")
    );
    QVERIFY(!received.resetRequired);
}

void HubApplicationEventCoordinatorTests::dispatchesVocabularyDirectlyWithoutEventCenter()
{
    VocabularyChangeSet received;
    HubApplicationEventCoordinatorCallbacks callbacks;
    callbacks.vocabularyChanged = [&received](
        const VocabularyChangeSet &change
    ) {
        received = change;
    };
    HubApplicationEventCoordinator coordinator(nullptr, callbacks);

    coordinator.dispatchVocabularyChanged(
        QStringList() << QStringLiteral("local-entry"),
        true
    );

    QCOMPARE(
        received.entryIds,
        QStringList() << QStringLiteral("local-entry")
    );
    QVERIFY(received.resetRequired);
}

void HubApplicationEventCoordinatorTests::disconnectsWhenDestroyed()
{
    ApplicationEvents events;
    int refreshes = 0;
    HubApplicationEventCoordinatorCallbacks callbacks;
    callbacks.settingsChanged = [&refreshes](const SettingsChangeSet &) {
        ++refreshes;
    };

    QScopedPointer<HubApplicationEventCoordinator> coordinator(
        new HubApplicationEventCoordinator(&events, callbacks)
    );
    coordinator.reset();
    events.publishSettingsChanged(SettingsChangeSet());

    QCOMPARE(refreshes, 0);
}

void HubApplicationEventCoordinatorTests::handlesMissingCallbacks()
{
    HubApplicationEventCoordinator coordinator(
        nullptr,
        HubApplicationEventCoordinatorCallbacks()
    );
    coordinator.dispatchSettingsChanged(QStringList(), QStringList());
    coordinator.dispatchHistoryChanged(QStringList(), false);
    coordinator.dispatchVocabularyChanged(QStringList(), true);
}

void HubApplicationEventCoordinatorTests::hubWindowUsesCoordinator()
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

    QVERIFY(hubContents.contains("m_refreshCoordinators->dispatchSettingsChanged("));
    QVERIFY(hubContents.contains("m_refreshCoordinators->dispatchHistoryChanged("));
    QVERIFY(hubContents.contains("m_refreshCoordinators->dispatchVocabularyChanged("));
    QVERIFY(!hubContents.contains("HubApplicationEventCoordinatorCallbacks callbacks;"));
    QVERIFY(!hubContents.contains("m_eventCoordinator"));
    QVERIFY(bundleContents.contains("HubApplicationEventCoordinatorCallbacks callbacks;"));
    QVERIFY(bundleContents.contains("new HubApplicationEventCoordinator(events, callbacks)"));
}

QTEST_MAIN(HubApplicationEventCoordinatorTests)

#include "hub_application_event_coordinator_tests.moc"
