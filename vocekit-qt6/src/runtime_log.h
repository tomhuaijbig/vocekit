#ifndef RUNTIME_LOG_H
#define RUNTIME_LOG_H

#include <QString>

QString runtimeLogDirectory();
void setRuntimeLogSessionId(const QString &sessionId);
QString runtimeLogSessionId();
void logRuntimeEvent(const QString &category, const QString &action, const QString &detail = QString(), qint64 elapsedMs = -1);

#endif // RUNTIME_LOG_H
