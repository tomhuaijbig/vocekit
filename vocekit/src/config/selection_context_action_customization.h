#ifndef VOCEKIT_SELECTION_CONTEXT_ACTION_CUSTOMIZATION_H
#define VOCEKIT_SELECTION_CONTEXT_ACTION_CUSTOMIZATION_H

#include <QMap>
#include <QString>
#include <QStringList>

struct AppSettingsData;
struct SelectionContextSettings;

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

struct SelectionContextActionNormalizationContext
{
    QStringList actionOrder;
    QStringList writableVocabularyScopeIds;
};

SelectionContextActionCustomizationMap
defaultSelectionContextActionCustomizations();
SelectionContextActionCustomizationMap
normalizeSelectionContextActionCustomizations(
    const SelectionContextActionCustomizationMap &values,
    const SelectionContextActionNormalizationContext &context
);
QString selectionContextActionDisplayName(
    const QString &actionId,
    const SelectionContextActionCustomizationMap &values
);
QStringList visibleSelectionContextActionOrder(
    const QStringList &order,
    const SelectionContextActionCustomizationMap &values
);
QStringList writableSelectionContextVocabularyScopeIds(
    const AppSettingsData &settings
);
SelectionContextSettings normalizeSelectionContextSettings(
    const SelectionContextSettings &source,
    const AppSettingsData &completeSettings
);

#endif // VOCEKIT_SELECTION_CONTEXT_ACTION_CUSTOMIZATION_H
