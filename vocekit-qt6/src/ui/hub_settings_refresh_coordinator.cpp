#include "hub_settings_refresh_coordinator.h"

namespace {

void run(const std::function<void()> &action)
{
    if (action) {
        action();
    }
}

} // namespace

HubSettingsRefreshCoordinator::HubSettingsRefreshCoordinator(
    const HubSettingsRefreshCoordinatorActions &actions
)
    : m_actions(actions)
{
}

void HubSettingsRefreshCoordinator::apply() const
{
    run(m_actions.reloadSettings);
    run(m_actions.refreshModeGrid);
    run(m_actions.refreshStatus);
    run(m_actions.refreshPrompts);
    run(m_actions.refreshFunctions);
    run(m_actions.refreshNavigation);
    run(m_actions.refreshActiveFunction);
    run(m_actions.refreshOcr);
    run(m_actions.refreshLogs);
    if (m_actions.historyCacheValid && m_actions.historyCacheValid()) {
        run(m_actions.refreshHistory);
    }
    run(m_actions.refreshRecentHistory);
}
