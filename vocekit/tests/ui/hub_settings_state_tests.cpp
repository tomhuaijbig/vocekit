#include <QtTest>

#include "../../src/domain/selection_context_actions.h"
#include "../../src/ui/hub_settings_state.h"

class HubSettingsStateTests : public QObject
{
    Q_OBJECT

private slots:
    void readsAndNormalizesSelectionContextSettings();
    void normalizesSelectionActionCustomizationsWithCompleteSettings();
    void deletedVocabularyScopeFallsBackWithoutChangingUnknownModel();
    void writableVocabularyScopeCatalogUsesFixedThenCustomOrder();
    void completeNormalizerMigratesModelsAndPreservesOtherFields();
    void replaceAndSaveNormalizesCompleteSelectionContextSettings();
    void saveNormalizesScopeAfterCustomFunctionRemoval();
    void reloadFunctionFlowStateSynchronizesOnlyModeAndFlow();
    void reloadFunctionFlowStateRejectsMissingFunctions();
    void readsAndSavesTypedSettings();
    void editsCustomFunctionsWithoutLegacySettings();
    void savesPromptLibraryThroughCallback();
    void savesFunctionScopedRecordingAndResultSettings();
    void normalizesFunctionNetworkPolicies();
    void savesFunctionScopedInputOrder();
    void savesFunctionScopedOutputOrder();
    void prefersNonFlowSaveAndReportsStructuredErrors();
    void updatingCustomFunctionPreservesItsEntireFlowState();
    void preservesAndNormalizesCustomFloatingBarStyleOverride();
    void nextCustomFunctionIdDoesNotReuseOrphanOrOrderedIds();
    void staleReplaceReloadsLatestStateWithoutReplayingEdits();
    void normalizesWindowsSpeechLanguage();
};

void HubSettingsStateTests::
normalizesSelectionActionCustomizationsWithCompleteSettings()
{
    AppSettingsData source;
    FunctionSettings custom;
    custom.id = QStringLiteral("custom-summarize");
    custom.name = QString::fromUtf8("摘要");
    custom.builtIn = false;
    source.functions.append(custom);

    for (const QString &id : defaultSelectionContextActionOrder()) {
        SelectionContextActionCustomization item =
            source.selectionContext.actionCustomizations.value(id);
        item.visible = false;
        source.selectionContext.actionCustomizations.insert(id, item);
    }
    source.selectionContext.actionOrder = QStringList()
        << selectionContextActionCopy()
        << selectionContextActionTranslate()
        << selectionContextActionExplain()
        << selectionContextActionSave()
        << selectionContextActionAiSearch();
    SelectionContextActionCustomization save =
        source.selectionContext.actionCustomizations.value(
            selectionContextActionSave());
    save.vocabularyScopeId = custom.id;
    source.selectionContext.actionCustomizations.insert(
        selectionContextActionSave(), save);

    HubWindowAccess access;
    access.settingsSnapshotProvider = [source]() { return source; };
    HubSettingsState state(access);
    const SelectionContextSettings normalized =
        state.selectionContextSettings();
    QCOMPARE(normalized.actionOrder.first(),
             selectionContextActionCopy());
    QVERIFY(normalized.actionCustomizations.value(
        normalized.actionOrder.first()).visible);
    QCOMPARE(normalized.actionCustomizations.value(
        selectionContextActionSave()).vocabularyScopeId, custom.id);
}

void HubSettingsStateTests::
deletedVocabularyScopeFallsBackWithoutChangingUnknownModel()
{
    AppSettingsData source;
    SelectionContextActionCustomization save =
        source.selectionContext.actionCustomizations.value(
            selectionContextActionSave());
    save.vocabularyScopeId = QStringLiteral("deleted-function");
    source.selectionContext.actionCustomizations.insert(
        selectionContextActionSave(), save);
    SelectionContextActionCustomization explain =
        source.selectionContext.actionCustomizations.value(
            selectionContextActionExplain());
    explain.modelId = QStringLiteral("openai:future");
    source.selectionContext.actionCustomizations.insert(
        selectionContextActionExplain(), explain);

    HubWindowAccess access;
    access.settingsSnapshotProvider = [source]() { return source; };
    HubSettingsState state(access);
    const SelectionContextSettings normalized =
        state.selectionContextSettings();
    QCOMPARE(normalized.actionCustomizations.value(
        selectionContextActionSave()).vocabularyScopeId,
        QStringLiteral("__global"));
    QCOMPARE(normalized.actionCustomizations.value(
        selectionContextActionExplain()).modelId,
        QStringLiteral("openai:future"));
}

