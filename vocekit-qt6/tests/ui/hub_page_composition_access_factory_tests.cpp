#include <QtTest>

#include "../../src/ui/hub_page_composition_access_factory.h"

#include <QFile>
#include <QWidget>

class HubPageCompositionAccessFactoryTests : public QObject
{
    Q_OBJECT

private slots:
    void preservesPageFactories();
    void routesImmediateActivationActions();
    void defersHistoryRefresh();
    void handlesMissingDependencies();
    void pageHostUsesIndependentFactory();
};

void HubPageCompositionAccessFactoryTests::preservesPageFactories()
{
    QWidget home;
    QWidget settings;
    HubPageCompositionAccessFactoryDependencies dependencies;
    dependencies.homePage = [&home]() { return &home; };
    dependencies.settingsPage = [&settings]() { return &settings; };

    const HubPageCompositionAccess access =
        createHubPageCompositionAccess(dependencies);

    QVERIFY(access.homePage);
    QVERIFY(access.settingsPage);
    QCOMPARE(access.homePage(), &home);
    QCOMPARE(access.settingsPage(), &settings);
    QVERIFY(!access.functionPage);
}

void HubPageCompositionAccessFactoryTests::routesImmediateActivationActions()
{
    int vocabularyRefreshes = 0;
    int ocrRefreshes = 0;
    int promptRefreshes = 0;
    int diagnosticsRefreshes = 0;
    int logRefreshes = 0;
    int settingsRefreshes = 0;

    HubPageCompositionAccessFactoryDependencies dependencies;
    dependencies.refreshVocabulary = [&vocabularyRefreshes]() {
        ++vocabularyRefreshes;
    };
    dependencies.refreshOcr = [&ocrRefreshes]() { ++ocrRefreshes; };
    dependencies.refreshPrompts = [&promptRefreshes]() { ++promptRefreshes; };
    dependencies.refreshDiagnostics = [&diagnosticsRefreshes]() {
        ++diagnosticsRefreshes;
    };
    dependencies.refreshLogs = [&logRefreshes]() { ++logRefreshes; };
    dependencies.refreshSettings = [&settingsRefreshes]() {
        ++settingsRefreshes;
    };

    const HubPageCompositionAccess access =
        createHubPageCompositionAccess(dependencies);
    access.vocabularyActivated(false);
    access.ocrActivated(false);
    access.promptsActivated(false);
    access.diagnosticsActivated(false);
    access.logsActivated(false);
    access.settingsActivated(false);
    access.settingsActivated(true);

    QCOMPARE(vocabularyRefreshes, 1);
    QCOMPARE(ocrRefreshes, 1);
    QCOMPARE(promptRefreshes, 1);
    QCOMPARE(diagnosticsRefreshes, 1);
    QCOMPARE(logRefreshes, 1);
    QCOMPARE(settingsRefreshes, 1);
}

void HubPageCompositionAccessFactoryTests::defersHistoryRefresh()
{
    QObject context;
    int refreshes = 0;
    HubPageCompositionAccessFactoryDependencies dependencies;
    dependencies.deferredContext = &context;
    dependencies.refreshHistory = [&refreshes]() { ++refreshes; };

    const HubPageCompositionAccess access =
        createHubPageCompositionAccess(dependencies);
    access.historyActivated(true);

    QCOMPARE(refreshes, 0);
    QTRY_COMPARE(refreshes, 1);
}

void HubPageCompositionAccessFactoryTests::handlesMissingDependencies()
{
    const HubPageCompositionAccess access =
        createHubPageCompositionAccess(
            HubPageCompositionAccessFactoryDependencies()
        );

    QVERIFY(access.historyActivated);
    QVERIFY(access.vocabularyActivated);
    QVERIFY(access.ocrActivated);
    QVERIFY(access.promptsActivated);
    QVERIFY(access.diagnosticsActivated);
    QVERIFY(access.logsActivated);
    QVERIFY(access.settingsActivated);
    access.historyActivated(false);
    access.vocabularyActivated(false);
    access.ocrActivated(false);
    access.promptsActivated(false);
    access.diagnosticsActivated(false);
    access.logsActivated(false);
    access.settingsActivated(false);
}

void HubPageCompositionAccessFactoryTests::pageHostUsesIndependentFactory()
{
    const QString sourcePath = QFINDTESTDATA(
        "../../src/ui/hub_page_host_controller.cpp"
    );
    QVERIFY2(!sourcePath.isEmpty(), "Page host source file not found");
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();

    QVERIFY(contents.contains("createHubPageCompositionAccess(access)"));
    QVERIFY(contents.contains("HubPageComposition::create"));
    QVERIFY(!contents.contains("HubPageCompositionAccess access;"));
    QVERIFY(!contents.contains("access.historyActivated ="));
}

QTEST_MAIN(HubPageCompositionAccessFactoryTests)

#include "hub_page_composition_access_factory_tests.moc"
