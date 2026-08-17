#include "prompt_library_store.h"

#include "../config/app_paths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>

namespace {

QString defaultPromptLibraryPath()
{
    return QDir(appBasePath()).filePath(QStringLiteral("config/prompts.json"));
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

PromptLibraryStore::PromptLibraryStore(const QString &path)
    : m_path(path.trimmed().isEmpty() ? defaultPromptLibraryPath() : path)
{
}

bool PromptLibraryStore::load(OperationError *error)
{
    clearError(error);
    m_items.clear();
    QFile file(m_path);
    if (!file.exists()) {
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        setError(
            error,
            QStringLiteral("prompt_library.open_failed"),
            QStringLiteral("无法打开提示词库。"),
            file.errorString()
        );
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(
            error,
            QStringLiteral("prompt_library.invalid_json"),
            QStringLiteral("提示词库格式不正确。"),
            parseError.errorString()
        );
        return false;
    }

    const QJsonArray values = document.object()
        .value(QStringLiteral("customPrompts"))
        .toArray();
    for (const QJsonValue &value : values) {
        const QJsonObject object = value.toObject();
        PromptLibraryItem item;
        item.id = object.value(QStringLiteral("id")).toString().trimmed();
        item.name = object.value(QStringLiteral("name")).toString().trimmed();
        item.scope = object.value(QStringLiteral("scope")).toString().trimmed();
        item.content = object.value(QStringLiteral("content")).toString();
        if (item.id.isEmpty() || item.name.isEmpty()) {
            continue;
        }
        if (item.scope.isEmpty()) {
            item.scope = QString::fromUtf8("通用");
        }
        m_items.append(item);
    }
    return true;
}

bool PromptLibraryStore::save(
    const QVector<PromptLibraryItem> &items,
    OperationError *error)
{
    clearError(error);
    const QFileInfo info(m_path);
    if (!QDir().mkpath(info.absolutePath())) {
        setError(
            error,
            QStringLiteral("prompt_library.directory_failed"),
            QStringLiteral("无法创建提示词库目录。"),
            info.absolutePath()
        );
        return false;
    }

    QJsonArray values;
    for (const PromptLibraryItem &item : items) {
        QJsonObject object;
        object.insert(QStringLiteral("id"), item.id);
        object.insert(QStringLiteral("name"), item.name);
        object.insert(
            QStringLiteral("scope"),
            item.scope.trimmed().isEmpty() ? QString::fromUtf8("通用") : item.scope.trimmed()
        );
        object.insert(QStringLiteral("content"), item.content);
        values.append(object);
    }
    QJsonObject root;
    root.insert(QStringLiteral("customPrompts"), values);
    const QByteArray bytes = QJsonDocument(root).toJson(QJsonDocument::Indented);

    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        setError(
            error,
            QStringLiteral("prompt_library.write_failed"),
            QStringLiteral("无法保存提示词库。"),
            file.errorString()
        );
        file.cancelWriting();
        return false;
    }
    m_items = items;
    return true;
}

const QVector<PromptLibraryItem> &PromptLibraryStore::items() const
{
    return m_items;
}

QString PromptLibraryStore::path() const
{
    return m_path;
}
