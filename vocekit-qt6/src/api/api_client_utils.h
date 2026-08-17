#ifndef VOCEKIT_API_CLIENT_UTILS_H
#define VOCEKIT_API_CLIENT_UTILS_H

#include <QByteArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QUrl>
#include <QVector>

// 接口 Provider 共用的签名、地址和响应读取工具集中在这里。
QString compactLogText(QString text, int maxLength = 700);
QString networkLogTarget(const QUrl &url);
QUrl urlWithDefaultHttps(QString text);
QUrl openAiCompatibleChatUrl(QString text);
QUrl anthropicMessagesUrl(QString text);
QString jsonPathStringValue(const QJsonValue &value, const QStringList &path);
QString firstJsonStringValue(
    const QJsonObject &root,
    const QVector<QStringList> &paths
);
QByteArray hmacSha256(const QByteArray &key, const QByteArray &message);

#endif // VOCEKIT_API_CLIENT_UTILS_H
