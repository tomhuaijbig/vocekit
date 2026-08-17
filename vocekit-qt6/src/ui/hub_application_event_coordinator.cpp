#include "hub_application_event_coordinator.h"

HubApplicationEventCoordinator::HubApplicationEventCoordinator(
    ApplicationEvents *events,
    const HubApplicationEventCoordinatorCallbacks &callbacks,
    QObject *parent
)
    : QObject(parent)
    , m_events(events)
    , m_callbacks(callbacks)
{
    if (!events) {
        return;
    }

    connect(
        events,
        &ApplicationEvents::settingsChanged,
        this,
        [this](SettingsChangeSet change) {
            if (m_callbacks.settingsChanged) {
                m_callbacks.settingsChanged(change);
            }
        }
    );

    connect(
        events,
        &ApplicationEvents::historyChanged,
        this,
        [this](HistoryChangeSet change) {
            if (m_callbacks.historyChanged) {
                m_callbacks.historyChanged(change);
            }
        }
    );

    connect(
        events,
        &ApplicationEvents::vocabularyChanged,
        this,
        [this](VocabularyChangeSet change) {
            if (m_callbacks.vocabularyChanged) {
                m_callbacks.vocabularyChanged(change);
            }
        }
    );
}

void HubApplicationEventCoordinator::dispatchSettingsChanged(
    const QStringList &keys,
    const QStringList &functionIds
)
{
    SettingsChangeSet change;
    change.keys = keys;
    change.functionIds = functionIds;
    if (m_events) {
        m_events->publishSettingsChanged(change);
    } else if (m_callbacks.settingsChanged) {
        m_callbacks.settingsChanged(change);
    }
}

void HubApplicationEventCoordinator::dispatchHistoryChanged(
    const QStringList &recordIds,
    bool resetRequired
)
{
    HistoryChangeSet change;
    change.recordIds = recordIds;
    change.resetRequired = resetRequired;
    if (m_events) {
        m_events->publishHistoryChanged(change);
    } else if (m_callbacks.historyChanged) {
        m_callbacks.historyChanged(change);
    }
}

void HubApplicationEventCoordinator::dispatchVocabularyChanged(
    const QStringList &entryIds,
    bool resetRequired
)
{
    VocabularyChangeSet change;
    change.entryIds = entryIds;
    change.resetRequired = resetRequired;
    if (m_events) {
        m_events->publishVocabularyChanged(change);
    } else if (m_callbacks.vocabularyChanged) {
        m_callbacks.vocabularyChanged(change);
    }
}