void HubSettingsStateTests::
writableVocabularyScopeCatalogUsesFixedThenCustomOrder()
{
    AppSettingsData source;
    const auto appendFunction = [&source](
        const QString &id,
        bool builtIn = false
    ) {
        FunctionSettings function;
        function.id = id;
        function.name = id;
        function.builtIn = builtIn;
        source.functions.append(function);
    };
    appendFunction(QStringLiteral(" custom-b "));
    appendFunction(QStringLiteral("custom-a"));
    appendFunction(QStringLiteral("custom-b"));
    appendFunction(QStringLiteral("custom-built-in"), true);
    appendFunction(QStringLiteral("   "));
    appendFunction(QStringLiteral("__all"));
    appendFunction(QStringLiteral("dictate"));

    QCOMPARE(writableSelectionContextVocabularyScopeIds(source),
             QStringList()
                 << QStringLiteral("__global")
                 << QStringLiteral("dictate")
                 << QStringLiteral("translate")
                 << QStringLiteral("ask")
                 << QStringLiteral("custom-b")
                 << QStringLiteral("custom-a"));
}

void HubSettingsStateTests::
completeNormalizerMigratesModelsAndPreservesOtherFields()
{
    AppSettingsData completeSettings;
    SelectionContextSettings source;
    source.enabled = true;
    source.keyboardSelectionEnabled = false;
    source.closeOnOutsideClick = false;
    source.pinEnabled = false;
    source.networkConsentAcknowledged = true;
    source.minimumTextLength = 777;
    source.pauseMinutes = 999;
    source.blockedApplications = QStringList()
        << QStringLiteral(" C:/Apps/Future.EXE ")
        << QStringLiteral("Future.EXE");
    source.actionOrder = QStringList()
        << selectionContextActionTranslate()
        << selectionContextActionTranslate()
        << QStringLiteral("future-action");

    const QStringList ids = defaultSelectionContextActionOrder();
    const QStringList models = QStringList()
        << QStringLiteral("gpt-4o")
        << QStringLiteral("openai:gpt-5.5")
        << QStringLiteral("claude:claude-3-7-sonnet")
        << QStringLiteral("openai:future")
        << QStringLiteral(" custom:future ");
    for (int index = 0; index < ids.size(); ++index) {
        SelectionContextActionCustomization item =
            source.actionCustomizations.value(ids.at(index));
        item.modelId = models.at(index);
        source.actionCustomizations.insert(ids.at(index), item);
    }

    const SelectionContextSettings normalized =
        normalizeSelectionContextSettings(source, completeSettings);
    QVERIFY(normalized.enabled);
    QVERIFY(!normalized.keyboardSelectionEnabled);
    QVERIFY(!normalized.closeOnOutsideClick);
    QVERIFY(!normalized.pinEnabled);
    QVERIFY(normalized.networkConsentAcknowledged);
    QCOMPARE(normalized.minimumTextLength, 777);
    QCOMPARE(normalized.pauseMinutes, 999);
    QCOMPARE(normalized.blockedApplications,
             source.blockedApplications);
    QCOMPARE(normalized.actionOrder.first(),
             selectionContextActionTranslate());
    QCOMPARE(normalized.actionCustomizations.value(
        selectionContextActionAiSearch()).modelId,
        QStringLiteral("openai:gpt-5.6-terra"));
    QCOMPARE(normalized.actionCustomizations.value(
        selectionContextActionTranslate()).modelId,
        QStringLiteral("openai:gpt-5.6-sol"));
    QCOMPARE(normalized.actionCustomizations.value(
        selectionContextActionExplain()).modelId,
        QStringLiteral("claude:claude-sonnet-5"));
    QCOMPARE(normalized.actionCustomizations.value(
        selectionContextActionSave()).modelId,
        QStringLiteral("openai:future"));
    QCOMPARE(normalized.actionCustomizations.value(
        selectionContextActionCopy()).modelId,
        QStringLiteral("custom:future"));
}

