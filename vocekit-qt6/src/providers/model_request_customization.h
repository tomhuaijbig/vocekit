#ifndef VOCEKIT_MODEL_REQUEST_CUSTOMIZATION_H
#define VOCEKIT_MODEL_REQUEST_CUSTOMIZATION_H

#include <QJsonObject>

// 高级设置只影响请求 Body。认证、超时和目标地址仍由 Provider 管理。
// 应用顺序：Provider 基础请求 -> 可视化参数 -> Raw JSON（最高优先）。
inline QJsonObject customizedModelRequestBody(
    QJsonObject body,
    const QJsonObject &advanced)
{
    if (!advanced.value(QStringLiteral("enabled")).toBool(false)) {
        return body;
    }

    const QJsonObject parameters =
        advanced.value(QStringLiteral("parameters")).toObject();
    for (auto it = parameters.constBegin(); it != parameters.constEnd(); ++it) {
        if (it.value().isNull()) {
            body.remove(it.key());
        } else {
            body.insert(it.key(), it.value());
        }
    }

    const QJsonObject raw = advanced.value(QStringLiteral("raw_json")).toObject();
    for (auto it = raw.constBegin(); it != raw.constEnd(); ++it) {
        if (it.value().isNull()) {
            body.remove(it.key());
        } else {
            body.insert(it.key(), it.value());
        }
    }
    return body;
}

inline QJsonObject advancedModelSettings(const QJsonObject &extra)
{
    return extra.value(QStringLiteral("vocekit_advanced")).toObject();
}

#endif // VOCEKIT_MODEL_REQUEST_CUSTOMIZATION_H
