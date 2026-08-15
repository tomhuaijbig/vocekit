#ifndef VOCEKIT_MODEL_CATALOG_H
#define VOCEKIT_MODEL_CATALOG_H

#include "../config/secret_config.h"
#include "../domain/app_legacy_types.h"

#include <QString>
#include <QVector>

// Central catalog for built-in and custom model options.
QVector<ModelOption> modelOptions();
QVector<ModelOption> modelOptionsForSecrets(const SecretConfig &secrets);
QString modelTitle(const QString &id);
QString modelDisplayText(const QString &id);
QString normalizeModelId(const QString &value, const QString &fallback = QString());
QString normalizeExplicitModelId(const QString &value);

#endif // VOCEKIT_MODEL_CATALOG_H
