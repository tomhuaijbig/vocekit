#include "hub_settings_state.h"

#include "../capture/screenshot_types.h"
#include "../config/app_paths.h"
#include "../config/app_settings_defaults.h"
#include "../input/hotkey_definitions.h"
#include "../recording/segmented_recording.h"

#include <QDir>
#include <QFileInfo>
#include <QKeySequence>

namespace {

QString titleForFunction(const QString &id)
{
    for (const HotkeyDef &definition : coreFunctionDefs()) {
        if (definition.id == id) {
            return definition.title;
        }
    }
    return id;
}

SelectionContextSettings normalizedHubSelectionContextSettings(
    const SelectionContextSettings &source,
    const AppSettingsData &completeSettings)
{
    SelectionContextSettings result = normalizeSelectionContextSettings(
        source,
        completeSettings
    );
    result.minimumTextLength = qBound(1, result.minimumTextLength, 1000);
    result.pauseMinutes = qBound(1, result.pauseMinutes, 1440);
    QStringList applications;
    for (const QString &value : result.blockedApplications) {
        const QString executable = QFileInfo(value.trimmed())
            .fileName()
            .toLower();
        if (!executable.isEmpty() && !applications.contains(executable)) {
            applications.append(executable);
        }
    }
    result.blockedApplications = applications;
    return result;
}

AppSettingsData normalizedHubSettingsData(const AppSettingsData &source)
{
    AppSettingsData result = source;
    result.selectionContext = normalizedHubSelectionContextSettings(
        source.selectionContext,
        result
    );
    return result;
}

CustomFunctionDef toCustomFunction(const FunctionSettings &source)
{
    const FunctionSettings settings = normalizeFunctionSettings(source);
    CustomFunctionDef function;
    function.id = settings.id;
    function.name = settings.name;
    function.shortcut = settings.shortcut;
    function.model = settings.modelId;
    function.outputMode = settings.output.outputMode;
    function.floatingBarStyleOverride =
        settings.output.floatingBarStyleOverride;
    function.outputOrder = settings.output.order;
    function.resultTemplate = settings.output.resultTemplate;
    function.useSelection = settings.input.useSelection;
    function.useVoice = settings.input.useVoice;
    function.useScreenshot = settings.input.useScreenshot;
    function.inputOrder = settings.input.order;
    function.screenshotTriggerMode = settings.input.screenshotTriggerMode;
    function.screenshotShortcut = settings.input.screenshotShortcut;
    function.floatingBarSeconds = settings.output.floatingBarSeconds;
    function.resultPopupSeconds = settings.output.resultPopupSeconds;
    function.countdownSeconds = settings.recording.countdownSeconds;
    function.recordingBeepEnabled = settings.recording.beepEnabled;
    function.recordingBeepPath = settings.recording.beepPath;
    function.recordingTriggerMode = settings.recording.triggerMode;
    function.longRecordingEnabled = settings.recording.longRecordingEnabled;
    function.segmentSeconds = settings.recording.segmentSeconds;
    function.maxRecordingMinutes = settings.recording.maximumMinutes;
    function.prompt = settings.prompt;
    function.promptId = settings.promptId;
    function.resultActions = settings.output.resultActions;
    function.networkPolicies = settings.network;
    return function;
}

FunctionSettings buildFunctionSettingsFromCustomFunction(
    const CustomFunctionDef &source)
{
    FunctionSettings settings;
    settings.id = source.id.trimmed();
    settings.name = source.name.trimmed();
    settings.builtIn = false;
    settings.shortcut = source.shortcut.trimmed();
    settings.modelId = source.model;
    settings.promptId = source.promptId.trimmed().isEmpty()
        ? settings.id
        : source.promptId.trimmed();
    settings.prompt = source.prompt;
    settings.input.useSelection = source.useSelection;
    settings.input.useVoice = source.useVoice;
    settings.input.useScreenshot = source.useScreenshot;
    settings.input.order = source.inputOrder;
    settings.input.screenshotTriggerMode = source.screenshotTriggerMode;
    settings.input.screenshotShortcut = source.screenshotShortcut;
    settings.recording.triggerMode = source.recordingTriggerMode;
    settings.recording.longRecordingEnabled = source.longRecordingEnabled;
    settings.recording.segmentSeconds = source.segmentSeconds;
    settings.recording.maximumMinutes = source.maxRecordingMinutes;
    settings.recording.countdownSeconds = source.countdownSeconds;
    settings.recording.beepEnabled = source.recordingBeepEnabled;
    settings.recording.beepPath = source.recordingBeepPath;
    settings.output.outputMode = source.outputMode;
    settings.output.floatingBarStyleOverride =
        normalizeFunctionFloatingBarStyle(
            source.floatingBarStyleOverride
        );
    settings.output.order = source.outputOrder;
    settings.output.resultTemplate = source.resultTemplate;
    settings.output.resultActions = source.resultActions;
    settings.output.floatingBarSeconds = source.floatingBarSeconds;
    settings.output.resultPopupSeconds = source.resultPopupSeconds;
    settings.network = source.networkPolicies;
    return normalizeFunctionSettings(settings);
}

} // namespace

