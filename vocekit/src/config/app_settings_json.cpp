#include "app_settings_json.h"
#include "app_settings_defaults.h"
#include "function_flow_json.h"

#include <QJsonArray>
#include <QSet>
#include <QtGlobal>

namespace {

const QStringList &builtInFunctionIds()
{
    static const QStringList ids = QStringList()
        << QStringLiteral("dictate")
        << QStringLiteral("translate")
        << QStringLiteral("ask");
    return ids;
}

QString builtInFunctionName(const QString &id)
{
    if (id == QStringLiteral("translate")) {
        return QStringLiteral("翻译");
    }
    if (id == QStringLiteral("ask")) {
        return QStringLiteral("问答");
    }
    return QStringLiteral("听写");
}

QString defaultShortcut(const QString &id)
{
    if (id == QStringLiteral("translate")) {
        return QStringLiteral("Ctrl+Alt+T");
    }
    if (id == QStringLiteral("ask")) {
        return QStringLiteral("Ctrl+Alt+Q");
    }
    return QStringLiteral("Ctrl+Alt+Space");
}

QString defaultScreenshotShortcut(const QString &id)
{
    if (id == QStringLiteral("translate")) {
        return QStringLiteral("Ctrl+Alt+Shift+T");
    }
    if (id == QStringLiteral("ask")) {
        return QStringLiteral("Ctrl+Alt+Shift+Q");
    }
    return QStringLiteral("Ctrl+Alt+Shift+Space");
}

FunctionSettings defaultBuiltInFunction(const QString &id)
{
    FunctionSettings settings;
    settings.id = id;
    settings.name = builtInFunctionName(id);
    settings.builtIn = true;
    settings.shortcut = defaultShortcut(id);
    settings.modelId = defaultModelForFunction(id);
    settings.promptId = id;
    settings.input.useSelection = defaultUseSelectionForFunction(id);
    settings.input.useVoice = defaultUseVoiceForFunction(id);
    settings.input.screenshotShortcut = defaultScreenshotShortcut(id);
    settings.recording.countdownSeconds = defaultCountdownSeconds();
    settings.recording.beepEnabled = true;
    settings.output.outputMode = defaultOutputModeForFunction(id);
    return settings;
}

QStringList stringListFromJson(const QJsonArray &array)
{
    QStringList values;
    for (const QJsonValue &value : array) {
        const QString text = value.toString().trimmed();
        if (!text.isEmpty() && !values.contains(text)) {
            values.append(text);
        }
    }
    return values;
}

QJsonArray stringListToJson(const QStringList &values)
{
    QJsonArray array;
    for (const QString &value : values) {
        array.append(value);
    }
    return array;
}

void applyCommonFunctionJson(
    FunctionSettings *settings,
    const QJsonObject &input,
    const QJsonObject &display,
    const QJsonObject &recording,
    const QJsonObject &network)
{
    if (!settings) {
        return;
    }

    if (input.contains(QStringLiteral("useSelection"))) {
        settings->input.useSelection =
            input.value(QStringLiteral("useSelection")).toBool();
    }
    if (input.contains(QStringLiteral("useVoice"))) {
        settings->input.useVoice =
            input.value(QStringLiteral("useVoice")).toBool();
    }
    if (input.contains(QStringLiteral("useScreenshot"))) {
        settings->input.useScreenshot =
            input.value(QStringLiteral("useScreenshot")).toBool();
    }
    if (input.contains(QStringLiteral("inputOrder"))) {
        settings->input.order = stringListFromJson(
            input.value(QStringLiteral("inputOrder")).toArray()
        );
    }
    const QString screenshotTriggerMode =
        input.value(QStringLiteral("screenshotTriggerMode")).toString();
    if (!screenshotTriggerMode.trimmed().isEmpty()) {
        settings->input.screenshotTriggerMode = screenshotTriggerMode;
    }
    const QString screenshotShortcut =
        input.value(QStringLiteral("screenshotShortcut")).toString();
    if (!screenshotShortcut.trimmed().isEmpty()) {
        settings->input.screenshotShortcut = screenshotShortcut;
    }

    if (display.contains(QStringLiteral("floatingBarSeconds"))) {
        settings->output.floatingBarSeconds =
            display.value(QStringLiteral("floatingBarSeconds")).toInt();
    }
    if (display.contains(QStringLiteral("resultPopupSeconds"))) {
        settings->output.resultPopupSeconds =
            display.value(QStringLiteral("resultPopupSeconds")).toInt();
    }
    if (display.contains(QStringLiteral("countdownSeconds"))) {
        settings->recording.countdownSeconds =
            display.value(QStringLiteral("countdownSeconds")).toInt();
    }
    if (display.contains(QStringLiteral("recordingBeepEnabled"))) {
        settings->recording.beepEnabled =
            display.value(QStringLiteral("recordingBeepEnabled")).toBool();
    }
    settings->recording.beepPath =
        display.value(QStringLiteral("recordingBeepPath")).toString();

    const QString triggerMode =
        recording.value(QStringLiteral("recordingTriggerMode")).toString();
    if (!triggerMode.trimmed().isEmpty()) {
        settings->recording.triggerMode = triggerMode;
    }
    if (recording.contains(QStringLiteral("longRecordingEnabled"))) {
        settings->recording.longRecordingEnabled =
            recording.value(QStringLiteral("longRecordingEnabled")).toBool();
    }
    if (recording.contains(QStringLiteral("segmentSeconds"))) {
        settings->recording.segmentSeconds =
            recording.value(QStringLiteral("segmentSeconds")).toInt();
    }
    if (recording.contains(QStringLiteral("maxRecordingMinutes"))) {
        settings->recording.maximumMinutes =
            recording.value(QStringLiteral("maxRecordingMinutes")).toInt();
    }
    settings->network = functionNetworkPoliciesFromJson(network);
}

QJsonObject inputJson(const FunctionSettings &settings)
{
    QJsonObject object;
    object.insert(
        QStringLiteral("useSelection"),
        settings.input.useSelection
    );
    object.insert(QStringLiteral("useVoice"), settings.input.useVoice);
    object.insert(
        QStringLiteral("useScreenshot"),
        settings.input.useScreenshot
    );
    object.insert(
        QStringLiteral("inputOrder"),
        stringListToJson(normalizeFunctionInputOrder(settings.input.order))
    );
    object.insert(
        QStringLiteral("screenshotTriggerMode"),
        settings.input.screenshotTriggerMode
    );
    object.insert(
        QStringLiteral("screenshotShortcut"),
        settings.input.screenshotShortcut
    );
    return object;
}

QJsonObject displayJson(const FunctionSettings &settings)
{
    QJsonObject object;
    object.insert(
        QStringLiteral("floatingBarSeconds"),
        settings.output.floatingBarSeconds
    );
    object.insert(
        QStringLiteral("resultPopupSeconds"),
        settings.output.resultPopupSeconds
    );
    object.insert(
        QStringLiteral("countdownSeconds"),
        settings.recording.countdownSeconds
    );
    object.insert(
        QStringLiteral("recordingBeepEnabled"),
        settings.recording.beepEnabled
    );
    object.insert(
        QStringLiteral("recordingBeepPath"),
        settings.recording.beepPath
    );
    return object;
}

QJsonObject recordingJson(const FunctionSettings &settings)
{
    QJsonObject object;
    object.insert(
        QStringLiteral("recordingTriggerMode"),
        settings.recording.triggerMode
    );
    object.insert(
        QStringLiteral("longRecordingEnabled"),
        settings.recording.longRecordingEnabled
    );
    object.insert(
        QStringLiteral("segmentSeconds"),
        settings.recording.segmentSeconds
    );
    object.insert(
        QStringLiteral("maxRecordingMinutes"),
        settings.recording.maximumMinutes
    );
    return object;
}

QJsonObject functionFlowJson(const FunctionSettings &settings)
{
    QJsonObject object = functionFlowStateToJson(settings.flow);
    const QJsonValue retainedMode =
        settings.flow.retainedValues.value(
            QStringLiteral("executionMode")
        );
    bool retainedModeKnown = false;
    if (retainedMode.isString()) {
        functionExecutionModeFromId(
            retainedMode.toString(),
            &retainedModeKnown
        );
    }
    const QJsonValue writtenMode =
        !retainedMode.isUndefined()
            && (!retainedMode.isString() || !retainedModeKnown)
            ? retainedMode
            : QJsonValue(functionExecutionModeId(settings.executionMode));
    object.insert(
        QStringLiteral("executionMode"),
        writtenMode
    );
    object.insert(
        QStringLiteral("enabled"),
        settings.executionMode == FunctionExecutionMode::Canvas
    );
    return object;
}

QJsonObject pointJson(const QPoint &point)
{
    QJsonObject object;
    object.insert(QStringLiteral("x"), point.x());
    object.insert(QStringLiteral("y"), point.y());
    return object;
}

QJsonObject rectangleJson(const QRect &rectangle)
{
    QJsonObject object = pointJson(rectangle.topLeft());
    object.insert(QStringLiteral("width"), rectangle.width());
    object.insert(QStringLiteral("height"), rectangle.height());
    return object;
}

bool readPoint(const QJsonObject &object, QPoint *point)
{
    if (!point || !object.contains(QStringLiteral("x"))
        || !object.contains(QStringLiteral("y"))) {
        return false;
    }
    *point = QPoint(
        object.value(QStringLiteral("x")).toInt(),
        object.value(QStringLiteral("y")).toInt()
    );
    return true;
}

bool readRectangle(const QJsonObject &object, QRect *rectangle)
{
    if (!rectangle || !object.contains(QStringLiteral("x"))
        || !object.contains(QStringLiteral("y"))
        || !object.contains(QStringLiteral("width"))
        || !object.contains(QStringLiteral("height"))) {
        return false;
    }
    *rectangle = QRect(
        object.value(QStringLiteral("x")).toInt(),
        object.value(QStringLiteral("y")).toInt(),
        object.value(QStringLiteral("width")).toInt(),
        object.value(QStringLiteral("height")).toInt()
    );
    return true;
}

} // namespace

