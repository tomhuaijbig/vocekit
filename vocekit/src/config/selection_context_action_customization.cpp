#include "selection_context_action_customization.h"

#include "../domain/selection_context_actions.h"

namespace {

QString normalizedDisplayName(const QString &actionId, const QString &value)
{
    const QString trimmed = value.trimmed();
    return trimmed.isEmpty()
        ? selectionContextActionTitle(actionId)
        : trimmed.left(24);
}

QString normalizedVocabularyScope(
    const QString &value,
    const QStringList &writableVocabularyScopeIds
)
{
    const QString scopeId = value.trimmed();
    if (scopeId.isEmpty() || scopeId == QStringLiteral("__all")) {
        return QStringLiteral("__global");
    }
    if (writableVocabularyScopeIds.isEmpty()) {
        return scopeId;
    }
    return writableVocabularyScopeIds.contains(scopeId)
        ? scopeId
        : QStringLiteral("__global");
}

} // namespace

SelectionContextActionCustomizationMap
defaultSelectionContextActionCustomizations()
{
    SelectionContextActionCustomizationMap values;
    for (const QString &id : defaultSelectionContextActionOrder()) {
        SelectionContextActionCustomization item;
        item.displayName = selectionContextActionTitle(id);
        values.insert(id, item);
    }
    return values;
}

SelectionContextActionCustomizationMap
normalizeSelectionContextActionCustomizations(
    const SelectionContextActionCustomizationMap &values,
    const SelectionContextActionNormalizationContext &context
)
{
    SelectionContextActionCustomizationMap normalized;
    const QStringList actionIds = defaultSelectionContextActionOrder();
    const QStringList normalizedOrder =
        normalizeSelectionContextActionOrder(context.actionOrder);
    for (const QString &id : actionIds) {
        SelectionContextActionCustomization item = values.contains(id)
            ? values.value(id)
            : SelectionContextActionCustomization();
        item.displayName = normalizedDisplayName(id, item.displayName);
        item.modelId = item.modelId.trimmed();
        item.promptOverride = item.promptOverride.trimmed().left(8000);
        item.targetLanguage = item.targetLanguage.trimmed().left(64);
        item.vocabularyScopeId = normalizedVocabularyScope(
            item.vocabularyScopeId,
            context.writableVocabularyScopeIds
        );
        item.copyMode = item.copyMode.trimmed();
        if (item.copyMode != QStringLiteral("original")
            && item.copyMode != QStringLiteral("trim")) {
            item.copyMode = QStringLiteral("original");
        }
        normalized.insert(id, item);
    }

    bool hasVisibleAction = false;
    for (const QString &id : actionIds) {
        if (normalized.value(id).visible) {
            hasVisibleAction = true;
            break;
        }
    }
    if (!hasVisibleAction && !normalizedOrder.isEmpty()) {
        SelectionContextActionCustomization first =
            normalized.value(normalizedOrder.first());
        first.visible = true;
        normalized.insert(normalizedOrder.first(), first);
    }
    return normalized;
}

QString selectionContextActionDisplayName(
    const QString &actionId,
    const SelectionContextActionCustomizationMap &values
)
{
    if (!defaultSelectionContextActionOrder().contains(actionId)) {
        return QString();
    }
    return normalizedDisplayName(
        actionId,
        values.value(actionId).displayName
    );
}

QStringList visibleSelectionContextActionOrder(
    const QStringList &order,
    const SelectionContextActionCustomizationMap &values
)
{
    const QStringList normalizedOrder =
        normalizeSelectionContextActionOrder(order);
    SelectionContextActionNormalizationContext context;
    context.actionOrder = normalizedOrder;
    const SelectionContextActionCustomizationMap normalizedValues =
        normalizeSelectionContextActionCustomizations(values, context);
    QStringList visibleOrder;
    for (const QString &id : normalizedOrder) {
        if (normalizedValues.value(id).visible) {
            visibleOrder.append(id);
        }
    }
    return visibleOrder;
}
