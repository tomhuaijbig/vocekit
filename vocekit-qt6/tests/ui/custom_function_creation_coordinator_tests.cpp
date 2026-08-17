#include <QtTest>

#include "../../src/ui/custom_function_creation_coordinator.h"
#include "../../src/ui/hub_settings_state.h"

class CustomFunctionCreationCoordinatorTests : public QObject
{
    Q_OBJECT

private slots:
    void createsAndReturnsPersistedFunction();
    void failedTransactionDoesNotMutateTheSettingsState();
    void ignoresMissingSettings();
};

void CustomFunctionCreationCoordinatorTests::createsAndReturnsPersistedFunction()
{
    AppSettingsData persisted;
    HubWindowAccess stateAccess;
    stateAccess.settingsSnapshotProvider = [&persisted]() {
        return persisted;
    };
    HubSettingsState settings(stateAccess);
    int transactions = 0;

    CustomFunctionCreationActions actions;
    actions.settings = &settings;
    actions.flows.addCustomFunction = [&](
        const FunctionSettings &function,
        OperationError *
    ) {
        ++transactions;
        persisted.functions.append(function);
        persisted.functionOrder.append(function.id);
        return true;
    };

    const QString id = createCustomFunction(actions);
    QCOMPARE(id, QStringLiteral("custom_1"));
    QCOMPARE(settings.customFunctions().size(), 1);
    QCOMPARE(transactions, 1);
    const CustomFunctionDef created = settings.customFunctions().first();
    QCOMPARE(created.id, QStringLiteral("custom_1"));
    QCOMPARE(created.name, QString::fromUtf8("自定义功能 1"));
    QCOMPARE(created.model, QStringLiteral("deepseek-v4-flash"));
    QVERIFY(created.useSelection);
    QVERIFY(created.useVoice);
    QVERIFY(!created.useScreenshot);
    QCOMPARE(
        created.prompt,
        QString::fromUtf8(
            "请根据选中文本和我的语音要求完成任务，输出可以直接使用的结果。"
        )
    );
}

void CustomFunctionCreationCoordinatorTests::
failedTransactionDoesNotMutateTheSettingsState()
{
    AppSettingsData persisted;
    HubWindowAccess stateAccess;
    stateAccess.settingsSnapshotProvider = [&persisted]() {
        return persisted;
    };
    HubSettingsState settings(stateAccess);
    CustomFunctionCreationActions actions;
    actions.settings = &settings;
    actions.flows.addCustomFunction = [](
        const FunctionSettings &,
        OperationError *error
    ) {
        error->code = QStringLiteral("flow_save_failed");
        return false;
    };
    OperationError error;
    QVERIFY(createCustomFunction(actions, &error).isEmpty());
    QVERIFY(settings.customFunctions().isEmpty());
    QVERIFY(persisted.functions.isEmpty());
    QCOMPARE(error.code, QStringLiteral("flow_save_failed"));
}

void CustomFunctionCreationCoordinatorTests::ignoresMissingSettings()
{
    CustomFunctionCreationActions actions;
    QVERIFY(createCustomFunction(actions).isEmpty());
}

QTEST_MAIN(CustomFunctionCreationCoordinatorTests)
#include "custom_function_creation_coordinator_tests.moc"
