#include "home_page_access_factory.h"

#include "current_status_snapshot.h"
#include "function_mode_grid_access_factory.h"
#include "hub_settings_state.h"

HomePageAccess createHomePageAccess(
    const HomePageAccessDependencies &dependencies
)
{
    HomePageAccess access;
    access.functionModes = createFunctionModeGridAccess(dependencies.settings);
    access.openFunction = dependencies.openFunction;
    access.settingsChanged = dependencies.settingsChanged;
    access.showWarning = dependencies.showWarning;

    access.recentEntries = dependencies.recentEntries
        ? dependencies.recentEntries
        : []() { return QVector<HistoryEntry>(); };

    const std::function<QVector<HistoryTabDef>()> historyTabs =
        dependencies.historyTabs;
    access.recentTabs = [historyTabs]() {
        QVector<RecentHistoryPanel::TabSpec> tabs;
        if (!historyTabs) {
            return tabs;
        }
        for (const HistoryTabDef &mode : historyTabs()) {
            RecentHistoryPanel::TabSpec tab;
            tab.id = mode.id;
            tab.title = mode.title;
            tabs.append(tab);
        }
        return tabs;
    };

    access.recentListFactory = dependencies.recentListFactory
        ? dependencies.recentListFactory
        : [](
            const QString &,
            const QVector<HistoryEntry> &,
            int
        ) -> QWidget * {
            return nullptr;
        };

    HubSettingsState *settings = dependencies.settings;
    access.currentStatus = [settings]() {
        return settings
            ? buildCurrentStatusSnapshot(settings->toData())
            : CurrentStatusSnapshot();
    };
    return access;
}
