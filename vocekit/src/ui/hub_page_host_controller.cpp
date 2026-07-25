#include "hub_page_host_controller.h"

#include "hub_page_composition.h"
#include "hub_page_router.h"

HubPageHostController::HubPageHostController(
    const HubPageHostControllerAccess &access,
    QWidget *parent
)
    : m_router(
          HubPageComposition::create(
              createHubPageCompositionAccess(access),
              parent
          )
      )
{
}

HubPageRouter *HubPageHostController::router() const
{
    return m_router.data();
}
