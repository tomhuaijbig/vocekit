#include "hub_refresh_coordinator_bundle.h"

namespace {

QStringList uniqueFunctionIds(const QStringList &functionIds)
{
    QStringList uniqueIds;
    for (const QString &functionId : functionIds) {
        const QString normalizedId = functionId.trimmed();
        if (!normalizedId.isEmpty()
            && !uniqueIds.contains(normalizedId)) {
            uniqueIds.append(normalizedId);
        }
    }
    return uniqueIds;
}

} // namespace

HubRefreshCoordinatorBundle::HubRefreshCoordinatorBundle(
    const HubRefreshCoordinatorBundleActions &actions
)
    : m_actions(actions)
    , m_settings(new HubSettingsRefreshCoordinator(actions.settings))
    , m_content(new HubContentRefreshCoordinator(actions.content))
{
    setApplicationEvents(nullptr);
}

void HubRefreshCoordinatorBundle::setApplicationEvents(
    ApplicationEvents *events
)
{
    HubApplicationEventCoordinatorCallbacks callbacks;
    callbacks.settingsChanged = [this](const SettingsChangeSet &change) {
        apply(change);
    };
    callbacks.historyChanged = [this](const HistoryChangeSet &change) {
        m_content->applyHistoryChanged(change);
    };
    callbacks.vocabularyChanged = [this](
        const VocabularyChangeSet &change
    ) {
        m_content->applyVocabularyChanged(change);
    };
    m_events.reset(new HubApplicationEventCoordinator(events, callbacks));
}

void HubRefreshCoordinatorBundle::apply(const SettingsChangeSet &change)
{
    const QStringList functionIds =
        uniqueFunctionIds(change.functionIds);
    const QStringList recognizedKeys = QStringList()
        << functionDefinitionsSettingsKey()
        << functionFlowDraftSettingsKey()
        << functionFlowEditorStateSettingsKey()
        << functionFlowPublishedSettingsKey()
        << functionExecutionModeSettingsKey();

    bool hasUnknownKey = change.keys.isEmpty();
    for (const QString &key : change.keys) {
        if (!recognizedKeys.contains(key)) {
            hasUnknownKey = true;
            break;
        }
    }
    const bool definitionsChanged =
        change.keys.contains(functionDefinitionsSettingsKey());
    const bool draftChanged =
        change.keys.contains(functionFlowDraftSettingsKey());
    const bool editorChanged =
        change.keys.contains(functionFlowEditorStateSettingsKey());
    const bool publishedChanged =
        change.keys.contains(functionFlowPublishedSettingsKey());
    const bool modeChanged =
        change.keys.contains(functionExecutionModeSettingsKey());
    const bool hasFunctionScopedChange =
        draftChanged || editorChanged || publishedChanged || modeChanged;
    const bool runtimeChanged =
        definitionsChanged
        || publishedChanged
        || modeChanged;
    const bool fullUiRefresh =
        hasUnknownKey || definitionsChanged
        || (hasFunctionScopedChange && functionIds.isEmpty());

    if (fullUiRefresh) {
        m_settings->apply();
    } else {
        if (m_actions.reloadFunctionFlows) {
            m_actions.reloadFunctionFlows(functionIds);
        }
        if (m_actions.refreshActiveCanvas) {
            m_actions.refreshActiveCanvas();
        }
    }

    if (modeChanged && !fullUiRefresh
        && m_actions.refreshActiveFunction) {
        m_actions.refreshActiveFunction();
    }
    if (runtimeChanged && m_actions.refreshRuntime) {
        m_actions.refreshRuntime(functionIds);
    }
    if (runtimeChanged && m_actions.refreshHotkeys) {
        m_actions.refreshHotkeys(functionIds);
    }
}

void HubRefreshCoordinatorBundle::dispatchSettingsChanged(
    const QStringList &keys,
    const QStringList &functionIds
)
{
    m_events->dispatchSettingsChanged(keys, functionIds);
}

void HubRefreshCoordinatorBundle::dispatchHistoryChanged(
    const QStringList &recordIds,
    bool resetRequired
)
{
    m_events->dispatchHistoryChanged(recordIds, resetRequired);
}

void HubRefreshCoordinatorBundle::dispatchVocabularyChanged(
    const QStringList &entryIds,
    bool resetRequired
)
{
    m_events->dispatchVocabularyChanged(entryIds, resetRequired);
}
