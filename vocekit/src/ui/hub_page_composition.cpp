#include "hub_page_composition.h"

namespace {

struct PageSpec
{
    const char *id;
    HubPageFactory factory;
    HubPageActivation activated;
};

void registerPage(HubPageRouter *router, const PageSpec &spec)
{
    if (!router || !spec.id || !spec.factory) {
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
        {"home", access.homePage, access.homeActivated},
        {"function", access.functionPage, access.functionActivated},
        {"history", access.historyPage, access.historyActivated},
        {"vocabulary", access.vocabularyPage, access.vocabularyActivated},
        {"ocr", access.ocrPage, access.ocrActivated},
        {"prompts", access.promptsPage, access.promptsActivated},
        {"diagnostics", access.diagnosticsPage, access.diagnosticsActivated},
        {"logs", access.logsPage, access.logsActivated},
        {"settings", access.settingsPage, access.settingsActivated},
        {"faq", access.faqPage, access.faqActivated}
    };
    for (const PageSpec &page : pages) {
        registerPage(router, page);
    }
    return router;
}