FunctionSettings functionSettingsFromCustomFunction(
    const CustomFunctionDef &source)
{
    return buildFunctionSettingsFromCustomFunction(source);
}

HubSettingsState::HubSettingsState(const HubWindowAccess &access)
    : m_access(access)
{
    load();
}

HubSettingsState::operator bool() const
{
    return true;
}

void HubSettingsState::load()
{
    const AppSettingsData loaded = m_access.settingsSnapshotProvider
        ? m_access.settingsSnapshotProvider()
        : AppSettingsData();
    m_data = normalizedHubSettingsData(loaded);
    m_promptLibrary = m_access.promptLibraryProvider
        ? m_access.promptLibraryProvider()
        : QVector<PromptLibraryItem>();
    refreshCustomFunctions();
}

bool HubSettingsState::save(OperationError *error) const
{
    const AppSettingsData normalized = normalizedHubSettingsData(m_data);
    if (m_access.applyNonFlowAndSave) {
        return m_access.applyNonFlowAndSave(normalized, error);
    }
    return m_access.applyAndSave
        ? m_access.applyAndSave(normalized)
        : false;
}

bool HubSettingsState::replaceAndSave(
    const AppSettingsData &data,
    OperationError *error)
{
    OperationError localError;
    OperationError *saveError = error ? error : &localError;
    *saveError = OperationError();
    const AppSettingsData previous = m_data;
    m_data = normalizedHubSettingsData(data);
    refreshCustomFunctions();
    if (save(saveError)) {
        return true;
    }
    if (saveError->code
        == QStringLiteral("settings_function_set_stale")) {
        load();
        return false;
    }
    m_data = previous;
    refreshCustomFunctions();
    return false;
}

void HubSettingsState::replaceFunctionFlowState(
    const QString &functionId,
    const FunctionFlowState &state)
{
    const int index = m_data.functionIndex(functionId);
    if (index < 0) {
        return;
    }
    m_data.functions[index].flow = state;
    refreshCustomFunctions();
}

bool HubSettingsState::reloadFunctionFlowState(
    const QString &functionId)
{
    if (!m_access.settingsSnapshotProvider) {
        return false;
    }
    const AppSettingsData latest =
        m_access.settingsSnapshotProvider();
    const int index = latest.functionIndex(functionId);
    if (index < 0) {
        return false;
    }
    const int localIndex = m_data.functionIndex(functionId);
    if (localIndex < 0) {
        return false;
    }
    FunctionSettings synchronized =
        m_data.functions.at(localIndex);
    const FunctionSettings &remote =
        latest.functions.at(index);
    synchronized.executionMode = remote.executionMode;
    synchronized.flow = remote.flow;
    m_data.functions[localIndex] =
        normalizeFunctionSettings(synchronized);
    refreshCustomFunctions();
    return true;
}

AppSettingsData HubSettingsState::toData() const
{
    return normalizedHubSettingsData(m_data);
}

const FunctionSettings *HubSettingsState::findFunction(const QString &id) const
{
    const int index = m_data.functionIndex(id);
    return index >= 0 ? &m_data.functions.at(index) : nullptr;
}

