#ifndef VOCEKIT_HUB_REFRESH_COORDINATOR_BUNDLE_H
#define VOCEKIT_HUB_REFRESH_COORDINATOR_BUNDLE_H

#include "hub_application_event_coordinator.h"
#include "hub_content_refresh_coordinator.h"
#include "hub_settings_refresh_coordinator.h"

#include <QScopedPointer>

// 主窗口刷新协调器的统一装配参数。
struct HubRefreshCoordinatorBundleActions
{
    HubSettingsRefreshCoordinatorActions settings;
    HubContentRefreshCoordinatorActions content;
    std::function<void(const QStringList &)> reloadFunctionFlows;
    std::function<void()> refreshActiveFunction;
    std::function<void()> refreshActiveCanvas;
    std::function<void(const QStringList &)> refreshRuntime;
    std::function<void(const QStringList &)> refreshHotkeys;
};

// 统一拥有设置、内容和应用事件协调器，隐藏它们之间的转发关系。
class HubRefreshCoordinatorBundle
{
public:
    explicit HubRefreshCoordinatorBundle(
        const HubRefreshCoordinatorBundleActions &actions
    );

    void setApplicationEvents(ApplicationEvents *events);
    void apply(const SettingsChangeSet &change);
    void dispatchSettingsChanged(
        const QStringList &keys,
        const QStringList &functionIds
    );
    void dispatchHistoryChanged(
        const QStringList &recordIds,
        bool resetRequired
    );
    void dispatchVocabularyChanged(
        const QStringList &entryIds,
        bool resetRequired
    );

private:
    HubRefreshCoordinatorBundleActions m_actions;
    QScopedPointer<HubSettingsRefreshCoordinator> m_settings;
    QScopedPointer<HubContentRefreshCoordinator> m_content;
    QScopedPointer<HubApplicationEventCoordinator> m_events;
};

#endif // VOCEKIT_HUB_REFRESH_COORDINATOR_BUNDLE_H