void HubSettingsStateTests::
replaceAndSaveNormalizesCompleteSelectionContextSettings()
{
    AppSettingsData persisted;
    bool callbackInvoked = false;
    HubWindowAccess access;
    access.settingsSnapshotProvider = [persisted]() {
        return persisted;
    };
    access.applyNonFlowAndSave = [&persisted, &callbackInvoked](
        const AppSettingsData &edited,
        OperationError *
    ) {
        callbackInvoked = true;
        persisted = edited;
        return true;
    };
    HubSettingsState state(access);

    AppSettingsData replacement = state.toData();
    replacement.selectionContext.actionOrder = QStringList()
        << selectionContextActionCopy();
    for (const QString &id : defaultSelectionContextActionOrder()) {
        SelectionContextActionCustomization item =
            replacement.selectionContext.actionCustomizations.value(id);
        item.visible = false;
        replacement.selectionContext.actionCustomizations.insert(id, item);
    }
    SelectionContextActionCustomization save =
        replacement.selectionContext.actionCustomizations.value(
            selectionContextActionSave());
    save.vocabularyScopeId = QStringLiteral("missing-custom");
    replacement.selectionContext.actionCustomizations.insert(
        selectionContextActionSave(), save);

    QVERIFY(state.replaceAndSave(replacement));
    QVERIFY(callbackInvoked);
    QCOMPARE(persisted.selectionContext.actionOrder.first(),
             selectionContextActionCopy());
    QVERIFY(persisted.selectionContext.actionCustomizations.value(
        selectionContextActionCopy()).visible);
    QCOMPARE(persisted.selectionContext.actionCustomizations.value(
        selectionContextActionSave()).vocabularyScopeId,
        QStringLiteral("__global"));
    QCOMPARE(state.toData().selectionContext.actionCustomizations.value(
        selectionContextActionSave()).vocabularyScopeId,
        QStringLiteral("__global"));
}

void HubSettingsStateTests::
saveNormalizesScopeAfterCustomFunctionRemoval()
{
    AppSettingsData source;
    FunctionSettings custom;
    custom.id = QStringLiteral("custom-scope");
    custom.name = QString::fromUtf8("临时词库范围");
    custom.builtIn = false;
    source.functions.append(custom);
    SelectionContextActionCustomization save =
        source.selectionContext.actionCustomizations.value(
            selectionContextActionSave());
    save.vocabularyScopeId = custom.id;
    source.selectionContext.actionCustomizations.insert(
        selectionContextActionSave(), save);

    AppSettingsData persisted;
    HubWindowAccess access;
    access.settingsSnapshotProvider = [source]() { return source; };
    access.applyNonFlowAndSave = [&persisted](
        const AppSettingsData &edited,
        OperationError *
    ) {
        persisted = edited;
        return true;
    };
    HubSettingsState state(access);
    state.removeCustomFunction(custom.id);

    QVERIFY(state.save());
    QCOMPARE(persisted.selectionContext.actionCustomizations.value(
        selectionContextActionSave()).vocabularyScopeId,
        QStringLiteral("__global"));

    CustomFunctionDef recreated;
    recreated.id = custom.id;
    recreated.name = QString::fromUtf8("重建词库范围");
    state.addCustomFunction(recreated);
    QCOMPARE(state.selectionContextSettings().actionCustomizations.value(
        selectionContextActionSave()).vocabularyScopeId,
        QStringLiteral("__global"));

    QVERIFY(state.save());
    QCOMPARE(persisted.selectionContext.actionCustomizations.value(
        selectionContextActionSave()).vocabularyScopeId,
        QStringLiteral("__global"));
}

void HubSettingsStateTests::readsAndNormalizesSelectionContextSettings()
{
    AppSettingsData source;
    source.selectionContext.enabled = true;
    source.selectionContext.minimumTextLength = 0;
    source.selectionContext.pauseMinutes = 9000;
    source.selectionContext.actionOrder = QStringList()
        << QStringLiteral("copy")
        << QStringLiteral("copy")
        << QStringLiteral("unknown");
    source.selectionContext.blockedApplications = QStringList()
        << QStringLiteral("C:/Program Files/Browser/Chrome.EXE")
        << QStringLiteral("chrome.exe")
        << QStringLiteral("WORD.EXE");
    HubWindowAccess access;
    access.settingsSnapshotProvider = [source]() { return source; };
    HubSettingsState state(access);

    SelectionContextSettings normalized = state.selectionContextSettings();
    QVERIFY(normalized.enabled);
    QCOMPARE(normalized.minimumTextLength, 1);
    QCOMPARE(normalized.pauseMinutes, 1440);
    QCOMPARE(normalized.actionOrder, QStringList()
        << QStringLiteral("copy")
        << QStringLiteral("ai-search")
        << QStringLiteral("translate")
        << QStringLiteral("explain")
        << QStringLiteral("save"));
    QCOMPARE(normalized.blockedApplications, QStringList()
        << QStringLiteral("chrome.exe")
        << QStringLiteral("word.exe"));

    normalized.keyboardSelectionEnabled = false;
    state.setSelectionContextSettings(normalized);
    QVERIFY(!state.selectionContextSettings().keyboardSelectionEnabled);
    QCOMPARE(state.toData().selectionContext.minimumTextLength, 1);
}