FunctionSettings HubSettingsState::defaultFunction(const QString &id) const
{
    FunctionSettings settings;
    settings.id = id;
    settings.name = titleForFunction(id);
    settings.builtIn = id == QStringLiteral("dictate")
        || id == QStringLiteral("translate")
        || id == QStringLiteral("ask");
    for (const HotkeyDef &definition : hotkeyDefs()) {
        if (definition.id == id) {
            settings.shortcut = definition.defaultValue;
            break;
        }
    }
    settings.modelId = defaultModelForFunction(id);
    settings.promptId = defaultPromptIdForFunction(id);
    settings.input.useSelection = defaultUseSelectionForFunction(id);
    settings.input.useVoice = defaultUseVoiceForFunction(id);
    settings.input.screenshotTriggerMode = screenshotTriggerSeparate();
    settings.input.screenshotShortcut = defaultScreenshotShortcutForFunction(id);
    settings.recording.countdownSeconds = defaultCountdownSeconds();
    settings.recording.beepEnabled = true;
    settings.output.outputMode = defaultOutputModeForFunction(id);
    settings.output.resultTemplate = resultTemplateSimple();
    settings.output.floatingBarSeconds = defaultFloatingBarSeconds();
    settings.output.resultPopupSeconds = defaultResultPopupSeconds();
    return normalizeFunctionSettings(settings);
}

FunctionSettings *HubSettingsState::mutableFunction(const QString &id)
{
    int index = m_data.functionIndex(id);
    if (index < 0) {
        m_data.functions.append(defaultFunction(id));
        index = m_data.functions.size() - 1;
    }
    return &m_data.functions[index];
}

QString HubSettingsState::hotkey(const QString &id) const
{
    if (const FunctionSettings *settings = findFunction(id)) {
        return settings->shortcut;
    }
    const QString configured = m_data.applicationHotkeys.value(id).trimmed();
    if (!configured.isEmpty()) {
        return configured;
    }
    for (const HotkeyDef &definition : hotkeyDefs()) {
        if (definition.id == id) {
            return definition.defaultValue;
        }
    }
    return QString();
}

void HubSettingsState::setHotkey(const QString &id, const QString &value)
{
    if (findFunction(id) || id == QStringLiteral("dictate")
        || id == QStringLiteral("translate") || id == QStringLiteral("ask")) {
        mutableFunction(id)->shortcut = value.trimmed();
        refreshCustomFunctions();
        return;
    }
    m_data.applicationHotkeys.insert(id, value.trimmed());
}

bool HubSettingsState::conflictsWithOther(
    const QString &id,
    const QString &value,
    QString *otherTitle) const
{
    const QString normalized = value.trimmed().toLower();
    if (normalized.isEmpty()) {
        return false;
    }
    for (const HotkeyDef &definition : hotkeyDefs()) {
        if (definition.id != id
            && hotkey(definition.id).trimmed().toLower() == normalized) {
            if (otherTitle) {
                *otherTitle = definition.title;
            }
            return true;
        }
    }
    for (const FunctionSettings &settings : m_data.functions) {
        if (settings.id != id
            && settings.shortcut.trimmed().toLower() == normalized) {
            if (otherTitle) {
                *otherTitle = settings.name;
            }
            return true;
        }
        const QString logicalId = screenshotHotkeyLogicalId(settings.id);
        if (logicalId != id && settings.input.useScreenshot
            && screenshotTriggerUsesSeparate(settings.input.screenshotTriggerMode)
            && screenshotShortcutFor(settings.id).trimmed().toLower() == normalized) {
            if (otherTitle) {
                *otherTitle = settings.name + QString::fromUtf8("截图");
            }
            return true;
        }
    }
    return false;
}

const QVector<CustomFunctionDef> &HubSettingsState::customFunctions() const
{
    return m_customFunctions;
}

void HubSettingsState::refreshCustomFunctions()
{
    m_customFunctions.clear();
    for (const FunctionSettings &settings : m_data.functions) {
        if (!settings.builtIn) {
            m_customFunctions.append(toCustomFunction(settings));
        }
    }
}

QString HubSettingsState::nextCustomFunctionId() const
{
    int maximum = 0;
    const auto includeId = [&maximum](const QString &id) {
        if (!id.startsWith(QStringLiteral("custom_"))) {
            return;
        }
        bool ok = false;
        const int number = id.mid(7).toInt(&ok);
        if (ok && number > 0) {
            maximum = qMax(maximum, number);
        }
    };
    for (const FunctionSettings &function : m_data.functions) {
        includeId(function.id);
    }
    for (const QString &id :
         m_data.retainedOrphanFunctionFlows.keys()) {
        includeId(id);
    }
    for (const QString &id : m_data.functionOrder) {
        includeId(id);
    }
    return QStringLiteral("custom_%1").arg(maximum + 1);
}

QString HubSettingsState::suggestedCustomShortcut() const
{
    for (int number = 1; number <= 9; ++number) {
        const QString shortcut = QStringLiteral("Ctrl+Alt+%1").arg(number);
        QString title;
        if (!conflictsWithOther(QString(), shortcut, &title)) {
            return shortcut;
        }
    }
    return QString();
}