AppSettingsData appSettingsDataFromJson(
    const QJsonObject &root,
    QStringList *warnings)
{
    AppSettingsData data;
    data.retainedRootValues = root;
    data.retainedRootValues.remove(QStringLiteral("functionFlows"));
    data.trayResident =
        root.value(QStringLiteral("trayResident")).toBool(true);
    data.autoStartEnabled =
        root.value(QStringLiteral("autoStartEnabled")).toBool(false);
    data.strongSelectionEnabled =
        root.value(QStringLiteral("strongSelectionEnabled")).toBool(false);
    data.floatingBarEnabled =
        root.value(QStringLiteral("floatingBarEnabled")).toBool(true);
    data.streamingSpeechRecognitionEnabled = root
        .value(QStringLiteral("streamingSpeechRecognitionEnabled"))
        .toBool(true);
    data.promptLocked =
        root.value(QStringLiteral("promptLocked")).toBool(false);
    data.dictatePolishEnabled =
        root.value(QStringLiteral("dictatePolishEnabled")).toBool(false);
    data.useSystemProxy =
        root.value(QStringLiteral("useSystemProxy")).toBool(false);
    data.resultPopupOpacity = qBound(
        60,
        root.value(QStringLiteral("resultPopupOpacity")).toInt(100),
        100
    );
    data.preRecordCountdownEnabled =
        root.value(QStringLiteral("preRecordCountdownEnabled")).toBool(false);
    data.recordingBeepEnabled =
        root.value(QStringLiteral("recordingBeepEnabled")).toBool(false);
    data.vocabularyEnabled =
        root.value(QStringLiteral("vocabularyEnabled")).toBool(true);
    data.vocabularyAddMode = normalizeVocabularyAddMode(
        root.value(QStringLiteral("vocabularyAddMode")).toString()
    );
    data.vocabularyOnlyForVoiceInput =
        root.value(QStringLiteral("vocabularyOnlyForVoiceInput")).toBool(false);
    data.vocabularyPromptEntryLimit = qBound(
        0,
        root.value(QStringLiteral("vocabularyPromptEntryLimit")).toInt(16),
        100
    );
    data.speechProvider = normalizeSpeechProvider(
        root.value(QStringLiteral("speechProvider")).toString()
    );
    data.ocrEngine = normalizeOcrEngine(
        root.value(QStringLiteral("ocrEngine")).toString()
    );
    data.ocrTimeoutMs = qBound(
        5000,
        root.value(QStringLiteral("ocrTimeoutMs")).toInt(45000),
        120000
    );
    data.recordDirectory =
        root.value(QStringLiteral("recordPath")).toString().trimmed();
    data.targetLanguage =
        root.value(QStringLiteral("targetLanguage"))
            .toString(QStringLiteral("简体中文"));
    data.historyInitialLoadCount = qBound(
        5,
        root.value(QStringLiteral("historyInitialLoadCount")).toInt(12),
        200
    );
    data.historyLoadMoreCount = qBound(
        5,
        root.value(QStringLiteral("historyLoadMoreCount")).toInt(25),
        200
    );
    data.logInitialLoadCount = qBound(
        5,
        root.value(QStringLiteral("logInitialLoadCount")).toInt(20),
        500
    );
    data.logLoadMoreCount = qBound(
        5,
        root.value(QStringLiteral("logLoadMoreCount")).toInt(30),
        500
    );
    data.favoriteFolders = stringListFromJson(
        root.value(QStringLiteral("favoriteFolders")).toArray()
    );

    data.windows.hasFloatingBarPosition = readPoint(
        root.value(QStringLiteral("floatingBarPosition")).toObject(),
        &data.windows.floatingBarPosition
    );
    data.windows.hasResultPopupGeometry = readRectangle(
        root.value(QStringLiteral("resultPopupGeometry")).toObject(),
        &data.windows.resultPopupGeometry
    );
    data.windows.hasScreenshotResultGeometry = readRectangle(
        root.value(QStringLiteral("screenshotResultGeometry")).toObject(),
        &data.windows.screenshotResultGeometry
    );
    data.windows.screenshotResultOpacity = qBound(
        30,
        root.value(QStringLiteral("screenshotResultOpacity")).toInt(92),
        100
    );
    data.windows.hasScreenshotLauncherPosition = readPoint(
        root.value(QStringLiteral("screenshotLauncherPosition")).toObject(),
        &data.windows.screenshotLauncherPosition
    );

    const QJsonObject hotkeys =
        root.value(QStringLiteral("hotkeys")).toObject();
    for (auto it = hotkeys.constBegin(); it != hotkeys.constEnd(); ++it) {
        if (!builtInFunctionIds().contains(it.key())) {
            data.applicationHotkeys.insert(
                it.key(),
                it.value().toString().trimmed()
            );
        }
    }

    const QJsonObject models =
        root.value(QStringLiteral("models")).toObject();
    const QJsonObject outputModes =
        root.value(QStringLiteral("outputModes")).toObject();
    const QJsonObject outputOrders =
        root.value(QStringLiteral("outputOrders")).toObject();
    const QJsonObject resultTemplates =
        root.value(QStringLiteral("resultTemplates")).toObject();
    const QJsonObject resultActions =
        root.value(QStringLiteral("resultActions")).toObject();
    const QJsonObject networkPolicies =
        root.value(QStringLiteral("networkPolicies")).toObject();
    const QJsonObject promptIds =
        root.value(QStringLiteral("promptIds")).toObject();
    const QJsonObject inputModes =
        root.value(QStringLiteral("inputModes")).toObject();
    const QJsonObject displayTimes =
        root.value(QStringLiteral("displayTimes")).toObject();
    const QJsonObject recordingModes =
        root.value(QStringLiteral("recordingModes")).toObject();

    for (const QString &id : builtInFunctionIds()) {
        FunctionSettings settings = defaultBuiltInFunction(id);
        const QString shortcut = hotkeys.value(id).toString().trimmed();
        if (!shortcut.isEmpty()) {
            settings.shortcut = shortcut;
        }
        const QString model = models.value(id).toString().trimmed();
        if (!model.isEmpty()) {
            settings.modelId = model;
        }
        const QString outputMode =
            outputModes.value(id).toString().trimmed();
        if (!outputMode.isEmpty()) {
            settings.output.outputMode = outputMode;
        }
        settings.output.order = stringListFromJson(
            outputOrders.value(id).toArray()
        );
        const QString resultTemplate =
            resultTemplates.value(id).toString().trimmed();
        if (!resultTemplate.isEmpty()) {
            settings.output.resultTemplate = resultTemplate;
        }
        QStringList actions;
        for (const QJsonValue &value :
             resultActions.value(id).toArray()) {
            actions.append(value.toString());
        }
        settings.output.resultActions = normalizeResultActionIds(actions);
        const QString promptId =
            promptIds.value(id).toString().trimmed();
        if (!promptId.isEmpty()) {
            settings.promptId = promptId;
        }
        applyCommonFunctionJson(
            &settings,
            inputModes.value(id).toObject(),
            displayTimes.value(id).toObject(),
            recordingModes.value(id).toObject(),
            networkPolicies.value(id).toObject()
        );
        data.functions.append(normalizeFunctionSettings(settings));
    }

    for (const QJsonValue &value :
         root.value(QStringLiteral("customFunctions")).toArray()) {
        const QJsonObject object = value.toObject();
        FunctionSettings settings;
        settings.id = object.value(QStringLiteral("id")).toString().trimmed();
        settings.name =
            object.value(QStringLiteral("name")).toString().trimmed();
        settings.builtIn = false;
        settings.shortcut =
            object.value(QStringLiteral("shortcut")).toString();
        settings.modelId =
            object.value(QStringLiteral("model"))
                .toString(defaultModelForFunction(QString()));
        settings.promptId =
            object.value(QStringLiteral("promptId")).toString().trimmed();
        if (settings.promptId.isEmpty()) {
            settings.promptId = settings.id;
        }
        settings.prompt = object.value(QStringLiteral("prompt")).toString();
        settings.input.useSelection =
            object.value(QStringLiteral("useSelection")).toBool(true);
        settings.input.useVoice =
            object.value(QStringLiteral("useVoice")).toBool(true);
        settings.input.useScreenshot =
            object.value(QStringLiteral("useScreenshot")).toBool(false);
        settings.input.screenshotTriggerMode =
            object.value(QStringLiteral("screenshotTriggerMode"))
                .toString(QStringLiteral("separate"));
        settings.input.screenshotShortcut =
            object.value(QStringLiteral("screenshotShortcut")).toString();
        settings.output.outputMode =
            object.value(QStringLiteral("outputMode"))
                .toString(outputModePopup());
        settings.output.order = stringListFromJson(
            object.value(QStringLiteral("outputOrder")).toArray()
        );
        settings.output.resultTemplate =
            object.value(QStringLiteral("resultTemplate"))
                .toString(resultTemplateSimple());
        QStringList actions;
        for (const QJsonValue &action :
             object.value(QStringLiteral("resultActions")).toArray()) {
            actions.append(action.toString());
        }
        settings.output.resultActions = normalizeResultActionIds(actions);
        applyCommonFunctionJson(
            &settings,
            object,
            object,
            object,
            object.value(QStringLiteral("networkPolicy")).toObject()
        );

        settings = normalizeFunctionSettings(settings);
        if (settings.id.isEmpty() || settings.name.isEmpty()) {
            if (warnings) {
                warnings->append(QStringLiteral(
                    "已跳过缺少编号或名称的自定义功能。"
                ));
            }
            continue;
        }
        if (data.functionIndex(settings.id) >= 0) {
            if (warnings) {
                warnings->append(
                    QStringLiteral("已跳过重复功能：") + settings.id
                );
            }
            continue;
        }
        data.functions.append(settings);
    }

    data.functionOrder = stringListFromJson(
        root.value(QStringLiteral("functionOrder")).toArray()
    );
    for (const FunctionSettings &settings : data.functions) {
        if (!data.functionOrder.contains(settings.id)) {
            data.functionOrder.append(settings.id);
        }
    }

    const QJsonValue functionFlowsValue =
        root.value(QStringLiteral("functionFlows"));
    if (functionFlowsValue.isObject()) {
        const QJsonObject functionFlows =
            functionFlowsValue.toObject();
        for (auto it = functionFlows.constBegin();
             it != functionFlows.constEnd();
             ++it) {
            const int index = data.functionIndex(it.key());
            if (index < 0) {
                data.retainedOrphanFunctionFlows.insert(
                    it.key(),
                    it.value()
                );
                continue;
            }
            if (it.value().isObject()) {
                QJsonObject flowObject = it.value().toObject();
                FunctionExecutionMode executionMode =
                    FunctionExecutionMode::Classic;
                const QJsonValue executionModeValue =
                    flowObject.value(QStringLiteral("executionMode"));
                if (executionModeValue.isString()) {
                    executionMode = functionExecutionModeFromId(
                        executionModeValue.toString()
                    );
                } else if (executionModeValue.isUndefined()
                    && flowObject
                    .value(QStringLiteral("enabled")).toBool(false)) {
                    executionMode = FunctionExecutionMode::Canvas;
                }
                flowObject.insert(
                    QStringLiteral("enabled"),
                    executionMode == FunctionExecutionMode::Canvas
                );
                data.functions[index].executionMode = executionMode;
                data.functions[index].flow =
                    functionFlowStateFromJson(flowObject, warnings);
                data.functions[index] =
                    normalizeFunctionSettings(data.functions[index]);
            }
        }
    }
    return data;
}

