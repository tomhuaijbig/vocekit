#include "hub_content_refresh_coordinator.h"

namespace {

void run(const std::function<void()> &action)
{
    if (action) {
        action();
    }
}

} // namespace

HubContentRefreshCoordinator::HubContentRefreshCoordinator(
    const HubContentRefreshCoordinatorActions &actions
)
    : m_actions(actions)
{
}

void HubContentRefreshCoordinator::applyHistoryChanged(
    const HistoryChangeSet &change
) const
{
    run(m_actions.invalidateHistoryCache);
    run(m_actions.refreshRecentHistory);
    if (m_actions.historyPageCreated && m_actions.historyPageCreated()) {
        if (m_actions.refreshHistory) {
            m_actions.refreshHistory(change.resetRequired);
        }
    }
}

void HubContentRefreshCoordinator::applyVocabularyChanged(
    const VocabularyChangeSet &change
) const
{
    Q_UNUSED(change);
    run(m_actions.refreshVocabulary);
    run(m_actions.refreshActiveFunction);
}