void HubSettingsStateTests::normalizesWindowsSpeechLanguage()
{
    AppSettingsData source;
    source.windowsSpeechLanguage = QStringLiteral(" EN-us ");
    HubWindowAccess access;
    access.settingsSnapshotProvider = [source]() { return source; };
    HubSettingsState state(access);
    QCOMPARE(state.windowsSpeechLanguage(), QStringLiteral("en-US"));
    state.setWindowsSpeechLanguage(QStringLiteral(" ZH-cn "));
    QCOMPARE(state.windowsSpeechLanguage(), QStringLiteral("zh-CN"));
    state.setWindowsSpeechLanguage(QStringLiteral("legacy-value"));
    QCOMPARE(state.windowsSpeechLanguage(), QStringLiteral("follow-windows"));
}

void HubSettingsStateTests::
reloadFunctionFlowStateSynchronizesOnlyModeAndFlow()
{
    AppSettingsData persisted;
    FunctionSettings function;
    function.id = QStringLiteral("custom_1");
    function.name = QStringLiteral("本地名称");
    function.shortcut = QStringLiteral("Ctrl+Alt+1");
    persisted.functions.append(normalizeFunctionSettings(function));
    FunctionSettings unrelated;
    unrelated.id = QStringLiteral("custom_2");
    unrelated.name = QStringLiteral("无关功能");
    unrelated.flow.draft.revision = 99;
    unrelated = normalizeFunctionSettings(unrelated);
    persisted.functions.append(unrelated);

    HubWindowAccess access;
    access.settingsSnapshotProvider = [&persisted]() {
        return persisted;
    };
    HubSettingsState state(access);

    FunctionSettings remote = persisted.functions.first();
    remote.name = QStringLiteral("远端名称不应覆盖");
    remote.shortcut = QStringLiteral("Ctrl+Alt+9");
    remote.executionMode = FunctionExecutionMode::Canvas;
    remote.flow.enabled = false;
    remote.flow.draft.revision = 8;
    persisted.functions.clear();
    persisted.functions.append(unrelated);
    persisted.functions.append(remote);

    QVERIFY(state.reloadFunctionFlowState(QStringLiteral("custom_1")));

    const FunctionSettings reloaded =
        state.toData().function(QStringLiteral("custom_1"));
    QCOMPARE(reloaded.name, QStringLiteral("本地名称"));
    QCOMPARE(reloaded.shortcut, QStringLiteral("Ctrl+Alt+1"));
    QCOMPARE(
        reloaded.executionMode,
        FunctionExecutionMode::Canvas
    );
    QVERIFY(reloaded.flow.enabled);
    QCOMPARE(reloaded.flow.draft.revision, 8);
}

void HubSettingsStateTests::
reloadFunctionFlowStateRejectsMissingFunctions()
{
    AppSettingsData persisted;
    FunctionSettings local;
    local.id = QStringLiteral("custom_1");
    local.name = QStringLiteral("仅本地存在");
    persisted.functions.append(normalizeFunctionSettings(local));

    HubWindowAccess access;
    access.settingsSnapshotProvider = [&persisted]() {
        return persisted;
    };
    HubSettingsState state(access);

    persisted.functions.clear();
    QVERIFY(!state.reloadFunctionFlowState(QStringLiteral("custom_1")));

    FunctionSettings remoteOnly;
    remoteOnly.id = QStringLiteral("custom_2");
    remoteOnly.name = QStringLiteral("仅最新快照存在");
    persisted.functions.append(normalizeFunctionSettings(remoteOnly));
    QVERIFY(!state.reloadFunctionFlowState(QStringLiteral("custom_2")));
}

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
    function.inputOrder = QStringList()
        << QStringLiteral("selection")
        << QStringLiteral("voice")
        << QStringLiteral("screenshot");
    function.outputOrder = QStringList()
        << QStringLiteral("resultPopup")
        << QStringLiteral("ai")
        << QStringLiteral("screenshotPanel")
        << QStringLiteral("autoWrite");
    function.promptId = function.id;
    state.addCustomFunction(function);

    QCOMPARE(state.customFunctions().size(), 1);
    QCOMPARE(state.toData().function(QStringLiteral("custom_1")).name, QStringLiteral("润色"));
    QCOMPARE(
        state.toData().function(QStringLiteral("custom_1")).input.order,
        function.inputOrder
    );
    QCOMPARE(
        state.toData().function(QStringLiteral("custom_1")).output.order,
        function.outputOrder
    );
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

