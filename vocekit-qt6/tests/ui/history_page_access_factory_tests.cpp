#include <QtTest>

#include "../../src/ui/history_page_access_factory.h"
#include "../../src/ui/hub_settings_state.h"

#include <QFile>

namespace {

AppSettingsData sampleSettings()
{
    AppSettingsData settings;
    settings.recordDirectory = QStringLiteral("C:/records");
    settings.favoriteFolders << QStringLiteral("重要");
    settings.historyInitialLoadCount = 18;
    settings.historyLoadMoreCount = 27;
    settings.speechProvider = QStringLiteral("xfyun");
    settings.useSystemProxy = true;

    FunctionSettings dictate;
    dictate.id = QStringLiteral("dictate");
    dictate.name = QStringLiteral("听写");
    dictate.builtIn = true;
    dictate.network.speech = QStringLiteral("direct");
    settings.functions.append(dictate);

    FunctionSettings custom;
    custom.id = QStringLiteral("custom_1");
    custom.name = QStringLiteral("润色");
    custom.shortcut = QStringLiteral("Ctrl+Alt+1");
    custom.network.speech = QStringLiteral("proxy");
    settings.functions.append(custom);
    return settings;
}

} // namespace

class HistoryPageAccessFactoryTests : public QObject
{
    Q_OBJECT

private slots:
    void buildsHistorySettingsSnapshot();
    void forwardsFavoriteFolderAndSaveActions();
    void forwardsHistoryChangePublisher();
    void handlesMissingSettings();
    void contentControllerUsesIndependentFactory();
};

void HistoryPageAccessFactoryTests::buildsHistorySettingsSnapshot()
{
    const AppSettingsData source = sampleSettings();
    HubWindowAccess stateAccess;
    stateAccess.settingsSnapshotProvider = [source]() { return source; };
    HubSettingsState settings(stateAccess);

    const HistoryPageAccess access = createHistoryPageAccess(&settings);
    QVERIFY(access.snapshotProvider);
    const HistoryPageSettingsSnapshot snapshot = access.snapshotProvider();

    QCOMPARE(snapshot.recordDirectoryPath, QStringLiteral("C:/records"));
    QCOMPARE(snapshot.favoriteFolders, QStringList() << QStringLiteral("重要"));
    QCOMPARE(snapshot.initialLoadCount, 18);
    QCOMPARE(snapshot.loadMoreCount, 27);
    QCOMPARE(snapshot.speechProvider, QStringLiteral("xfyun"));
    QVERIFY(snapshot.useSystemProxy);
    QCOMPARE(snapshot.customFunctions.size(), 1);
    QCOMPARE(snapshot.customFunctions.first().id, QStringLiteral("custom_1"));
    QCOMPARE(
        snapshot.speechNetworkPolicies.value(QStringLiteral("dictate")),
        QStringLiteral("direct")
    );
    QCOMPARE(
        snapshot.speechNetworkPolicies.value(QStringLiteral("custom_1")),
        QStringLiteral("proxy")
    );
}

void HistoryPageAccessFactoryTests::forwardsFavoriteFolderAndSaveActions()
{
    AppSettingsData source = sampleSettings();
    int saveCount = 0;
    HubWindowAccess stateAccess;
    stateAccess.settingsSnapshotProvider = [source]() { return source; };
    stateAccess.applyAndSave = [&saveCount, &source](const AppSettingsData &data) {
        ++saveCount;
        source = data;
        return true;
    };
    HubSettingsState settings(stateAccess);

    const HistoryPageAccess access = createHistoryPageAccess(&settings);
    QVERIFY(access.addFavoriteFolder(QStringLiteral("工作")));
    QVERIFY(access.saveSettings());
    QCOMPARE(saveCount, 1);
    QVERIFY(source.favoriteFolders.contains(QStringLiteral("工作")));
}

void HistoryPageAccessFactoryTests::forwardsHistoryChangePublisher()
{
    AppSettingsData source = sampleSettings();
    HubWindowAccess stateAccess;
    stateAccess.settingsSnapshotProvider = [source]() { return source; };
    HubSettingsState settings(stateAccess);

    QStringList receivedIds;
    bool receivedReset = false;
    const HistoryPageAccess access = createHistoryPageAccess(
        &settings,
        [&receivedIds, &receivedReset](
            const QStringList &recordIds,
            bool resetRequired
        ) {
            receivedIds = recordIds;
            receivedReset = resetRequired;
        }
    );

    QVERIFY(access.historyChanged);
    access.historyChanged(
        QStringList() << QStringLiteral("records/detail.json"),
        true
    );
    QCOMPARE(
        receivedIds,
        QStringList() << QStringLiteral("records/detail.json")
    );
    QVERIFY(receivedReset);
}

void HistoryPageAccessFactoryTests::handlesMissingSettings()
{
    const HistoryPageAccess access = createHistoryPageAccess(nullptr);
    QVERIFY(access.snapshotProvider);
    QVERIFY(access.addFavoriteFolder);
    QVERIFY(access.saveSettings);
    QVERIFY(access.snapshotProvider().customFunctions.isEmpty());
    QVERIFY(!access.addFavoriteFolder(QStringLiteral("工作")));
    QVERIFY(!access.saveSettings());
}

void HistoryPageAccessFactoryTests::contentControllerUsesIndependentFactory()
{
    const QString sourcePath = QFINDTESTDATA(
        "../../src/ui/hub_content_pages_controller.cpp"
    );
    QVERIFY2(!sourcePath.isEmpty(), "找不到内容页面控制器源文件");
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();

    QVERIFY(contents.contains(
        "createHistoryPageAccess(access.settings, access.historyChanged)"
    ));
    QVERIFY(!contents.contains("HistoryPageAccess historyPageAccess() const"));
    QVERIFY(!contents.contains("HistoryPageAccess access;"));
}

QTEST_APPLESS_MAIN(HistoryPageAccessFactoryTests)

#include "history_page_access_factory_tests.moc"