void HubSettingsState::addCustomFunction(const CustomFunctionDef &function)
{
    const FunctionSettings settings =
        functionSettingsFromCustomFunction(function);
    if (settings.id.isEmpty() || settings.name.isEmpty()) {
        return;
    }
    m_data.functions.append(settings);
    if (!m_data.functionOrder.contains(settings.id)) {
        m_data.functionOrder.append(settings.id);
    }
    refreshCustomFunctions();
}

void HubSettingsState::updateCustomFunction(const CustomFunctionDef &function)
{
    FunctionSettings settings =
        functionSettingsFromCustomFunction(function);
    const int index = m_data.functionIndex(settings.id);
    if (index >= 0 && !m_data.functions.at(index).builtIn) {
        settings.flow = m_data.functions.at(index).flow;
        m_data.functions[index] = settings;
    } else if (index < 0) {
        m_data.functions.append(settings);
    }
    if (!m_data.functionOrder.contains(settings.id)) {
        m_data.functionOrder.append(settings.id);
    }
    refreshCustomFunctions();
}

void HubSettingsState::removeCustomFunction(const QString &id)
{
    const int index = m_data.functionIndex(id);
    if (index >= 0 && !m_data.functions.at(index).builtIn) {
        m_data.functions.remove(index);
    }
    m_data.functionOrder.removeAll(id);
    refreshCustomFunctions();
}

const QVector<PromptLibraryItem> &HubSettingsState::promptLibrary() const
{
    return m_promptLibrary;
}

PromptLibraryItem HubSettingsState::promptLibraryItem(const QString &id) const
{
    for (const PromptLibraryItem &item : m_promptLibrary) {
        if (item.id == id) {
            return item;
        }
    }
    return PromptLibraryItem();
}

QString HubSettingsState::nextPromptLibraryId() const
{
    int maximum = 0;
    for (const PromptLibraryItem &item : m_promptLibrary) {
        if (item.id.startsWith(QStringLiteral("prompt_"))) {
            maximum = qMax(maximum, item.id.mid(7).toInt());
        }
    }
    return QStringLiteral("prompt_%1").arg(maximum + 1);
}

bool HubSettingsState::hasPromptId(const QString &id) const
{
    const QString value = id.trimmed();
    if (value == QStringLiteral("dictate") || value == QStringLiteral("translate")
        || value == QStringLiteral("ask") || m_data.functionIndex(value) >= 0) {
        return true;
    }
    for (const PromptLibraryItem &item : m_promptLibrary) {
        if (item.id == value) {
            return true;
        }
    }
    return false;
}

QString HubSettingsState::promptIdFor(const QString &functionId) const
{
    const FunctionSettings *settings = findFunction(functionId);
    const QString fallback = defaultPromptIdForFunction(functionId);
    if (!settings) {
        return fallback;
    }
    const QString value = settings->promptId.trimmed();
    return hasPromptId(value) ? value : (settings->builtIn ? fallback : settings->id);
}

void HubSettingsState::setPromptIdFor(const QString &functionId, const QString &promptId)
{
    FunctionSettings *settings = mutableFunction(functionId);
    const QString fallback = settings->builtIn
        ? defaultPromptIdForFunction(functionId)
        : settings->id;
    settings->promptId = hasPromptId(promptId) ? promptId.trimmed() : fallback;
    refreshCustomFunctions();
}

void HubSettingsState::addPromptLibraryItem(PromptLibraryItem item)
{
    if (item.id.trimmed().isEmpty()) item.id = nextPromptLibraryId();
    if (item.name.trimmed().isEmpty()) item.name = QString::fromUtf8("新提示词");
    if (item.scope.trimmed().isEmpty()) item.scope = QString::fromUtf8("通用");
    m_promptLibrary.append(item);
}

bool HubSettingsState::updatePromptLibraryItem(const PromptLibraryItem &item)
{
    if (item.id.trimmed().isEmpty() || item.name.trimmed().isEmpty()) return false;
    for (int index = 0; index < m_promptLibrary.size(); ++index) {
        if (m_promptLibrary.at(index).id == item.id) {
            m_promptLibrary[index] = item;
            return true;
        }
    }
    m_promptLibrary.append(item);
    return true;
}

