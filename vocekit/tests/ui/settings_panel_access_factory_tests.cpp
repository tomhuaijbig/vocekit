#include <QtTest>

#include "../../src/ui/hub_settings_state.h"
#include "../../src/ui/settings_panel_access_factory.h"

#include <QFile>

class SettingsPanelAccessFactoryTests : public QObject
{
    Q_OBJECT

private slots:
    void providesSnapshotAndPersistsSettings();
    void preservesFailedSaveState();
    void publishesOneSettingsChange();
    void handlesMissingDependencies();
    void utilityControllerUsesIndependentFactory();
    void forwardsFloatingBarPreviewCallback();
};

void SettingsPanelAccessFactoryTests::providesSnapshotAndPersistsSettings()
{
    AppSettingsData initial;
    initial.trayResident = false;
    AppSettingsData saved;

    HubWindowAccess settingsAccess;
    settingsAccess.settingsSnapshotProvider = [initial]() {
        return initial;
    };
    settingsAccess.applyAndSave = [&saved](const AppSettingsData &settings) {
        saved = settings;
        return true;
    };
    HubSettingsState settings(settingsAccess);

    SettingsPanelAccessFactoryDependencies dependencies;
    dependencies.settings = &settings;
    const SettingsPanelAssembly assembly =
        createSettingsPanelAssembly(dependencies);

    QVERIFY(assembly.access.snapshotProvider);
    QVERIFY(!assembly.access.snapshotProvider().trayResident);
    QVERIFY(assembly.access.applyAndSave);

    AppSettingsData changed = initial;
    changed.trayResident = true;
    QVERIFY(assembly.access.applyAndSave(changed));
    QVERIFY(saved.trayResident);
    QVERIFY(settings.toData().trayResident);
}

void SettingsPanelAccessFactoryTests::preservesFailedSaveState()
{
    AppSettingsData initial;
    initial.resultPopupOpacity = 90;

    HubWindowAccess settingsAccess;
    settingsAccess.settingsSnapshotProvider = [initial]() {
        return initial;
    };
    settingsAccess.applyAndSave = [](const AppSettingsData &) {
        return false;
    };
    HubSettingsState settings(settingsAccess);

    SettingsPanelAccessFactoryDependencies dependencies;
    dependencies.settings = &settings;
    const SettingsPanelAssembly assembly =
        createSettingsPanelAssembly(dependencies);

    AppSettingsData changed = initial;
    changed.resultPopupOpacity = 65;
    QVERIFY(!assembly.access.applyAndSave(changed));
    QCOMPARE(settings.toData().resultPopupOpacity, 90);
}

void SettingsPanelAccessFactoryTests::publishesOneSettingsChange()
{
    QStringList actions;
    SettingsPanelAccessFactoryDependencies dependencies;
    dependencies.notifySettingsChanged = [&actions]() {
        actions.append(QStringLiteral("notify"));
    };

    const SettingsPanelAssembly assembly =
        createSettingsPanelAssembly(dependencies);
    QVERIFY(assembly.onChanged);
    assembly.onChanged();

    QCOMPARE(
        actions,
        QStringList() << QStringLiteral("notify")
    );

    const QString sourcePath = QFINDTESTDATA(
        "../../src/ui/settings_panel_access_factory.cpp"
    );
    QVERIFY2(!sourcePath.isEmpty(), "找不到设置页访问工厂源文件");
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::ReadOnly));
    QVERIFY(!source.readAll().contains("applySettingsChanged"));
}

void SettingsPanelAccessFactoryTests::handlesMissingDependencies()
{
    const SettingsPanelAssembly assembly = createSettingsPanelAssembly(
        SettingsPanelAccessFactoryDependencies()
    );

    QVERIFY(assembly.access.snapshotProvider);
    QCOMPARE(assembly.access.snapshotProvider().trayResident, true);
    QVERIFY(assembly.access.applyAndSave);
    QVERIFY(!assembly.access.applyAndSave(AppSettingsData()));
    QVERIFY(assembly.onChanged);
    assembly.onChanged();
}

void SettingsPanelAccessFactoryTests::utilityControllerUsesIndependentFactory()
{
    const QString sourcePath = QFINDTESTDATA(
        "../../src/ui/hub_utility_pages_controller.cpp"
    );
    QVERIFY2(!sourcePath.isEmpty(), "找不到辅助页面控制器源文件");
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();

    QVERIFY(contents.contains("createSettingsPanelAssembly(dependencies)"));
    QVERIFY(!contents.contains("SettingsPanelAccess access;"));
}

void SettingsPanelAccessFactoryTests::forwardsFloatingBarPreviewCallback()
{
    QStringList styles;
    SettingsPanelAccessFactoryDependencies dependencies;
    dependencies.previewFloatingBarStyle = [&](const QString &style) {
        styles.append(style);
    };
    const SettingsPanelAssembly assembly =
        createSettingsPanelAssembly(dependencies);
    QVERIFY(assembly.access.previewFloatingBarStyle);
    assembly.access.previewFloatingBarStyle(QStringLiteral("statusPill"));
    QCOMPARE(styles, QStringList() << QStringLiteral("statusPill"));
}

QTEST_APPLESS_MAIN(SettingsPanelAccessFactoryTests)

#include "settings_panel_access_factory_tests.moc"
