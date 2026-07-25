#ifndef VOCEKIT_HUB_HOME_PAGE_CONTROLLER_H
#define VOCEKIT_HUB_HOME_PAGE_CONTROLLER_H

#include "home_page.h"

#include <QScopedPointer>

class QWidget;

// 首页控制器负责页面的按需创建、实例跟踪和局部刷新。
class HubHomePageController
{
public:
    explicit HubHomePageController(
        const HomePageAccess &access,
        QWidget *parent = nullptr
    );
    ~HubHomePageController();

    QWidget *page();
    HomePage *pageWidget() const;
    bool pageCreated() const;

    void refreshFunctionModes();
    void refreshRecentHistory();
    void refreshCurrentStatus();

private:
    HubHomePageController(const HubHomePageController &) = delete;
    HubHomePageController &operator=(
        const HubHomePageController &
    ) = delete;

    class Impl;
    QScopedPointer<Impl> m_impl;
};

#endif // VOCEKIT_HUB_HOME_PAGE_CONTROLLER_H
