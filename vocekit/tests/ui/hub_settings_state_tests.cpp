#include <QtTest>

#include "../../src/ui/hub_settings_state.h"

class HubSettingsStateTests : public QObject
{
    Q_OBJECT

private slots:
    void readsAndSavesTypedSettings();
    void editsCustomFunctionsWithoutLegacySettings();
    void savesPromptLibraryThroughCallback();
    void savesFunctionScopedRecordingAndResultSettings();
    void normalizesFunctionNetworkPolicies();
};

void HubSettingsStateTests::readsAndSavesTypedSettings()
{
    AppSettingsData source;
    FunctionSettings dictate;
    dictate.id = QStringLiteral("dictate");
    dictate.name = QStringLiteral("听写");
    dictate.builtIn = true;
    dictate.shortcut = QStringLiteral("Alt+X");
    dictate.modelId = QStringLiteral("deepseek-v4-flash");
    source.functions.append(dictate);

    AppSettingsData saved;
    HubWindowAccess access;
    access.settingsSnapshotProvider = [source]() { return source; };
    access.applyAndSave = [&saved](const AppSettingsData &data) {
        saved = data;
        return true;
    };

    HubSettingsState state(access);
    QCOMPARE(state.hotkey(QStringLiteral("dictate")), QStringLiteral("Alt+X"));
    state.setModelFor(QStringLiteral("dictate"), QStringLiteral("deepseek-v4-pro"));
    QVERIFY(state.save());
    QCOMPARE(
        saved.function(QStringLiteral("dictate")).modelId,
        QStringLiteral("deepseek-v4-pro")
    );
}

void HubSettingsStateTests::editsCustomFunctionsWithoutLegacySettings()
{
    HubWindowAccess access;
    access.settingsSnapshotProvider = []() { return AppSettingsData(); };
    HubSettingsState state(access);

    CustomFunctionDef function;
    function.id = QStringLiteral("custom_1");
    function.name = QStringLiteral("润色");
    function.shortcut = QStringLiteral("Ctrl+Alt+1");
    function.model = QStringLiteral("deepseek-v4-flash");
    function.useSelection = true;
    function.promptId = function.id;
    state.addCustomFunction(function);

    QCOMPARE(state.customFunctions().size(), 1);
    QCOMPARE(state.toData().function(QStringLiteral("custom_1")).name, QStringLiteral("润色"));
    QCOMPARE(state.nextCustomFunctionId(), QStringLiteral("custom_2"));
}

void HubSettingsStateTests::savesPromptLibraryThroughCallback()
{
    QVector<PromptLibraryItem> saved;
    HubWindowAccess access;
    access.settingsSnapshotProvider = []() { return AppSettingsData(); };
    access.promptLibraryProvider = []() { return QVector<PromptLibraryItem>(); };
    access.savePromptLibrary = [&saved](const QVector<PromptLibraryItem> &items) {
        saved = items;
        return true;
    };

    HubSettingsState state(access);
    PromptLibraryItem item;
    item.name = QStringLiteral("正式语气");
    item.content = QStringLiteral("使用正式语气改写。");
    state.addPromptLibraryItem(item);

    QVERIFY(state.savePromptLibrary());
    QCOMPARE(saved.size(), 1);
    QCOMPARE(saved.first().id, QStringLiteral("prompt_1"));
}

void HubSettingsStateTests::savesFunctionScopedRecordingAndResultSettings()
{
    AppSettingsData saved;
    HubWindowAccess access;
    access.settingsSnapshotProvider = []() { return AppSettingsData(); };
    access.applyAndSave = [&saved](const AppSettingsData &data) {
        saved = data;
        return true;
    };

    HubSettingsState state(access);
    const QString functionId = QStringLiteral("custom_1");
    state.setRecordingBeepEnabledFor(functionId, false);
    state.setRecordingBeepPathFor(
        functionId,
        QStringLiteral("  C:/sounds/start.wav  ")
    );
    state.setLongRecordingEnabledFor(functionId, true);
    state.setSegmentSecondsFor(functionId, 42);
    state.setResultActionsFor(
        functionId,
        QStringList()
            << QStringLiteral("copy")
            << QStringLiteral("copy")
            << QStringLiteral("unsupported")
            << QStringLiteral("write")
    );

    QVERIFY(state.save());
    QVERIFY(!state.recordingBeepEnabledFor(functionId));
    QCOMPARE(
        state.recordingBeepPathFor(functionId),
        QStringLiteral("C:/sounds/start.wav")
    );
    QVERIFY(state.longRecordingEnabledFor(functionId));
    QCOMPARE(state.segmentSecondsFor(functionId), 42);
    QCOMPARE(
        state.resultActionsFor(functionId),
        QStringList() << QStringLiteral("copy") << QStringLiteral("write")
    );

    const FunctionSettings &function = saved.function(functionId);
    QVERIFY(!function.recording.beepEnabled);
    QCOMPARE(function.recording.beepPath, QStringLiteral("C:/sounds/start.wav"));
    QVERIFY(function.recording.longRecordingEnabled);
    QCOMPARE(function.recording.segmentSeconds, 42);
    QCOMPARE(
        function.output.resultActions,
        QStringList() << QStringLiteral("copy") << QStringLiteral("write")
    );
}

void HubSettingsStateTests::normalizesFunctionNetworkPolicies()
{
    HubWindowAccess access;
    access.settingsSnapshotProvider = []() { return AppSettingsData(); };
    HubSettingsState state(access);

    FunctionNetworkPolicies policies;
    policies.speech = QStringLiteral(" direct ");
    policies.ocr = QStringLiteral("unsupported");
    policies.model = QStringLiteral("systemProxy");
    state.setNetworkPoliciesFor(QStringLiteral("dictate"), policies);

    const FunctionNetworkPolicies saved =
        state.networkPoliciesFor(QStringLiteral("dictate"));
    QCOMPARE(saved.speech, QStringLiteral("direct"));
    QCOMPARE(saved.ocr, QStringLiteral("inherit"));
    QCOMPARE(saved.model, QStringLiteral("systemProxy"));
}

QTEST_APPLESS_MAIN(HubSettingsStateTests)

#include "hub_settings_state_tests.moc"
