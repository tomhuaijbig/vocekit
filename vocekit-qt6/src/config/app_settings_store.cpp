#include "app_settings_store.h"

#include "app_settings_json.h"
#include "app_paths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>
#include <QSet>

namespace {

QString defaultSettingsPath()
{
    return appConfigFilePath(QStringLiteral("settings.json"));
}

void clearError(OperationError *error)
{
    if (error) {
        *error = OperationError();
    }
}

void setError(
    OperationError *error,
    const QString &code,
    const QString &message,
    const QString &detail = QString())
{
    if (!error) {
        return;
    }
    error->code = code;
    error->message = message;
    error->detail = detail;
}

} // namespace

AppSettingsStore::AppSettingsStore(const QString &path)
    : m_path(path.trimmed().isEmpty() ? defaultSettingsPath() : path)
{
}

bool AppSettingsStore::load(OperationError *error)
{
    clearError(error);
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(
            error,
            QStringLiteral("settings.open_failed"),
            QStringLiteral("无法打开设置文件。"),
            file.errorString()
        );
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(
        file.readAll(),
        &parseError
    );
    if (parseError.error != QJsonParseError::NoError
        || !document.isObject()) {
        setError(
            error,
            QStringLiteral("settings.invalid_json"),
            QStringLiteral("设置文件格式不正确。"),
            parseError.errorString()
        );
        return false;
    }

    QStringList warnings;
    m_data = appSettingsDataFromJson(document.object(), &warnings);
    if (!warnings.isEmpty() && error) {
        error->detail = warnings.join(QStringLiteral("\n"));
    }
    return true;
}

bool AppSettingsStore::loadOrCreateDefaults(OperationError *error)
{
    if (load(error)) {
        return true;
    }
    m_data = appSettingsDataFromJson(QJsonObject());
    return save(error);
}

bool AppSettingsStore::save(OperationError *error) const
{
    clearError(error);
    const QFileInfo info(m_path);
    if (!QDir().mkpath(info.absolutePath())) {
        setError(
            error,
            QStringLiteral("settings.directory_failed"),
            QStringLiteral("无法创建设置目录。"),
            info.absolutePath()
        );
        return false;
    }

    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly)) {
        setError(
            error,
            QStringLiteral("settings.write_failed"),
            QStringLiteral("无法写入设置文件。"),
            file.errorString()
        );
        return false;
    }

    const QByteArray bytes = QJsonDocument(
        appSettingsDataToJson(m_data)
    ).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size()) {
        setError(
            error,
            QStringLiteral("settings.write_failed"),
            QStringLiteral("设置文件写入不完整。"),
            file.errorString()
        );
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        setError(
            error,
            QStringLiteral("settings.commit_failed"),
            QStringLiteral("无法保存设置文件。"),
            file.errorString()
        );
        return false;
    }
    return true;
}

const AppSettingsData &AppSettingsStore::snapshot() const
{
    return m_data;
}

FunctionSettings AppSettingsStore::function(const QString &id) const
{
    return m_data.function(id);
}

bool AppSettingsStore::updateFunction(
    const FunctionSettings &updatedFunction)
{
    const FunctionSettings normalized =
        normalizeFunctionSettings(updatedFunction);
    if (normalized.id.isEmpty() || normalized.name.isEmpty()) {
        return false;
    }

    const int index = m_data.functionIndex(normalized.id);
    if (index >= 0) {
        if (m_data.functions.at(index).builtIn != normalized.builtIn) {
            return false;
        }
        m_data.functions[index] = normalized;
        return true;
    }
    if (normalized.builtIn) {
        return false;
    }

    m_data.functions.append(normalized);
    if (!m_data.functionOrder.contains(normalized.id)) {
        m_data.functionOrder.append(normalized.id);
    }
    return true;
}

void AppSettingsStore::updateGlobal(const AppSettingsData &data)
{
    const QVector<FunctionSettings> functions = m_data.functions;
    m_data = data;
    m_data.functions = functions;
    for (const FunctionSettings &item : functions) {
        if (!m_data.functionOrder.contains(item.id)) {
            m_data.functionOrder.append(item.id);
        }
    }
}

void AppSettingsStore::replaceSnapshot(const AppSettingsData &data)
{
    m_data = data;
}

bool AppSettingsStore::replaceAndSave(
    const AppSettingsData &data,
    OperationError *error)
{
    const AppSettingsData previous = m_data;
    m_data = data;
    if (save(error)) {
        return true;
    }
    m_data = previous;
    return false;
}

bool AppSettingsStore::replaceNonFlowSettingsAndSave(
    const AppSettingsData &editedSettings,
    OperationError *error)
{
    clearError(error);
    QSet<QString> currentIds;
    QSet<QString> editedIds;
    for (const FunctionSettings &function : m_data.functions) {
        currentIds.insert(function.id);
    }
    for (const FunctionSettings &function : editedSettings.functions) {
        editedIds.insert(function.id);
    }
    if (currentIds != editedIds
        || currentIds.size() != m_data.functions.size()
        || editedIds.size() != editedSettings.functions.size()) {
        setError(
            error,
            QStringLiteral("settings_function_set_stale"),
            QStringLiteral("功能集合已变化，请重新加载设置。")
        );
        return false;
    }

    // editedSettings owns non-flow fields; the current store owns builtIn,
    // executionMode, and flow.
    AppSettingsData merged = editedSettings;
    merged.retainedRootValues = m_data.retainedRootValues;
    merged.retainedOrphanFunctionFlows =
        m_data.retainedOrphanFunctionFlows;
    for (FunctionSettings &editedFunction : merged.functions) {
        const int currentIndex =
            m_data.functionIndex(editedFunction.id);
        if (currentIndex < 0) {
            setError(
                error,
                QStringLiteral("settings_function_set_stale"),
                QStringLiteral("功能集合已变化，请重新加载设置。")
            );
            return false;
        }
        const FunctionSettings &current =
            m_data.functions.at(currentIndex);
        editedFunction.builtIn = current.builtIn;
        editedFunction.executionMode = current.executionMode;
        editedFunction.flow = current.flow;
        editedFunction = normalizeFunctionSettings(editedFunction);
    }
    return replaceAndSave(merged, error);
}

QString AppSettingsStore::path() const
{
    return m_path;
}
