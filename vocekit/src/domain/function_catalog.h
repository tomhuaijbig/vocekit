#ifndef VOCEKIT_FUNCTION_CATALOG_H
#define VOCEKIT_FUNCTION_CATALOG_H

#include "../config/app_settings_data.h"

#include <QString>

FunctionSettings functionSettingsById(
    const AppSettingsData &settings,
    const QString &id,
    bool *found = nullptr
);

QString functionDisplayTitle(
    const AppSettingsData &settings,
    const QString &id,
    const QString &fallback = QString()
);

#endif // VOCEKIT_FUNCTION_CATALOG_H
