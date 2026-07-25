#ifndef VOCEKIT_HUB_CONTENT_REFRESH_COORDINATOR_H
#define VOCEKIT_HUB_CONTENT_REFRESH_COORDINATOR_H

#include "../app/application_events.h"

#include <functional>

// 历史和词库内容变化后的刷新动作。协调器统一事件与本地回退路径。
struct HubContentRefreshCoordinatorActions
{
    std::function<void()> invalidateHistoryCache;
    std::function<void()> refreshRecentHistory;
    std::function<bool()> historyPageCreated;
    std::function<void(bool)> refreshHistory;
    std::function<void()> refreshVocabulary;
    std::function<void()> refreshActiveFunction;
};

class HubContentRefreshCoordinator
{
public:
    explicit HubContentRefreshCoordinator(
        const HubContentRefreshCoordinatorActions &actions
    );

    void applyHistoryChanged(const HistoryChangeSet &change) const;
    void applyVocabularyChanged(const VocabularyChangeSet &change) const;

private:
    HubContentRefreshCoordinatorActions m_actions;
};

#endif // VOCEKIT_HUB_CONTENT_REFRESH_COORDINATOR_H
