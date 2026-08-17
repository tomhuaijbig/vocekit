#ifndef VOCEKIT_HUB_SETTINGS_REFRESH_COORDINATOR_H
#define VOCEKIT_HUB_SETTINGS_REFRESH_COORDINATOR_H

#include <functional>

// 设置变化后的刷新动作。协调器固定执行顺序，主窗口只提供每一步实现。
struct HubSettingsRefreshCoordinatorActions
{
    std::function<void()> reloadSettings;
    std::function<void()> refreshModeGrid;
    std::function<void()> refreshStatus;
    std::function<void()> refreshPrompts;
    std::function<void()> refreshFunctions;
    std::function<void()> refreshNavigation;
    std::function<void()> refreshActiveFunction;
    std::function<void()> refreshOcr;
    std::function<void()> refreshLogs;
    std::function<bool()> historyCacheValid;
    std::function<void()> refreshHistory;
    std::function<void()> refreshRecentHistory;
};

class HubSettingsRefreshCoordinator
{
public:
    explicit HubSettingsRefreshCoordinator(
        const HubSettingsRefreshCoordinatorActions &actions
    );

    void apply() const;

private:
    HubSettingsRefreshCoordinatorActions m_actions;
};

#endif // VOCEKIT_HUB_SETTINGS_REFRESH_COORDINATOR_H
