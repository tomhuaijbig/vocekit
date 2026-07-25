#ifndef VOCEKIT_HUB_PAGE_COMPOSITION_H
#define VOCEKIT_HUB_PAGE_COMPOSITION_H

#include "hub_page_router.h"

#include <functional>

using HubPageFactory = std::function<QWidget *()>;
using HubPageActivation = std::function<void(bool pageChanged)>;

struct HubPageCompositionAccess
{
    HubPageFactory homePage;
    HubPageFactory functionPage;
    HubPageFactory historyPage;
    HubPageFactory vocabularyPage;
    HubPageFactory ocrPage;
    HubPageFactory promptsPage;
    HubPageFactory diagnosticsPage;
    HubPageFactory logsPage;
    HubPageFactory settingsPage;
    HubPageFactory faqPage;

    HubPageActivation homeActivated;
    HubPageActivation functionActivated;
    HubPageActivation historyActivated;
    HubPageActivation vocabularyActivated;
    HubPageActivation ocrActivated;
    HubPageActivation promptsActivated;
    HubPageActivation diagnosticsActivated;
    HubPageActivation logsActivated;
    HubPageActivation settingsActivated;
    HubPageActivation faqActivated;
};

// 固定页面目录、注册顺序和控件所有权集中在装配器，主窗口只提供工厂与激活动作。
class HubPageComposition
{
public:
    static HubPageRouter *create(
        const HubPageCompositionAccess &access,
        QWidget *parent = nullptr
    );
};

#endif // VOCEKIT_HUB_PAGE_COMPOSITION_H
