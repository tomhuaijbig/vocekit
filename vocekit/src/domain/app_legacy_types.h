#ifndef VOCEKIT_APP_LEGACY_TYPES_H
#define VOCEKIT_APP_LEGACY_TYPES_H

#include "../result_flow_config.h"

#include <QString>
#include <QStringList>

#include <functional>

// 旧界面层仍在共用的数据类型。先集中到一个头文件，后续再逐步拆成更小的领域对象。
struct HotkeyDef
{
    QString id;
    QString title;
    QString defaultValue;
    QString hint;
};

struct CustomFunctionDef
{
    QString id;
    QString name;
    QString shortcut;
    QString model;
    QString outputMode;
    QStringList outputOrder = QStringList()
        << QStringLiteral("ai")
        << QStringLiteral("autoWrite")
        << QStringLiteral("resultPopup")
        << QStringLiteral("screenshotPanel");
    QString resultTemplate;
    bool useSelection = true;
    bool useVoice = true;
    bool useScreenshot = false;
    QStringList inputOrder = QStringList()
        << QStringLiteral("voice")
        << QStringLiteral("selection")
        << QStringLiteral("screenshot");
    QString screenshotTriggerMode = QStringLiteral("separate");
    QString screenshotShortcut;
    int floatingBarSeconds = 2;
    int resultPopupSeconds = 0;
    int countdownSeconds = 3;
    bool recordingBeepEnabled = true;
    QString recordingBeepPath;
    QString recordingTriggerMode = QStringLiteral("toggle");
    bool longRecordingEnabled = false;
    int segmentSeconds = 55;
    int maxRecordingMinutes = 30;
    QString prompt;
    QString promptId;
    QStringList resultActions = defaultResultActionIds();
    FunctionNetworkPolicies networkPolicies;
};

struct PromptLibraryItem
{
    QString id;
    QString name;
    QString scope;
    QString content;
};

struct VocabularyEntry
{
    QString id;
    QString source;
    QString target;
    QString aliases;
    QString scopeId;
    QString matchMode;
    QString note;
    bool enabled = true;
};

struct VocabularySuggestion
{
    VocabularyEntry entry;
    bool valid = false;
};

struct VocabularyCandidate
{
    VocabularyEntry entry;
    QString reason;
    int score = 0;
};

struct OcrAiTaskResult
{
    QString text;
    QString error;
};

using VocabularyAiCallback = std::function<VocabularySuggestion(
    const QString &,
    const QString &,
    QString *,
    const QString &,
    const QString &
)>;

struct ModelOption
{
    QString id;
    QString title;
    QString hint;
};

#endif // VOCEKIT_APP_LEGACY_TYPES_H