void HubSettingsStateTests::savesFunctionScopedInputOrder()
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
    state.setInputOrderFor(
        functionId,
        QStringList()
            << QStringLiteral("selection")
            << QStringLiteral("voice")
            << QStringLiteral("selection")
    );

    QCOMPARE(
        state.inputOrderFor(functionId),
        QStringList()
            << QStringLiteral("selection")
            << QStringLiteral("voice")
            << QStringLiteral("screenshot")
    );
    QVERIFY(state.save());
    QCOMPARE(
        saved.function(functionId).input.order,
        state.inputOrderFor(functionId)
    );
}

void HubSettingsStateTests::savesFunctionScopedOutputOrder()
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
    state.setOutputOrderFor(
        functionId,
        QStringList()
            << QStringLiteral("resultPopup")
            << QStringLiteral("ai")
            << QStringLiteral("resultPopup")
    );

    QCOMPARE(
        state.outputOrderFor(functionId),
        QStringList()
            << QStringLiteral("resultPopup")
            << QStringLiteral("ai")
            << QStringLiteral("autoWrite")
            << QStringLiteral("screenshotPanel")
    );
    QVERIFY(state.save());
    QCOMPARE(
        saved.function(functionId).output.order,
        state.outputOrderFor(functionId)
    );
}

void HubSettingsStateTests::
prefersNonFlowSaveAndReportsStructuredErrors()
{
    int legacyCalls = 0;
    int nonFlowCalls = 0;
    HubWindowAccess access;
    access.settingsSnapshotProvider = []() {
        return AppSettingsData();
    };
    access.applyAndSave = [&legacyCalls](const AppSettingsData &) {
        ++legacyCalls;
        return true;
    };
    access.applyNonFlowAndSave = [&nonFlowCalls](
        const AppSettingsData &,
        OperationError *error
    ) {
        ++nonFlowCalls;
        error->code = QStringLiteral("settings_function_set_stale");
        return false;
    };
    HubSettingsState state(access);
    OperationError error;
    QVERIFY(!state.save(&error));
    QCOMPARE(nonFlowCalls, 1);
    QCOMPARE(legacyCalls, 0);
    QCOMPARE(
        error.code,
        QStringLiteral("settings_function_set_stale")
    );
}

void HubSettingsStateTests::
updatingCustomFunctionPreservesItsEntireFlowState()
{
    AppSettingsData data;
    FunctionSettings original;
    original.id = QStringLiteral("custom_1");
    original.name = QStringLiteral("旧名称");
    original.flow.enabled = true;
    original.flow.draft.revision = 7;
    original.flow.draft.graphHash = QStringLiteral("draft_hash");
    original.flow.published.revision = 4;
    original.flow.published.graphHash =
        QStringLiteral("published_hash");
    original.flow.editor.viewportCenter = QPointF(12.0, 34.0);
    original.flow.editor.zoom = 1.5;
    original.flow.retainedValues.insert(
        QStringLiteral("future"),
        9
    );
    data.functions.append(original);

    HubWindowAccess access;
    access.settingsSnapshotProvider = [data]() { return data; };
    HubSettingsState state(access);
    CustomFunctionDef edited = state.customFunctions().first();
    edited.name = QStringLiteral("新名称");
    edited.shortcut = QStringLiteral("Ctrl+Alt+8");
    edited.model = QStringLiteral("deepseek-v4-pro");
    edited.outputMode = QStringLiteral("autoWrite");
    state.updateCustomFunction(edited);

    const FunctionSettings saved =
        state.toData().function(QStringLiteral("custom_1"));
    QCOMPARE(saved.name, QStringLiteral("新名称"));
    QCOMPARE(saved.flow.enabled, original.flow.enabled);
    QCOMPARE(saved.flow.draft.revision, 7);
    QCOMPARE(saved.flow.draft.graphHash, QStringLiteral("draft_hash"));
    QCOMPARE(saved.flow.published.revision, 4);
    QCOMPARE(
        saved.flow.published.graphHash,
        QStringLiteral("published_hash")
    );
    QCOMPARE(
        saved.flow.editor.viewportCenter,
        QPointF(12.0, 34.0)
    );
    QCOMPARE(saved.flow.editor.zoom, 1.5);
    QCOMPARE(
        saved.flow.retainedValues
            .value(QStringLiteral("future")).toInt(),
        9
    );
}