bool HubSettingsState::removePromptLibraryItem(const QString &id)
{
    bool removed = false;
    for (int index = m_promptLibrary.size() - 1; index >= 0; --index) {
        if (m_promptLibrary.at(index).id == id) {
            m_promptLibrary.remove(index);
            removed = true;
        }
    }
    if (!removed) return false;
    for (FunctionSettings &settings : m_data.functions) {
        if (settings.promptId == id) {
            settings.promptId = settings.builtIn
                ? defaultPromptIdForFunction(settings.id)
                : settings.id;
        }
    }
    refreshCustomFunctions();
    return true;
}

bool HubSettingsState::savePromptLibrary() const
{
    return m_access.savePromptLibrary
        ? m_access.savePromptLibrary(m_promptLibrary)
        : false;
}

QString HubSettingsState::modelFor(const QString &id) const
{
    const FunctionSettings *settings = findFunction(id);
    return settings && !settings->modelId.trimmed().isEmpty()
        ? settings->modelId
        : defaultModelForFunction(id);
}

void HubSettingsState::setModelFor(const QString &id, const QString &model)
{
    mutableFunction(id)->modelId = model;
    refreshCustomFunctions();
}

QString HubSettingsState::outputModeFor(const QString &id) const
{
    const FunctionSettings *settings = findFunction(id);
    return normalizeOutputMode(
        settings ? settings->output.outputMode : QString(),
        defaultOutputModeForFunction(id)
    );
}

void HubSettingsState::setOutputModeFor(const QString &id, const QString &mode)
{
    mutableFunction(id)->output.outputMode = normalizeOutputMode(mode, defaultOutputModeForFunction(id));
    refreshCustomFunctions();
}

QString HubSettingsState::floatingBarStyleOverrideFor(
    const QString &id) const
{
    const FunctionSettings *settings = findFunction(id);
    return normalizeFunctionFloatingBarStyle(
        settings
            ? settings->output.floatingBarStyleOverride
            : QString()
    );
}

void HubSettingsState::setFloatingBarStyleOverrideFor(
    const QString &id,
    const QString &style)
{
    mutableFunction(id)->output.floatingBarStyleOverride =
        normalizeFunctionFloatingBarStyle(style);
    refreshCustomFunctions();
}

QStringList HubSettingsState::outputOrderFor(const QString &id) const
{
    const FunctionSettings *settings = findFunction(id);
    return normalizeFunctionOutputOrder(
        settings ? settings->output.order : QStringList()
    );
}

void HubSettingsState::setOutputOrderFor(
    const QString &id,
    const QStringList &order
)
{
    mutableFunction(id)->output.order =
        normalizeFunctionOutputOrder(order);
    refreshCustomFunctions();
}

QString HubSettingsState::resultTemplateFor(const QString &id) const
{
    const FunctionSettings *settings = findFunction(id);
    return normalizeResultTemplate(settings ? settings->output.resultTemplate : QString());
}

void HubSettingsState::setResultTemplateFor(const QString &id, const QString &value)
{
    mutableFunction(id)->output.resultTemplate = normalizeResultTemplate(value);
    refreshCustomFunctions();
}

QStringList HubSettingsState::resultActionsFor(const QString &id) const
{
    const FunctionSettings *settings = findFunction(id);
    return normalizeResultActionIds(settings ? settings->output.resultActions : QStringList());
}

void HubSettingsState::setResultActionsFor(const QString &id, const QStringList &actions)
{
    mutableFunction(id)->output.resultActions = normalizeResultActionIds(actions);
    refreshCustomFunctions();
}

FunctionNetworkPolicies HubSettingsState::networkPoliciesFor(const QString &id) const
{
    const FunctionSettings *settings = findFunction(id);
    return settings ? settings->network : FunctionNetworkPolicies();
}

void HubSettingsState::setNetworkPoliciesFor(const QString &id, const FunctionNetworkPolicies &policies)
{
    FunctionNetworkPolicies normalized = policies;
    normalized.speech = normalizeNetworkPolicy(normalized.speech);
    normalized.ocr = normalizeNetworkPolicy(normalized.ocr);
    normalized.model = normalizeNetworkPolicy(normalized.model);
    mutableFunction(id)->network = normalized;
    refreshCustomFunctions();
}

bool HubSettingsState::useSelectionFor(const QString &id) const
{
    const FunctionSettings *settings = findFunction(id);
    return settings ? settings->input.useSelection : defaultUseSelectionForFunction(id);
}

void HubSettingsState::setUseSelectionFor(const QString &id, bool enabled)
{
    mutableFunction(id)->input.useSelection = enabled;
    refreshCustomFunctions();
}

