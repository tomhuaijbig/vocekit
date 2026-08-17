#include "function_flow_runtime_log.h"

#include "../domain/function_flow_errors.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>

namespace {

bool isSuspiciousMetadata(const QString &value)
{
    const QString lower = value.toLower();
    return lower.contains(QStringLiteral("data:image"))
        || lower.startsWith(QStringLiteral("sk-"))
        || lower.contains(QStringLiteral("api_key"))
        || lower.contains(QStringLiteral("apikey"))
        || lower.contains(QStringLiteral("authorization"))
        || lower.contains(QStringLiteral("bearer "));
}

QString safeIdentifier(const QString &value, int maxLength = 128)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty() || trimmed.size() > maxLength
        || isSuspiciousMetadata(trimmed)) {
        return QString();
    }

    for (const QChar character : trimmed) {
        const ushort code = character.unicode();
        const bool allowed =
            (code >= 'a' && code <= 'z')
            || (code >= 'A' && code <= 'Z')
            || (code >= '0' && code <= '9')
            || character == QChar('_')
            || character == QChar('-')
            || character == QChar('.')
            || character == QChar(':')
            || character == QChar('/')
            || character == QChar('@')
            || character == QChar('+');
        if (!allowed) {
            return QString();
        }
    }
    return trimmed;
}

void insertSafeIdentifier(
    QJsonObject *object,
    const QString &key,
    const QString &value,
    int maxLength = 128
)
{
    if (!object) {
        return;
    }
    const QString safe = safeIdentifier(value, maxLength);
    if (!safe.isEmpty()) {
        object->insert(key, safe);
    }
}

} // namespace

QJsonObject functionFlowRuntimeLogMetadata(
    const FunctionFlowRuntimeLogEntry &entry
)
{
    QJsonObject object;
    insertSafeIdentifier(
        &object,
        QStringLiteral("functionId"),
        entry.functionId
    );
    if (entry.publishedRevision > 0) {
        object.insert(
            QStringLiteral("publishedRevision"),
            entry.publishedRevision
        );
    }
    if (QRegularExpression(QStringLiteral("^[0-9a-f]{64}$"))
            .match(entry.publishedHash)
            .hasMatch()) {
        object.insert(
            QStringLiteral("publishedHash"),
            entry.publishedHash
        );
    }
    insertSafeIdentifier(
        &object,
        QStringLiteral("runId"),
        entry.runId.value
    );
    insertSafeIdentifier(
        &object,
        QStringLiteral("trigger"),
        entry.trigger
    );
    insertSafeIdentifier(
        &object,
        QStringLiteral("nodeId"),
        entry.nodeId
    );
    insertSafeIdentifier(
        &object,
        QStringLiteral("nodeType"),
        entry.nodeType
    );
    insertSafeIdentifier(
        &object,
        QStringLiteral("status"),
        entry.status
    );
    if (entry.elapsedMs >= 0) {
        object.insert(
            QStringLiteral("elapsedMs"),
            static_cast<double>(entry.elapsedMs)
        );
    }
    if (isFunctionFlowStableErrorCode(entry.error.code)) {
        object.insert(
            QStringLiteral("errorCode"),
            entry.error.code.trimmed()
        );
    }
    insertSafeIdentifier(
        &object,
        QStringLiteral("modelId"),
        entry.modelId
    );
    insertSafeIdentifier(
        &object,
        QStringLiteral("promptVersion"),
        entry.promptVersion
    );
    if (entry.httpStatus >= 100 && entry.httpStatus <= 599) {
        object.insert(QStringLiteral("httpStatus"), entry.httpStatus);
    }
    return object;
}

QByteArray functionFlowRuntimeLogLine(
    const FunctionFlowRuntimeLogEntry &entry
)
{
    return QJsonDocument(functionFlowRuntimeLogMetadata(entry))
        .toJson(QJsonDocument::Compact);
}

bool appendFunctionFlowRuntimeLog(
    const QString &filePath,
    const FunctionFlowRuntimeLogEntry &entry
)
{
    const QString trimmedPath = filePath.trimmed();
    if (trimmedPath.isEmpty()) {
        return false;
    }

    const QFileInfo info(trimmedPath);
    if (!info.dir().exists()
        && !QDir().mkpath(info.dir().absolutePath())) {
        return false;
    }

    QFile file(info.absoluteFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        return false;
    }
    const QByteArray line =
        functionFlowRuntimeLogLine(entry) + QByteArray(1, '\n');
    return file.write(line) == line.size() && file.flush();
}
