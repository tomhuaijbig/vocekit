#ifndef VOCEKIT_HUB_PAGE_HOST_CONTROLLER_H
#define VOCEKIT_HUB_PAGE_HOST_CONTROLLER_H

#include "hub_page_composition_access_factory.h"

#include <QScopedPointer>

class HubPageRouter;
class QWidget;

using HubPageHostControllerAccess =
    HubPageCompositionAccessFactoryDependencies;

// Owns the command-center page router and performs page composition once.
class HubPageHostController
{
public:
    explicit HubPageHostController(
        const HubPageHostControllerAccess &access,
        QWidget *parent = nullptr
    );

    HubPageRouter *router() const;

private:
    HubPageHostController(const HubPageHostController &) = delete;
    HubPageHostController &operator=(
        const HubPageHostController &
    ) = delete;

    QScopedPointer<HubPageRouter> m_router;
};

#endif // VOCEKIT_HUB_PAGE_HOST_CONTROLLER_H
