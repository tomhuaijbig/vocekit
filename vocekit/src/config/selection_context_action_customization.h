#ifndef VOCEKIT_SELECTION_CONTEXT_ACTION_CUSTOMIZATION_H
#define VOCEKIT_SELECTION_CONTEXT_ACTION_CUSTOMIZATION_H

#include <QMap>
#include <QString>
#include <QStringList>

struct SelectionContextActionCustomization
{
    QString displayName;
    bool visible = true;
    QString modelId;
    QString promptOverride;
    QString targetLanguage;
    QString vocabularyScopeId = QStringLiteral("__global");
    QString copyMode = QStringLiteral("original");
};

typedef QMap<QString, SelectionContextActionCustomization>
    SelectionContextActionCustomizationMap;

SelectionContextActionCustomizationMap
defaultSelectionContextActionCustomizations();
SelectionContextActionCustomizationMap
normalizeSelectionContextActionCustomizations(
    const SelectionContextActionCustomizationMap &values,
    const QStringList &writableVocabularyScopeIds
);
QString selectionContextActionDisplayName(
    const QString &actionId,
    const SelectionContextActionCustomizationMap &values
);
QStringList visibleSelectionContextActionOrder(
    const QStringList &order,
    const SelectionContextActionCustomizationMap &values
);

#endif // VOCEKIT_SELECTION_CONTEXT_ACTION_CUSTOMIZATION_H
