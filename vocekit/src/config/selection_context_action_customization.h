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
    QString usageHint;
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
    aiSearch.usageHint = QString::fromUtf8(
        "\xE7\xBB\x93\xE5\x90\x88\xE9\x80\x89\xE4\xB8\xAD\xE6\x96\x87\xE5\xAD\x97\xE6\x90\x9C\xE7\xB4\xA2\xE5\xB9\xB6\xE6\x95\xB4\xE7\x90\x86\xE7\x9B\xB8\xE5\x85\xB3\xE4\xBF\xA1\xE6\x81\xAF");
    values.insert(QStringLiteral("ai-search"), aiSearch);

    SelectionContextActionCustomization translate;
    translate.displayName = QString::fromUtf8("\xE7\xBF\xBB\xE8\xAF\x91");
    translate.usageHint = QString::fromUtf8(
        "\xE5\xB0\x86\xE9\x80\x89\xE4\xB8\xAD\xE6\x96\x87\xE5\xAD\x97\xE7\xBF\xBB\xE8\xAF\x91\xE4\xB8\xBA\xE8\xAE\xBE\xE7\xBD\xAE\xE7\x9A\x84\xE7\x9B\xAE\xE6\xA0\x87\xE8\xAF\xAD\xE8\xA8\x80");
    values.insert(QStringLiteral("translate"), translate);

    SelectionContextActionCustomization explain;
    explain.displayName = QString::fromUtf8("\xE8\xA7\xA3\xE9\x87\x8A");
    explain.usageHint = QString::fromUtf8(
        "\xE4\xBD\xBF\xE7\x94\xA8 AI \xE8\xA7\xA3\xE9\x87\x8A\xE9\x80\x89\xE4\xB8\xAD\xE6\x96\x87\xE5\xAD\x97\xE7\x9A\x84\xE5\x90\xAB\xE4\xB9\x89");
    values.insert(QStringLiteral("explain"), explain);

    SelectionContextActionCustomization save;
    save.displayName = QString::fromUtf8("\xE4\xBF\x9D\xE5\xAD\x98");
    save.usageHint = QString::fromUtf8(
        "\xE6\x8A\x8A\xE9\x80\x89\xE4\xB8\xAD\xE6\x96\x87\xE5\xAD\x97\xE4\xBF\x9D\xE5\xAD\x98\xE5\x88\xB0\xE8\xAE\xBE\xE7\xBD\xAE\xE7\x9A\x84\xE8\xAF\x8D\xE5\xBA\x93");
    values.insert(QStringLiteral("save"), save);

    SelectionContextActionCustomization copy;
    copy.displayName = QString::fromUtf8("\xE5\xA4\x8D\xE5\x88\xB6");
    copy.usageHint = QString::fromUtf8(
        "\xE5\xA4\x8D\xE5\x88\xB6\xE9\x80\x89\xE4\xB8\xAD\xE6\x96\x87\xE5\xAD\x97\xEF\xBC\x8C\xE4\xB8\x8D\xE8\xB0\x83\xE7\x94\xA8\xE6\xA8\xA1\xE5\x9E\x8B");
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
QString selectionContextActionUsageHint(
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
