#include "hub_refresh_coordinator_bundle.h"

HubRefreshCoordinatorBundle::HubRefreshCoordinatorBundle(
    const HubRefreshCoordinatorBundleActions &actions
)
    : m_settings(new HubSettingsRefreshCoordinator(actions.settings))
    , m_content(new HubContentRefreshCoordinator(actions.content))
{
    setApplicationEvents(nullptr);
}

void HubRefreshCoordinatorBundle::setApplicationEvents(
    ApplicationEvents *events
)
{
    HubApplicationEventCoordinatorCallbacks callbacks;
    callbacks.settingsChanged = [this](const SettingsChangeSet &) {
        applySettingsChanged();
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

void HubRefreshCoordinatorBundle::applySettingsChanged()
{
    m_settings->apply();
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
