#include <QtTest>

#include "../../src/ui/hub_settings_state.h"
#include "../../src/ui/settings_panel_access_factory.h"
#include "../../src/ui/attention_message.h"
#include "../../src/ui/selection_context_settings_card.h"

#include <QFile>
#include <QCheckBox>
#include <QMessageBox>
#include <QSpinBox>

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
    void realSettingsPanelSubmitsWholeSelectionValueAndRollsBackFailure();
};

void SettingsPanelAccessFactoryTests::providesSnapshotAndPersistsSettings()
{
    AppSettingsData initial;
    initial.trayResident = false;
    initial.selectionContext.enabled = false;
    initial.selectionContext.minimumTextLength = 2;
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
    changed.selectionContext.enabled = true;
    changed.selectionContext.minimumTextLength = 19;
    QVERIFY(assembly.access.applyAndSave(changed));
    QVERIFY(saved.trayResident);
    QVERIFY(saved.selectionContext.enabled);
    QCOMPARE(saved.selectionContext.minimumTextLength, 19);
    QVERIFY(settings.toData().trayResident);
    QVERIFY(settings.selectionContextSettings().enabled);
}

void SettingsPanelAccessFactoryTests::preservesFailedSaveState()
{
    AppSettingsData initial;
    initial.resultPopupOpacity = 90;
    initial.selectionContext.enabled = false;
    initial.selectionContext.pauseMinutes = 30;

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
    changed.selectionContext.enabled = true;
    changed.selectionContext.pauseMinutes = 180;
    QVERIFY(!assembly.access.applyAndSave(changed));
    QCOMPARE(settings.toData().resultPopupOpacity, 90);
    QVERIFY(!settings.selectionContextSettings().enabled);
    QCOMPARE(settings.selectionContextSettings().pauseMinutes, 30);
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

void SettingsPanelAccessFactoryTests::
realSettingsPanelSubmitsWholeSelectionValueAndRollsBackFailure()
{
    AppSettingsData persisted;
    persisted.selectionContext.enabled = false;
    persisted.selectionContext.keyboardSelectionEnabled = true;
    persisted.selectionContext.minimumTextLength = 2;
    persisted.selectionContext.pauseMinutes = 30;
    persisted.selectionContext.blockedApplications = QStringList()
        << QStringLiteral("private.exe");
    AppSettingsData submitted;
    int saveCalls = 0;

    SettingsPanelAccess access;
    access.snapshotProvider = [&]() { return persisted; };
    access.applyAndSave = [&](const AppSettingsData &next) {
        submitted = next;
        ++saveCalls;
        return false;
    };
    setAttentionMessageBoxClickCallbackForTests([](QWidget *widget) {
        QMessageBox *box = qobject_cast<QMessageBox *>(widget);
        if (box) {
            box->done(QMessageBox::Ok);
        }
    });
    SettingsPanel panel(access, std::function<void()>());
    SelectionContextSettingsCard *card =
        panel.findChild<SelectionContextSettingsCard *>(
            QStringLiteral("selectionContextSettingsCard")
        );
    QVERIFY(card);
    QSpinBox *minimum = card->findChild<QSpinBox *>(
        QStringLiteral("selectionContextMinimumLengthSpin")
    );
    QVERIFY(minimum);
    minimum->setValue(41);

    QTRY_COMPARE(saveCalls, 1);
    QCOMPARE(submitted.selectionContext.minimumTextLength, 41);
    QVERIFY(submitted.selectionContext.keyboardSelectionEnabled);
    QCOMPARE(submitted.selectionContext.pauseMinutes, 30);
    QCOMPARE(
        submitted.selectionContext.blockedApplications,
        QStringList() << QStringLiteral("private.exe")
    );
    QCOMPARE(persisted.selectionContext.minimumTextLength, 2);
    QCOMPARE(minimum->value(), 2);
    QCOMPARE(card->settings().minimumTextLength, 2);
    setAttentionMessageBoxClickCallbackForTests(
        std::function<void(QWidget *)>()
    );
}

QTEST_MAIN(SettingsPanelAccessFactoryTests)

#include "settings_panel_access_factory_tests.moc"
