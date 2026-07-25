#ifndef VOCEKIT_APP_SETTINGS_JSON_H
#define VOCEKIT_APP_SETTINGS_JSON_H

#include "app_settings_data.h"

#include <QJsonObject>
#include <QStringList>

AppSettingsData appSettingsDataFromJson(
    const QJsonObject &root,
    QStringList *warnings = nullptr
);

QJsonObject appSettingsDataToJson(const AppSettingsData &data);

#endif // VOCEKIT_APP_SETTINGS_JSON_H
