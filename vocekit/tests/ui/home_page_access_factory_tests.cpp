#include <QtTest>

#include "../../src/ui/home_page_access_factory.h"
#include "../../src/ui/hub_settings_state.h"

#include <QFile>

namespace {

AppSettingsData sampleSettings()
{
    AppSettingsData settings;
    settings.autoStartEnabled = true;
    settings.strongSelectionEnabled = true;
    settings.speechProvider = QStringLiteral("xfyun");

    FunctionSettings dictate;
    dictate.id = QStringLiteral("dictate");
    dictate.name = QStringLiteral("听写");
    dictate.builtIn = true;
    dictate.shortcut = QStringLiteral("Alt+X");
    dictate.modelId = QStringLiteral("test-model");
    dictate.input.useVoice = true;
    settings.functions.append(dictate);
    settings.functionOrder.append(dictate.id);
    return settings;
}

} // namespace

class HomePageAccessFactoryTests : public QObject
{
    Q_OBJECT

private slots:
    void buildsTypedHomePageAccess();
    void forwardsPageActions();
    void handlesMissingSettingsAndProviders();
    void hubWindowUsesIndependentFactory();
};

void HomePageAccessFactoryTests::buildsTypedHomePageAccess()
{
    const AppSettingsData source = sampleSettings();
    HubWindowAccess stateAccess;
    stateAccess.settingsSnapshotProvider = [source]() { return source; };
    HubSettingsState settings(stateAccess);

    HomePageAccessDependencies dependencies;
    dependencies.settings = &settings;
    dependencies.recentEntries = []() {
        return QVector<HistoryEntry>() << HistoryEntry();
    };
    dependencies.historyTabs = []() {
        HistoryTabDef all;
        all.id = QStringLiteral("__all");
        all.title = QStringLiteral("全部");
        HistoryTabDef custom;
        custom.id = QStringLiteral("custom_1");
        custom.title = QStringLiteral("润色");
        return QVector<HistoryTabDef>() << all << custom;
    };

    const HomePageAccess access = createHomePageAccess(dependencies);
    QVERIFY(access.functionModes.snapshotProvider);
    QCOMPARE(access.functionModes.snapshotProvider().cards.size(), 3);
    QCOMPARE(access.recentEntries().size(), 1);

    const QVector<RecentHistoryPanel::TabSpec> tabs = access.recentTabs();
    QCOMPARE(tabs.size(), 2);
    QCOMPARE(tabs.at(0).id, QStringLiteral("__all"));
    QCOMPARE(tabs.at(0).title, QStringLiteral("全部"));
    QCOMPARE(tabs.at(1).id, QStringLiteral("custom_1"));
    QCOMPARE(tabs.at(1).title, QStringLiteral("润色"));

    const CurrentStatusSnapshot status = access.currentStatus();
    QVERIFY(status.autoStartEnabled);
    QVERIFY(status.strongSelectionEnabled);
    QCOMPARE(status.speechProvider, QStringLiteral("xfyun"));
}

void HomePageAccessFactoryTests::forwardsPageActions()
{
    bool edited = false;
    bool settingsChanged = false;
    bool warned = false;
    bool listRequested = false;

    HomePageAccessDependencies dependencies;
    dependencies.editFunction = [&edited](
        const QString &,
        const QString &,
        bool,
        const CustomFunctionDef &
    ) {
        edited = true;
    };
    dependencies.settingsChanged = [&settingsChanged]() {
        settingsChanged = true;
    };
    dependencies.showWarning = [&warned](const QString &, const QString &) {
        warned = true;
    };
    dependencies.recentListFactory = [&listRequested](
        const QString &,
        const QVector<HistoryEntry> &,
        int
    ) -> QWidget * {
        listRequested = true;
        return nullptr;
    };

    const HomePageAccess access = createHomePageAccess(dependencies);
    access.editFunction(QString(), QString(), false, CustomFunctionDef());
    access.settingsChanged();
    access.showWarning(QString(), QString());
    access.recentListFactory(QString(), QVector<HistoryEntry>(), 0);

    QVERIFY(edited);
    QVERIFY(settingsChanged);
    QVERIFY(warned);
    QVERIFY(listRequested);
}

void HomePageAccessFactoryTests::handlesMissingSettingsAndProviders()
{
    const HomePageAccess access =
        createHomePageAccess(HomePageAccessDependencies());
    QVERIFY(access.functionModes.snapshotProvider);
    QVERIFY(access.recentEntries);
    QVERIFY(access.recentTabs);
    QVERIFY(access.currentStatus);
    QVERIFY(access.functionModes.snapshotProvider().cards.isEmpty());
    QVERIFY(access.recentEntries().isEmpty());
    QVERIFY(access.recentTabs().isEmpty());
    QVERIFY(!access.currentStatus().autoStartEnabled);
}

void HomePageAccessFactoryTests::hubWindowUsesIndependentFactory()
{
    const QString sourcePath = QFINDTESTDATA("../../src/ui/hub_window.cpp");
    QVERIFY2(!sourcePath.isEmpty(), "找不到 HubWindow 源文件");
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();

    QVERIFY(contents.contains("createHomePageAccess(dependencies)"));
    QVERIFY(!contents.contains("HomePageAccess access"));
    QVERIFY(!contents.contains("RecentHistoryPanel::TabSpec tab"));
}

QTEST_APPLESS_MAIN(HomePageAccessFactoryTests)

#include "home_page_access_factory_tests.moc"
