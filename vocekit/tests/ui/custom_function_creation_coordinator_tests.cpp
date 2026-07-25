#include <QtTest>

#include "../../src/ui/custom_function_creation_coordinator.h"
#include "../../src/ui/hub_settings_state.h"

namespace {

HubSettingsState createSettings()
{
    HubWindowAccess access;
    access.settingsSnapshotProvider = []() {
        AppSettingsData data;
        return data;
    };
    return HubSettingsState(access);
}

} // namespace

class CustomFunctionCreationCoordinatorTests : public QObject
{
    Q_OBJECT

private slots:
    void createsAndKeepsAcceptedFunction();
    void rollsBackCancelledFunction();
    void ignoresMissingSettings();
};

void CustomFunctionCreationCoordinatorTests::createsAndKeepsAcceptedFunction()
{
    HubSettingsState settings = createSettings();
    int saves = 0;
    CustomFunctionDef edited;

    CustomFunctionCreationActions actions;
    actions.settings = &settings;
    actions.saveSettings = [&saves]() { ++saves; };
    actions.editFunction = [&edited](const CustomFunctionDef &function) {
        edited = function;
        return true;
    };

    QVERIFY(createAndEditCustomFunction(actions));
    QCOMPARE(settings.customFunctions().size(), 1);
    QCOMPARE(saves, 1);
    QCOMPARE(edited.id, QStringLiteral("custom_1"));
    QCOMPARE(edited.name, QString::fromUtf8("自定义功能 1"));
    QCOMPARE(edited.model, QStringLiteral("deepseek-v4-flash"));
    QVERIFY(edited.useSelection);
    QVERIFY(edited.useVoice);
    QVERIFY(!edited.useScreenshot);
    QCOMPARE(
        edited.prompt,
        QString::fromUtf8(
            "请根据选中文本和我的语音要求完成任务，输出可以直接使用的结果。"
        )
    );
}

void CustomFunctionCreationCoordinatorTests::rollsBackCancelledFunction()
{
    HubSettingsState settings = createSettings();
    int saves = 0;

    CustomFunctionCreationActions actions;
    actions.settings = &settings;
    actions.saveSettings = [&saves]() { ++saves; };
    actions.editFunction = [](const CustomFunctionDef &) { return false; };

    QVERIFY(!createAndEditCustomFunction(actions));
    QVERIFY(settings.customFunctions().isEmpty());
    QCOMPARE(saves, 2);
}

void CustomFunctionCreationCoordinatorTests::ignoresMissingSettings()
{
    int calls = 0;
    CustomFunctionCreationActions actions;
    actions.saveSettings = [&calls]() { ++calls; };
    actions.editFunction = [&calls](const CustomFunctionDef &) {
        ++calls;
        return true;
    };

    QVERIFY(!createAndEditCustomFunction(actions));
    QCOMPARE(calls, 0);
}

QTEST_MAIN(CustomFunctionCreationCoordinatorTests)
#include "custom_function_creation_coordinator_tests.moc"