bool HubSettingsState::useVoiceFor(const QString &id) const
{
    const FunctionSettings *settings = findFunction(id);
    return settings ? settings->input.useVoice : defaultUseVoiceForFunction(id);
}

void HubSettingsState::setUseVoiceFor(const QString &id, bool enabled)
{
    mutableFunction(id)->input.useVoice = enabled;
    refreshCustomFunctions();
}

bool HubSettingsState::useScreenshotFor(const QString &id) const
{
    const FunctionSettings *settings = findFunction(id);
    return settings ? settings->input.useScreenshot : false;
}

void HubSettingsState::setUseScreenshotFor(const QString &id, bool enabled)
{
    mutableFunction(id)->input.useScreenshot = enabled;
    refreshCustomFunctions();
}

QStringList HubSettingsState::inputOrderFor(const QString &id) const
{
    const FunctionSettings *settings = findFunction(id);
    return normalizeFunctionInputOrder(
        settings ? settings->input.order : QStringList()
    );
}

void HubSettingsState::setInputOrderFor(
    const QString &id,
    const QStringList &order
)
{
    mutableFunction(id)->input.order =
        normalizeFunctionInputOrder(order);
    refreshCustomFunctions();
}

QString HubSettingsState::screenshotTriggerModeFor(const QString &id) const
{
    const FunctionSettings *settings = findFunction(id);
    return normalizeScreenshotTriggerMode(settings ? settings->input.screenshotTriggerMode : screenshotTriggerSeparate());
}

void HubSettingsState::setScreenshotTriggerModeFor(const QString &id, const QString &mode)
{
    mutableFunction(id)->input.screenshotTriggerMode = normalizeScreenshotTriggerMode(mode);
    refreshCustomFunctions();
}

QString HubSettingsState::screenshotShortcutFor(const QString &id) const
{
    const FunctionSettings *settings = findFunction(id);
    if (settings && !settings->input.screenshotShortcut.trimmed().isEmpty()) {
        return settings->input.screenshotShortcut.trimmed();
    }
    return settings && !settings->shortcut.trimmed().isEmpty()
        ? screenshotShortcutFromFunctionShortcut(settings->shortcut)
        : defaultScreenshotShortcutForFunction(id);
}

void HubSettingsState::setScreenshotShortcutFor(const QString &id, const QString &shortcut)
{
    mutableFunction(id)->input.screenshotShortcut = shortcut.trimmed();
    refreshCustomFunctions();
}

int HubSettingsState::floatingBarSecondsFor(const QString &id) const
{
    const FunctionSettings *settings = findFunction(id);
    return qBound(0, settings ? settings->output.floatingBarSeconds : defaultFloatingBarSeconds(), 60);
}

void HubSettingsState::setFloatingBarSecondsFor(const QString &id, int seconds)
{
    mutableFunction(id)->output.floatingBarSeconds = qBound(0, seconds, 60);
    refreshCustomFunctions();
}

int HubSettingsState::resultPopupSecondsFor(const QString &id) const
{
    const FunctionSettings *settings = findFunction(id);
    return qBound(0, settings ? settings->output.resultPopupSeconds : defaultResultPopupSeconds(), 600);
}

void HubSettingsState::setResultPopupSecondsFor(const QString &id, int seconds)
{
    mutableFunction(id)->output.resultPopupSeconds = qBound(0, seconds, 600);
    refreshCustomFunctions();
}

int HubSettingsState::countdownSecondsFor(const QString &id) const
{
    const FunctionSettings *settings = findFunction(id);
    return qBound(0, settings ? settings->recording.countdownSeconds : defaultCountdownSeconds(), 60);
}

void HubSettingsState::setCountdownSecondsFor(const QString &id, int seconds)
{
    mutableFunction(id)->recording.countdownSeconds = qBound(0, seconds, 60);
    refreshCustomFunctions();
}

bool HubSettingsState::recordingBeepEnabledFor(const QString &id) const
{
    const FunctionSettings *settings = findFunction(id);
    return settings ? settings->recording.beepEnabled : true;
}

void HubSettingsState::setRecordingBeepEnabledFor(const QString &id, bool enabled)
{
    mutableFunction(id)->recording.beepEnabled = enabled;
    refreshCustomFunctions();
}

QString HubSettingsState::recordingBeepPathFor(const QString &id) const
{
    const FunctionSettings *settings = findFunction(id);
    return settings ? settings->recording.beepPath.trimmed() : QString();
}

