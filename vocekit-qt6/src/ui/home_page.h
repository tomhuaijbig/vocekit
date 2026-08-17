#ifndef VOCEKIT_HOME_PAGE_H
#define VOCEKIT_HOME_PAGE_H

#include "current_status_snapshot.h"
#include "function_mode_grid.h"
#include "recent_history_panel.h"

#include <QWidget>

#include <functional>

class CurrentStatusPanel;

// 主页面只负责组合功能卡片、最近记录和当前状态，不直接读取配置或历史文件。
struct HomePageAccess
{
    FunctionModeGridAccess functionModes;
    FunctionModeGrid::OpenCallback openFunction;
    FunctionModeGrid::SettingsChangedCallback settingsChanged;
    FunctionModeGrid::WarningCallback showWarning;
    RecentHistoryPanel::EntriesProvider recentEntries;
    RecentHistoryPanel::TabsProvider recentTabs;
    RecentHistoryPanel::ListFactory recentListFactory;
    std::function<CurrentStatusSnapshot()> currentStatus;
};

class HomePage : public QWidget
{
public:
    explicit HomePage(
        const HomePageAccess &access,
        QWidget *parent = nullptr
    );

    void refreshFunctionModes();
    void refreshRecentHistory();
    void refreshCurrentStatus();

private:
    HomePageAccess m_access;
    FunctionModeGrid *m_functionModes = nullptr;
    RecentHistoryPanel *m_recentHistory = nullptr;
    CurrentStatusPanel *m_currentStatus = nullptr;
};

#endif // VOCEKIT_HOME_PAGE_H
