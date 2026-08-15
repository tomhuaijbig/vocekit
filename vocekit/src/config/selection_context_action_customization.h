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

inline SelectionContextActionCustomizationMap
defaultSelectionContextActionCustomizations()
{
    SelectionContextActionCustomizationMap values;

    SelectionContextActionCustomization aiSearch;
    aiSearch.displayName = QString::fromUtf8("AI \xE6\x90\x9C\xE7\xB4\xA2");
    values.insert(QStringLiteral("ai-search"), aiSearch);

    SelectionContextActionCustomization translate;
    translate.displayName = QString::fromUtf8("\xE7\xBF\xBB\xE8\xAF\x91");
    values.insert(QStringLiteral("translate"), translate);

    SelectionContextActionCustomization explain;
    explain.displayName = QString::fromUtf8("\xE8\xA7\xA3\xE9\x87\x8A");
    values.insert(QStringLiteral("explain"), explain);

    SelectionContextActionCustomization save;
    save.displayName = QString::fromUtf8("\xE4\xBF\x9D\xE5\xAD\x98");
    values.insert(QStringLiteral("save"), save);

    SelectionContextActionCustomization copy;
    copy.displayName = QString::fromUtf8("\xE5\xA4\x8D\xE5\x88\xB6");
    values.insert(QStringLiteral("copy"), copy);

    return values;
}
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