void HubSettingsState::setRecordingBeepPathFor(const QString &id, const QString &path)
{
    mutableFunction(id)->recording.beepPath = path.trimmed();
    refreshCustomFunctions();
}

QString HubSettingsState::recordingTriggerModeFor(const QString &id) const
{
    const FunctionSettings *settings = findFunction(id);
    return normalizeRecordingTriggerMode(settings ? settings->recording.triggerMode : QStringLiteral("toggle"));
}

void HubSettingsState::setRecordingTriggerModeFor(const QString &id, const QString &mode)
{
    mutableFunction(id)->recording.triggerMode = normalizeRecordingTriggerMode(mode);
    refreshCustomFunctions();
}

bool HubSettingsState::longRecordingEnabledFor(const QString &id) const
{
    const FunctionSettings *settings = findFunction(id);
    return settings ? settings->recording.longRecordingEnabled : false;
}

void HubSettingsState::setLongRecordingEnabledFor(const QString &id, bool enabled)
{
    mutableFunction(id)->recording.longRecordingEnabled = enabled;
    refreshCustomFunctions();
}

int HubSettingsState::segmentSecondsFor(const QString &id) const
{
    const FunctionSettings *settings = findFunction(id);
    return normalizeRecordingSegmentSeconds(settings ? settings->recording.segmentSeconds : 55);
}

void HubSettingsState::setSegmentSecondsFor(const QString &id, int seconds)
{
    mutableFunction(id)->recording.segmentSeconds = normalizeRecordingSegmentSeconds(seconds);
    refreshCustomFunctions();
}

int HubSettingsState::maxRecordingMinutesFor(const QString &id) const
{
    const FunctionSettings *settings = findFunction(id);
    return normalizeMaxRecordingMinutes(settings ? settings->recording.maximumMinutes : 30);
}

void HubSettingsState::setMaxRecordingMinutesFor(const QString &id, int minutes)
{
    mutableFunction(id)->recording.maximumMinutes = normalizeMaxRecordingMinutes(minutes);
    refreshCustomFunctions();
}

QStringList HubSettingsState::defaultFunctionOrderIds() const
{
    QStringList ids;
    for (const FunctionSettings &settings : m_data.functions) {
        if (settings.builtIn) ids.append(settings.id);
    }
    for (const FunctionSettings &settings : m_data.functions) {
        if (!settings.builtIn) ids.append(settings.id);
    }
    return ids;
}

QStringList HubSettingsState::functionOrderIds() const
{
    const QStringList defaults = defaultFunctionOrderIds();
    QStringList result;
    for (const QString &id : m_data.functionOrder) {
        if (defaults.contains(id) && !result.contains(id)) result.append(id);
    }
    for (const QString &id : defaults) {
        if (!result.contains(id)) result.append(id);
    }
    return result;
}

bool HubSettingsState::setFunctionOrderIds(const QStringList &ids)
{
    QStringList normalized;
    const QStringList defaults = defaultFunctionOrderIds();
    for (const QString &id : ids) {
        if (defaults.contains(id) && !normalized.contains(id)) normalized.append(id);
    }
    for (const QString &id : defaults) {
        if (!normalized.contains(id)) normalized.append(id);
    }
    if (normalized == functionOrderIds()) return false;
    m_data.functionOrder = normalized;
    return true;
}

const QStringList &HubSettingsState::favoriteFolders() const
{
    return m_data.favoriteFolders;
}

bool HubSettingsState::addFavoriteFolder(const QString &name)
{
    const QString value = name.trimmed();
    if (value.isEmpty() || m_data.favoriteFolders.contains(value)) {
        return false;
    }
    m_data.favoriteFolders.append(value);
    return true;
}

int HubSettingsState::historyInitialLoadCount() const
{
    return qBound(5, m_data.historyInitialLoadCount, 200);
}

int HubSettingsState::historyLoadMoreCount() const
{
    return qBound(5, m_data.historyLoadMoreCount, 200);
}

