#ifndef VOCEKIT_MODEL_REQUEST_LOG_H
#define VOCEKIT_MODEL_REQUEST_LOG_H

#include "../providers/provider_types.h"

#include <QJsonObject>
#include <QVector>

QJsonObject redactedModelRequestJson(const QJsonObject &object);
QString redactedModelLogText(const QString &text);
bool appendModelRequestLog(const ModelResult &result);
QVector<QJsonObject> recentModelRequestLogs(int maximumCount = 100);
QString modelRequestLogPath();

#endif // VOCEKIT_MODEL_REQUEST_LOG_H
