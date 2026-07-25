#ifndef VOCEKIT_HOME_PAGE_ACCESS_FACTORY_H
#define VOCEKIT_HOME_PAGE_ACCESS_FACTORY_H

#include "home_page.h"

#include "../domain/history_types.h"

class HubSettingsState;

// Composition input for the home page. The main window provides actions and
// data sources; the factory converts them to the smaller HomePage contract.
struct HomePageAccessDependencies
{
    HubSettingsState *settings = nullptr;
    FunctionModeGrid::EditCallback editFunction;
    FunctionModeGrid::SettingsChangedCallback settingsChanged;
    FunctionModeGrid::WarningCallback showWarning;
    RecentHistoryPanel::EntriesProvider recentEntries;
    std::function<QVector<HistoryTabDef>()> historyTabs;
    RecentHistoryPanel::ListFactory recentListFactory;
};

HomePageAccess createHomePageAccess(
    const HomePageAccessDependencies &dependencies
);

#endif // VOCEKIT_HOME_PAGE_ACCESS_FACTORY_H