bool HubSettingsState::trayResident() const { return m_data.trayResident; }
bool HubSettingsState::autoStartEnabled() const { return m_data.autoStartEnabled; }
bool HubSettingsState::strongSelectionEnabled() const { return m_data.strongSelectionEnabled; }
void HubSettingsState::setStrongSelectionEnabled(bool enabled) { m_data.strongSelectionEnabled = enabled; }
SelectionContextSettings HubSettingsState::selectionContextSettings() const
{
    return normalizedHubSelectionContextSettings(
        m_data.selectionContext,
        m_data
    );
}
void HubSettingsState::setSelectionContextSettings(
    const SelectionContextSettings &settings)
{
    AppSettingsData completeSettings = m_data;
    completeSettings.selectionContext = settings;
    m_data.selectionContext = normalizedHubSelectionContextSettings(
        settings,
        completeSettings
    );
}
bool HubSettingsState::floatingBarEnabled() const { return m_data.floatingBarEnabled; }
void HubSettingsState::setFloatingBarEnabled(bool enabled) { m_data.floatingBarEnabled = enabled; }
bool HubSettingsState::promptLocked() const { return m_data.promptLocked; }
void HubSettingsState::setPromptLocked(bool locked) { m_data.promptLocked = locked; }
bool HubSettingsState::useSystemProxy() const { return m_data.useSystemProxy; }
void HubSettingsState::setUseSystemProxy(bool enabled) { m_data.useSystemProxy = enabled; }
int HubSettingsState::resultPopupOpacity() const { return qBound(60, m_data.resultPopupOpacity, 100); }

QString HubSettingsState::recordDirectoryPath() const
{
    const QString path = m_data.recordDirectory.trimmed();
    if (path.isEmpty() || path == QString::fromUtf8("本地按日期保存")) return defaultRecordDirectory();
    return QDir(path).isRelative()
        ? QDir(appBasePath()).absoluteFilePath(path)
        : QDir::cleanPath(path);
}

void HubSettingsState::setRecordDirectoryPath(const QString &path)
{
    const QString value = path.trimmed();
    if (value.isEmpty() || value == QStringLiteral("records")) {
        m_data.recordDirectory.clear();
        return;
    }
    const QString clean = QDir(value).isRelative()
        ? QDir::cleanPath(QDir(appBasePath()).absoluteFilePath(value))
        : QDir::cleanPath(value);
    m_data.recordDirectory = clean == QDir::cleanPath(defaultRecordDirectory())
        ? QString()
        : clean;
}

void HubSettingsState::resetRecordDirectory() { m_data.recordDirectory.clear(); }
QString HubSettingsState::speechProvider() const { return normalizeSpeechProvider(m_data.speechProvider); }
void HubSettingsState::setSpeechProvider(const QString &provider) { m_data.speechProvider = normalizeSpeechProvider(provider); }
QString HubSettingsState::windowsSpeechLanguage() const
{
    return normalizeWindowsSpeechLanguage(m_data.windowsSpeechLanguage);
}
void HubSettingsState::setWindowsSpeechLanguage(const QString &language)
{
    m_data.windowsSpeechLanguage = normalizeWindowsSpeechLanguage(language);
}
QString HubSettingsState::ocrEngine() const { return normalizeOcrEngine(m_data.ocrEngine); }
void HubSettingsState::setOcrEngine(const QString &engine) { m_data.ocrEngine = normalizeOcrEngine(engine); }

bool HubSettingsState::hasFloatingBarPosition() const { return m_data.windows.hasFloatingBarPosition; }
QPoint HubSettingsState::floatingBarPosition() const { return m_data.windows.floatingBarPosition; }
void HubSettingsState::setFloatingBarPosition(const QPoint &position)
{
    m_data.windows.hasFloatingBarPosition = true;
    m_data.windows.floatingBarPosition = position;
}
bool HubSettingsState::hasResultPopupGeometry() const { return m_data.windows.hasResultPopupGeometry; }
QRect HubSettingsState::resultPopupGeometry() const { return m_data.windows.resultPopupGeometry; }
void HubSettingsState::setResultPopupGeometry(const QRect &geometry)
{
    if (geometry.width() <= 0 || geometry.height() <= 0) return;
    m_data.windows.hasResultPopupGeometry = true;
    m_data.windows.resultPopupGeometry = QRect(
        geometry.topLeft(),
        QSize(qBound(640, geometry.width(), 2000), qBound(460, geometry.height(), 1600))
    );
}
bool HubSettingsState::hasScreenshotLauncherPosition() const { return m_data.windows.hasScreenshotLauncherPosition; }
QPoint HubSettingsState::screenshotLauncherPosition() const { return m_data.windows.screenshotLauncherPosition; }
void HubSettingsState::setScreenshotLauncherPosition(const QPoint &position)
{
    m_data.windows.hasScreenshotLauncherPosition = true;
    m_data.windows.screenshotLauncherPosition = position;
}
