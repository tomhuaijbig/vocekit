#include "selection_context_actions.h"

namespace {

QString text8(const char *text)
{
    return QString::fromUtf8(text);
}

QString normalizedFunctionId(const QString &value)
{
    const QString id = value.trimmed();
    return id.isEmpty() || id.contains(QLatin1Char(':')) ? QString() : id;
}

} // namespace

QString selectionContextActionAiSearch()
{
    return QStringLiteral("ai-search");
}

QString selectionContextActionTranslate()
{
    return QStringLiteral("translate");
}

QString selectionContextActionExplain()
{
    return QStringLiteral("explain");
}

QString selectionContextActionSave()
{
    return QStringLiteral("save");
}

QString selectionContextActionCopy()
{
    return QStringLiteral("copy");
}

QString selectionContextActionForFunction(const QString &functionId)
{
    const QString id = normalizedFunctionId(functionId);
    return id.isEmpty() ? QString() : QStringLiteral("function:") + id;
}

QString selectionContextFunctionId(const QString &actionId)
{
    const QString prefix = QStringLiteral("function:");
    const QString value = actionId.trimmed();
    if (!value.startsWith(prefix)) {
        return QString();
    }
    return normalizedFunctionId(value.mid(prefix.size()));
}

bool isSelectionContextFunctionAction(const QString &actionId)
{
    return !selectionContextFunctionId(actionId).isEmpty();
}

QString selectionContextMenuBlockApplication()
{
    return QStringLiteral("block-application");
}

QString selectionContextMenuOpenSettings()
{
    return QStringLiteral("open-settings");
}

QStringList defaultSelectionContextActionOrder()
{
    return QStringList()
        << selectionContextActionAiSearch()
        << selectionContextActionTranslate()
        << selectionContextActionExplain()
        << selectionContextActionSave()
        << selectionContextActionCopy();
}

QStringList normalizeSelectionContextActionOrder(const QStringList &values)
{
    const QStringList defaults = defaultSelectionContextActionOrder();
    QStringList normalized;
    for (const QString &value : values) {
        const QString id = value.trimmed();
        if (defaults.contains(id) && !normalized.contains(id)) {
            normalized.append(id);
        }
    }
    for (const QString &id : defaults) {
        if (!normalized.contains(id)) {
            normalized.append(id);
        }
    }
    return normalized;
}

QString selectionContextActionTitle(const QString &id)
{
    if (id == selectionContextActionAiSearch()) {
        return text8("AI 搜索");
    }
    if (id == selectionContextActionTranslate()) {
        return text8("翻译");
    }
    if (id == selectionContextActionExplain()) {
        return text8("解释");
    }
    if (id == selectionContextActionSave()) {
        return text8("保存");
    }
    if (id == selectionContextActionCopy()) {
        return text8("复制");
    }
    if (id == selectionContextMenuBlockApplication()) {
        return text8("在此应用中禁用");
    }
    if (id == selectionContextMenuOpenSettings()) {
        return text8("打开设置");
    }
    return QString();
}
