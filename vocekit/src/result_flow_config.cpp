#include "result_flow_config.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSet>

QStringList defaultResultActionIds()
{
    return QStringList()
        << QStringLiteral("regenerate")
        << QStringLiteral("retryModel")
        << QStringLiteral("followUp")
        << QStringLiteral("expand")
        << QStringLiteral("vocabulary")
        << QStringLiteral("copy")
        << QStringLiteral("write")
        << QStringLiteral("replace");
}

QStringList normalizeResultActionIds(const QStringList &ids)
{
    if (ids.isEmpty()) {
        return defaultResultActionIds();
    }

    const QSet<QString> supported =
        QSet<QString>::fromList(defaultResultActionIds());
    QSet<QString> seen;
    QStringList normalized;
    for (const QString &id : ids) {
        const QString value = id.trimmed();
        if (!supported.contains(value) || seen.contains(value)) {
            continue;
        }
        normalized.append(value);
        seen.insert(value);
    }
    return normalized.isEmpty() ? defaultResultActionIds() : normalized;
}

QString normalizeNetworkPolicy(const QString &policy)
{
    const QString value = policy.trimmed();
    if (value == QStringLiteral("direct")
        || value == QStringLiteral("systemProxy")) {
        return value;
    }
    return QStringLiteral("inherit");
}

QString resolveNetworkPolicy(
    const QString &policy,
    bool globalUseSystemProxy)
{
    const QString normalized = normalizeNetworkPolicy(policy);
    if (normalized == QStringLiteral("inherit")) {
        return globalUseSystemProxy
            ? QStringLiteral("systemProxy")
            : QStringLiteral("direct");
    }
    return normalized;
}

QJsonObject functionNetworkPoliciesToJson(
    const FunctionNetworkPolicies &policies)
{
    QJsonObject object;
    object.insert(
        QStringLiteral("speech"),
        normalizeNetworkPolicy(policies.speech)
    );
    object.insert(
        QStringLiteral("ocr"),
        normalizeNetworkPolicy(policies.ocr)
    );
    object.insert(
        QStringLiteral("model"),
        normalizeNetworkPolicy(policies.model)
    );
    return object;
}

FunctionNetworkPolicies functionNetworkPoliciesFromJson(
    const QJsonObject &object)
{
    FunctionNetworkPolicies policies;
    policies.speech = normalizeNetworkPolicy(
        object.value(QStringLiteral("speech")).toString()
    );
    policies.ocr = normalizeNetworkPolicy(
        object.value(QStringLiteral("ocr")).toString()
    );
    policies.model = normalizeNetworkPolicy(
        object.value(QStringLiteral("model")).toString()
    );
    return policies;
}

bool shouldFallbackFromStreamFailure(
    const QString &error,
    int httpStatus,
    bool cancelled)
{
    if (cancelled || httpStatus == 400 || httpStatus == 401
        || httpStatus == 403 || httpStatus == 404
        || httpStatus == 429) {
        return false;
    }

    const QString value = error.trimmed().toLower();
    if (value.contains(QStringLiteral("cancel"))
        || value.contains(QStringLiteral("取消"))
        || value.contains(QStringLiteral("认证"))
        || value.contains(QStringLiteral("authentication"))
        || value.contains(QStringLiteral("unauthorized"))
        || value.contains(QStringLiteral("401"))
        || value.contains(QStringLiteral("forbidden"))
        || value.contains(QStringLiteral("403"))
        || value.contains(QStringLiteral("限流"))
        || value.contains(QStringLiteral("rate limit"))
        || value.contains(QStringLiteral("429"))
        || value.contains(QStringLiteral("模型不存在"))
        || value.contains(QStringLiteral("model not found"))
        || value.contains(QStringLiteral("404"))
        || value.contains(QStringLiteral("参数错误"))
        || value.contains(QStringLiteral("invalid request"))) {
        return false;
    }
    return !value.isEmpty();
}