void HubSettingsStateTests::
preservesAndNormalizesCustomFloatingBarStyleOverride()
{
    CustomFunctionDef source;
    source.id = QStringLiteral("custom_1");
    source.name = QString::fromUtf8("润色");
    source.floatingBarStyleOverride =
        QStringLiteral("liveTranscriptCard");

    const FunctionSettings settings =
        functionSettingsFromCustomFunction(source);
    QCOMPARE(
        settings.output.floatingBarStyleOverride,
        QStringLiteral("liveTranscriptCard")
    );

    AppSettingsData data;
    data.functions.append(settings);
    HubWindowAccess access;
    access.settingsSnapshotProvider = [data]() { return data; };
    HubSettingsState state(access);
    QCOMPARE(
        state.customFunctions().first().floatingBarStyleOverride,
        QStringLiteral("liveTranscriptCard")
    );
    QCOMPARE(
        state.floatingBarStyleOverrideFor(QStringLiteral("custom_1")),
        QStringLiteral("liveTranscriptCard")
    );

    state.setFloatingBarStyleOverrideFor(
        QStringLiteral("custom_1"),
        QStringLiteral("unknown-style")
    );
    QCOMPARE(
        state.floatingBarStyleOverrideFor(QStringLiteral("custom_1")),
        QStringLiteral("inherit")
    );
    QCOMPARE(
        state.customFunctions().first().floatingBarStyleOverride,
        QStringLiteral("inherit")
    );
}

void HubSettingsStateTests::
nextCustomFunctionIdDoesNotReuseOrphanOrOrderedIds()
{
    AppSettingsData data;
    FunctionSettings existing;
    existing.id = QStringLiteral("custom_2");
    existing.name = QStringLiteral("现有");
    data.functions.append(existing);
    data.retainedOrphanFunctionFlows.insert(
        QStringLiteral("custom_7"),
        QJsonObject()
    );
    data.functionOrder.append(QStringLiteral("custom_9"));

    HubWindowAccess access;
    access.settingsSnapshotProvider = [data]() { return data; };
    HubSettingsState state(access);
    QCOMPARE(
        state.nextCustomFunctionId(),
        QStringLiteral("custom_10")
    );
}

void HubSettingsStateTests::
staleReplaceReloadsLatestStateWithoutReplayingEdits()
{
    AppSettingsData persisted;
    persisted.resultPopupOpacity = 91;
    FunctionSettings first;
    first.id = QStringLiteral("custom_1");
    first.name = QStringLiteral("第一个");
    persisted.functions.append(first);

    HubWindowAccess access;
    access.settingsSnapshotProvider = [&persisted]() {
        return persisted;
    };
    access.applyNonFlowAndSave = [&persisted](
        const AppSettingsData &edited,
        OperationError *error
    ) {
        if (edited.functions.size() != persisted.functions.size()) {
            if (error) {
                error->code = QStringLiteral(
                    "settings_function_set_stale"
                );
            }
            return false;
        }
        persisted = edited;
        return true;
    };
    HubSettingsState state(access);
    AppSettingsData staleEdit = state.toData();
    staleEdit.resultPopupOpacity = 55;

    FunctionSettings second;
    second.id = QStringLiteral("custom_2");
    second.name = QStringLiteral("第二个");
    persisted.functions.append(second);

    QVERIFY(!state.replaceAndSave(staleEdit));
    const AppSettingsData reloaded = state.toData();
    QCOMPARE(reloaded.functions.size(), 2);
    QCOMPARE(reloaded.resultPopupOpacity, 91);
}

QTEST_APPLESS_MAIN(HubSettingsStateTests)

#include "hub_settings_state_tests.moc"
