#ifndef VOCEKIT_HUB_REFRESH_COORDINATOR_ACTION_FACTORY_H
#define VOCEKIT_HUB_REFRESH_COORDINATOR_ACTION_FACTORY_H

#include "hub_refresh_coordinator_bundle.h"
#include "log_pagination_snapshot.h"

#include <functional>

// 刷新协调器需要的数据访问动作，与具体页面类型解耦。
struct HubRefreshDataAccess
{
    std::function<void()> reloadSettings;
    std::function<void(const QStringList &)> reloadFunctionFlows;
    std::function<AppSettingsData()> settingsSnapshot;
    std::function<bool()> historyCacheValid;
    std::function<void(bool)> refreshHistory;
    std::function<void()> invalidateHistoryCache;
    std::function<bool()> historyPageCreated;
};

// 主窗口真正需要提供的可见界面刷新动作。
struct HubRefreshUiActions
{
    std::function<void()> refreshModeGrid;
    std::function<void()> refreshStatus;
    std::function<void()> refreshPrompts;
    std::function<void()> refreshFunctions;
    std::function<void()> refreshNavigation;
    std::function<void()> refreshActiveFunction;
    std::function<void()> refreshActiveCanvas;
    std::function<void(const QStringList &)> refreshRuntime;
    std::function<void(const QStringList &)> refreshHotkeys;
    std::function<void()> refreshOcr;
    std::function<void()> refreshRecentHistory;
    std::function<void()> refreshVocabulary;
    std::function<void(const LogPaginationSnapshot &)> updateLogPagination;
};

// 在页面侧已经提供函数式历史访问时，只补充设置刷新动作。
template <typename SettingsState>
HubRefreshDataAccess createHubRefreshDataAccess(
    SettingsState *settings,
    const HubRefreshDataAccess &historyAccess
)
{
    HubRefreshDataAccess access = historyAccess;
    if (settings) {
        access.reloadSettings = [settings]() { settings->load(); };
        access.settingsSnapshot = [settings]() {
            return settings->toData();
        };
    }
    return access;
}

template <typename SettingsState, typename HistoryController>
HubRefreshDataAccess createHubRefreshDataAccess(
    SettingsState *settings,
    HistoryController *history
)
{
    HubRefreshDataAccess access;
    if (history) {
        access.historyCacheValid = [history]() {
            return history->historyCacheValid();
        };
        access.refreshHistory = [history](bool resetRequired) {
            history->refreshTabs(resetRequired);
        };
        access.invalidateHistoryCache = [history]() {
            history->invalidateCache();
        };
        access.historyPageCreated = [history]() {
            return history->pageCreated();
        };
    }
    return createHubRefreshDataAccess(settings, access);
}

HubRefreshCoordinatorBundleActions createHubRefreshCoordinatorActions(
    const HubRefreshDataAccess &data,
    const HubRefreshUiActions &ui
);

#endif // VOCEKIT_HUB_REFRESH_COORDINATOR_ACTION_FACTORY_H