QJsonObject resultRecoveryStateToJson(
    const ResultRecoveryState &state)
{
    QJsonObject object;
    object.insert(QStringLiteral("version"), 1);
    object.insert(QStringLiteral("modeId"), state.modeId);
    object.insert(QStringLiteral("functionTitle"), state.functionTitle);
    object.insert(QStringLiteral("selectedText"), state.selectedText);
    object.insert(QStringLiteral("voiceText"), state.voiceText);
    object.insert(QStringLiteral("textInput"), state.textInput);
    object.insert(QStringLiteral("ocrText"), state.ocrText);
    object.insert(QStringLiteral("generatedText"), state.generatedText);
    object.insert(QStringLiteral("editedText"), state.editedText);
    object.insert(QStringLiteral("model"), state.model);
    object.insert(QStringLiteral("promptId"), state.promptId);
    object.insert(QStringLiteral("stage"), state.stage);
    object.insert(
        QStringLiteral("networkPolicies"),
        functionNetworkPoliciesToJson(state.networkPolicies)
    );
    object.insert(
        QStringLiteral("createdAt"),
        state.createdAt.toString(Qt::ISODateWithMs)
    );
    object.insert(
        QStringLiteral("updatedAt"),
        state.updatedAt.toString(Qt::ISODateWithMs)
    );
    return object;
}

ResultRecoveryState resultRecoveryStateFromJson(
    const QJsonObject &object)
{
    ResultRecoveryState state;
    state.modeId = object.value(QStringLiteral("modeId")).toString().trimmed();
    state.functionTitle =
        object.value(QStringLiteral("functionTitle")).toString();
    state.selectedText =
        object.value(QStringLiteral("selectedText")).toString();
    state.voiceText = object.value(QStringLiteral("voiceText")).toString();
    state.textInput = object.value(QStringLiteral("textInput")).toString();
    state.ocrText = object.value(QStringLiteral("ocrText")).toString();
    state.generatedText =
        object.value(QStringLiteral("generatedText")).toString();
    state.editedText =
        object.value(QStringLiteral("editedText")).toString();
    state.model = object.value(QStringLiteral("model")).toString();
    state.promptId = object.value(QStringLiteral("promptId")).toString();
    state.stage = object.value(QStringLiteral("stage")).toString();
    state.networkPolicies = functionNetworkPoliciesFromJson(
        object.value(QStringLiteral("networkPolicies")).toObject()
    );
    state.createdAt = QDateTime::fromString(
        object.value(QStringLiteral("createdAt")).toString(),
        Qt::ISODateWithMs
    );
    state.updatedAt = QDateTime::fromString(
        object.value(QStringLiteral("updatedAt")).toString(),
        Qt::ISODateWithMs
    );
    state.valid = !state.modeId.isEmpty()
        && !state.stage.trimmed().isEmpty();
    return state;
}

bool saveResultRecoveryState(
    const QString &path,
    const ResultRecoveryState &state,
    QString *error)
{
    const QFileInfo info(path);
    if (!QDir().mkpath(info.absolutePath())) {
        if (error) {
            *error = QStringLiteral("无法创建恢复文件目录。");
        }
        return false;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    const QByteArray data = QJsonDocument(
        resultRecoveryStateToJson(state)
    ).toJson(QJsonDocument::Indented);
    if (file.write(data) != data.size() || !file.commit()) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    return true;
}

bool loadResultRecoveryState(
    const QString &path,
    ResultRecoveryState *state,
    QString *error)
{
    if (!state) {
        if (error) {
            *error = QStringLiteral("恢复状态输出参数为空。");
        }
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError
        || !document.isObject()) {
        if (error) {
            *error = parseError.errorString();
        }
        return false;
    }
    *state = resultRecoveryStateFromJson(document.object());
    if (!state->valid) {
        if (error) {
            *error = QStringLiteral("恢复文件缺少必要字段。");
        }
        return false;
    }
    return true;
}

bool clearResultRecoveryState(
    const QString &path,
    QString *error)
{
    if (!QFileInfo::exists(path)) {
        return true;
    }
    if (QFile::remove(path)) {
        return true;
    }
    if (error) {
        *error = QStringLiteral("无法删除恢复文件。");
    }
    return false;
}
