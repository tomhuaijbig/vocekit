#include "hub_home_page_controller.h"

#include <QPointer>
#include <QWidget>

class HubHomePageController::Impl
{
public:
    Impl(const HomePageAccess &pageAccess, QWidget *pageParent)
        : access(pageAccess), parent(pageParent)
    {
    }

    HomePageAccess access;
    QPointer<QWidget> parent;
    QPointer<HomePage> page;
};

HubHomePageController::HubHomePageController(
    const HomePageAccess &access,
    QWidget *parent
)
    : m_impl(new Impl(access, parent))
{
}

HubHomePageController::~HubHomePageController() = default;

QWidget *HubHomePageController::page()
{
    if (!m_impl->page) {
        m_impl->page = new HomePage(m_impl->access, m_impl->parent.data());
    }
    return m_impl->page.data();
}

HomePage *HubHomePageController::pageWidget() const
{
    return m_impl->page.data();
}

bool HubHomePageController::pageCreated() const
{
    return !m_impl->page.isNull();
}

void HubHomePageController::refreshFunctionModes()
{
    if (m_impl->page) {
        m_impl->page->refreshFunctionModes();
    }
}

void HubHomePageController::refreshRecentHistory()
{
    if (m_impl->page) {
        m_impl->page->refreshRecentHistory();
    }
}

void HubHomePageController::refreshCurrentStatus()
{
    if (m_impl->page) {
        m_impl->page->refreshCurrentStatus();
    }
}
