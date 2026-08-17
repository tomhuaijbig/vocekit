#include "hub_page_composition.h"

namespace {

struct PageSpec
{
    const char *id;
    HubPageFactory factory;
    HubPageActivation activated;
    bool eager;
};

void registerPage(HubPageRouter *router, const PageSpec &spec)
{
    if (!router || !spec.id || !spec.factory) {
        return;
    }

    if (!spec.eager) {
        router->registerDeferredPage(
            QString::fromLatin1(spec.id),
            spec.factory,
            spec.activated
        );
        return;
    }

    QWidget *page = spec.factory();
    if (!page) {
        return;
    }

    HubPageRegistration registration;
    registration.id = QString::fromLatin1(spec.id);
    registration.page = page;
    registration.activated = spec.activated;
    if (!router->registerPage(registration)) {
        delete page;
    }
}

} // namespace

HubPageRouter *HubPageComposition::create(
    const HubPageCompositionAccess &access,
    QWidget *parent)
{
    auto *router = new HubPageRouter(parent);
    const QVector<PageSpec> pages = {
        {"home", access.homePage, access.homeActivated, true},
        {"function", access.functionPage, access.functionActivated, false},
        {"history", access.historyPage, access.historyActivated, false},
        {"vocabulary", access.vocabularyPage, access.vocabularyActivated, false},
        {"ocr", access.ocrPage, access.ocrActivated, false},
        {"prompts", access.promptsPage, access.promptsActivated, false},
        {"diagnostics", access.diagnosticsPage, access.diagnosticsActivated, false},
        {"logs", access.logsPage, access.logsActivated, false},
        {"settings", access.settingsPage, access.settingsActivated, false},
        {"faq", access.faqPage, access.faqActivated, false}
    };
    for (const PageSpec &page : pages) {
        registerPage(router, page);
    }
    return router;
}
