#ifndef VOCEKIT_HUB_PAGE_COMPOSITION_ACCESS_FACTORY_H
#define VOCEKIT_HUB_PAGE_COMPOSITION_ACCESS_FACTORY_H

#include "hub_page_composition.h"

class QObject;

// 页面装配所需的工厂和刷新动作。页面激活时机由独立工厂统一转换，
// 主窗口只提供具体页面及刷新实现。
struct HubPageCompositionAccessFactoryDependencies
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

    QObject *deferredContext = nullptr;
    std::function<void()> refreshHistory;
    std::function<void()> refreshVocabulary;
    std::function<void()> refreshOcr;
    std::function<void()> refreshPrompts;
    std::function<void()> refreshDiagnostics;
    std::function<void()> refreshLogs;
    std::function<void()> refreshSettings;
};

HubPageCompositionAccess createHubPageCompositionAccess(
    const HubPageCompositionAccessFactoryDependencies &dependencies
);

#endif // VOCEKIT_HUB_PAGE_COMPOSITION_ACCESS_FACTORY_H
