#ifndef VOCEKIT_FUNCTION_MODE_GRID_ACCESS_FACTORY_H
#define VOCEKIT_FUNCTION_MODE_GRID_ACCESS_FACTORY_H

#include "function_mode_grid.h"

class HubSettingsState;

// Builds the typed settings bridge used by the home-page function grid.
FunctionModeGridAccess createFunctionModeGridAccess(
    HubSettingsState *settings
);

#endif // VOCEKIT_FUNCTION_MODE_GRID_ACCESS_FACTORY_H