QJsonObject appSettingsDataToJson(const AppSettingsData &data)
{
    QJsonObject root = data.retainedRootValues;
    root.remove(QStringLiteral("functionFlows"));
    QJsonObject hotkeys;
    for (auto it = data.applicationHotkeys.constBegin();
         it != data.applicationHotkeys.constEnd();
         ++it) {
        hotkeys.insert(it.key(), it.value());
    }

    QJsonObject models;
    QJsonObject outputModes;
    QJsonObject outputOrders;
    QJsonObject resultTemplates;
    QJsonObject resultActions;
    QJsonObject networkPolicies;
    QJsonObject promptIds;
    QJsonObject inputModes;
    QJsonObject displayTimes;
    QJsonObject recordingModes;
    QJsonArray customFunctions;
    QJsonObject functionFlows = data.retainedOrphanFunctionFlows;

    for (const FunctionSettings &rawSettings : data.functions) {
        const FunctionSettings settings =
            normalizeFunctionSettings(rawSettings);
        functionFlows.insert(
            settings.id,
            functionFlowJson(settings)
        );
        if (settings.builtIn) {
            hotkeys.insert(settings.id, settings.shortcut);
            models.insert(settings.id, settings.modelId);
            outputModes.insert(
                settings.id,
                settings.output.outputMode
            );
            outputOrders.insert(
                settings.id,
                stringListToJson(settings.output.order)
            );
            resultTemplates.insert(
                settings.id,
                settings.output.resultTemplate
            );
            resultActions.insert(
                settings.id,
                stringListToJson(settings.output.resultActions)
            );
            networkPolicies.insert(
                settings.id,
                functionNetworkPoliciesToJson(settings.network)
            );
            promptIds.insert(settings.id, settings.promptId);
            inputModes.insert(settings.id, inputJson(settings));
            displayTimes.insert(settings.id, displayJson(settings));
            recordingModes.insert(settings.id, recordingJson(settings));
            continue;
        }

        QJsonObject object;
        object.insert(QStringLiteral("id"), settings.id);
        object.insert(QStringLiteral("name"), settings.name);
        object.insert(QStringLiteral("shortcut"), settings.shortcut);
        object.insert(QStringLiteral("model"), settings.modelId);
        object.insert(QStringLiteral("promptId"), settings.promptId);
        object.insert(QStringLiteral("prompt"), settings.prompt);
        const QJsonObject input = inputJson(settings);
        for (auto it = input.constBegin(); it != input.constEnd(); ++it) {
            object.insert(it.key(), it.value());
        }
        const QJsonObject display = displayJson(settings);
        for (auto it = display.constBegin(); it != display.constEnd(); ++it) {
            object.insert(it.key(), it.value());
        }
        const QJsonObject recording = recordingJson(settings);
        for (auto it = recording.constBegin();
             it != recording.constEnd();
             ++it) {
            object.insert(it.key(), it.value());
        }
        object.insert(
            QStringLiteral("outputMode"),
            settings.output.outputMode
        );
        object.insert(
            QStringLiteral("outputOrder"),
            stringListToJson(settings.output.order)
        );
        object.insert(
            QStringLiteral("resultTemplate"),
            settings.output.resultTemplate
        );
        object.insert(
            QStringLiteral("resultActions"),
            stringListToJson(settings.output.resultActions)
        );
        object.insert(
            QStringLiteral("networkPolicy"),
            functionNetworkPoliciesToJson(settings.network)
        );
        customFunctions.append(object);
    }

    root.insert(QStringLiteral("hotkeys"), hotkeys);
    root.insert(QStringLiteral("models"), models);
    root.insert(QStringLiteral("outputModes"), outputModes);
    root.insert(QStringLiteral("outputOrders"), outputOrders);
    root.insert(QStringLiteral("resultTemplates"), resultTemplates);
    root.insert(QStringLiteral("resultActions"), resultActions);
    root.insert(QStringLiteral("networkPolicies"), networkPolicies);
    root.insert(QStringLiteral("promptIds"), promptIds);
    root.insert(QStringLiteral("inputModes"), inputModes);
    root.insert(QStringLiteral("displayTimes"), displayTimes);
    root.insert(QStringLiteral("recordingModes"), recordingModes);
    root.insert(QStringLiteral("customFunctions"), customFunctions);
    root.insert(QStringLiteral("functionFlows"), functionFlows);

    root.insert(QStringLiteral("trayResident"), data.trayResident);
    root.insert(QStringLiteral("autoStartEnabled"), data.autoStartEnabled);
    root.insert(
        QStringLiteral("strongSelectionEnabled"),
        data.strongSelectionEnabled
    );
    root.insert(
        QStringLiteral("floatingBarEnabled"),
        data.floatingBarEnabled
    );
    root.insert(
        QStringLiteral("streamingSpeechRecognitionEnabled"),
        data.streamingSpeechRecognitionEnabled
    );
    root.insert(QStringLiteral("promptLocked"), data.promptLocked);
    root.insert(
        QStringLiteral("dictatePolishEnabled"),
        data.dictatePolishEnabled
    );
    root.insert(QStringLiteral("useSystemProxy"), data.useSystemProxy);
    root.insert(
        QStringLiteral("resultPopupOpacity"),
        data.resultPopupOpacity
    );
    root.insert(
        QStringLiteral("preRecordCountdownEnabled"),
        data.preRecordCountdownEnabled
    );
    root.insert(
        QStringLiteral("recordingBeepEnabled"),
        data.recordingBeepEnabled
    );
    root.insert(
        QStringLiteral("vocabularyEnabled"),
        data.vocabularyEnabled
    );
    root.insert(
        QStringLiteral("vocabularyAddMode"),
        data.vocabularyAddMode
    );
    root.insert(
        QStringLiteral("vocabularyOnlyForVoiceInput"),
        data.vocabularyOnlyForVoiceInput
    );
    root.insert(
        QStringLiteral("vocabularyPromptEntryLimit"),
        data.vocabularyPromptEntryLimit
    );
    root.insert(QStringLiteral("speechProvider"), data.speechProvider);
    root.insert(QStringLiteral("ocrEngine"), data.ocrEngine);
    root.insert(QStringLiteral("ocrTimeoutMs"), data.ocrTimeoutMs);
    root.insert(QStringLiteral("recordPath"), data.recordDirectory);
    root.insert(QStringLiteral("targetLanguage"), data.targetLanguage);
    root.insert(
        QStringLiteral("historyInitialLoadCount"),
        data.historyInitialLoadCount
    );
    root.insert(
        QStringLiteral("historyLoadMoreCount"),
        data.historyLoadMoreCount
    );
    root.insert(
        QStringLiteral("logInitialLoadCount"),
        data.logInitialLoadCount
    );
    root.insert(
        QStringLiteral("logLoadMoreCount"),
        data.logLoadMoreCount
    );
    root.insert(
        QStringLiteral("favoriteFolders"),
        stringListToJson(data.favoriteFolders)
    );
    root.insert(
        QStringLiteral("functionOrder"),
        stringListToJson(data.functionOrder)
    );

    if (data.windows.hasFloatingBarPosition) {
        root.insert(
            QStringLiteral("floatingBarPosition"),
            pointJson(data.windows.floatingBarPosition)
        );
    } else {
        root.remove(QStringLiteral("floatingBarPosition"));
    }
    if (data.windows.hasResultPopupGeometry) {
        root.insert(
            QStringLiteral("resultPopupGeometry"),
            rectangleJson(data.windows.resultPopupGeometry)
        );
    } else {
        root.remove(QStringLiteral("resultPopupGeometry"));
    }
    if (data.windows.hasScreenshotResultGeometry) {
        root.insert(
            QStringLiteral("screenshotResultGeometry"),
            rectangleJson(data.windows.screenshotResultGeometry)
        );
    } else {
        root.remove(QStringLiteral("screenshotResultGeometry"));
    }
    root.insert(
        QStringLiteral("screenshotResultOpacity"),
        data.windows.screenshotResultOpacity
    );
    if (data.windows.hasScreenshotLauncherPosition) {
        root.insert(
            QStringLiteral("screenshotLauncherPosition"),
            pointJson(data.windows.screenshotLauncherPosition)
        );
    } else {
        root.remove(QStringLiteral("screenshotLauncherPosition"));
    }
    return root;
}
