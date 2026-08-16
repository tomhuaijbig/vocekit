#ifndef VOCEKIT_FUNCTION_SETTINGS_H
#define VOCEKIT_FUNCTION_SETTINGS_H

#include "../result_flow_config.h"
#include "function_flow_graph.h"
#include "model_sampling_settings.h"
#include "operation_error.h"

#include <QString>
#include <QStringList>

enum class FunctionExecutionMode
{
    Classic,
    Canvas
};

QString functionExecutionModeId(FunctionExecutionMode mode);
FunctionExecutionMode functionExecutionModeFromId(
    const QString &id,
    bool *known = nullptr
);

QString functionInputVoiceId();
QString functionInputSelectionId();
QString functionInputScreenshotId();
QStringList defaultFunctionInputOrder();
QStringList normalizeFunctionInputOrder(const QStringList &order);
QString functionOutputAiId();
QString functionOutputAutoWriteId();
QString functionOutputPopupId();
QString functionOutputScreenshotPanelId();
QStringList defaultFunctionOutputOrder();
QStringList normalizeFunctionOutputOrder(const QStringList &order);

// 一个功能可以组合选中文字、语音和截图三种输入。
struct FunctionInputSettings
{
    bool useSelection = false;
    bool useVoice = false;
    bool useScreenshot = false;
    QStringList order = QStringList()
        << QStringLiteral("voice")
        << QStringLiteral("selection")
        << QStringLiteral("screenshot");
    QString screenshotTriggerMode = QStringLiteral("separate");
    QString screenshotShortcut;
};

// 单个功能自己的录音方式、时间限制和提示设置。
struct FunctionRecordingSettings
{
    QString triggerMode = QStringLiteral("toggle");
    bool longRecordingEnabled = false;
    int segmentSeconds = 55;
    int maximumMinutes = 30;
    int countdownSeconds = 0;
    bool beepEnabled = false;
    QString beepPath;
};

// 单个功能自己的结果展示方式和结果窗口操作。
struct FunctionOutputSettings
{
    QString outputMode = QStringLiteral("resultPopup");
    QStringList order = QStringList()
        << QStringLiteral("ai")
        << QStringLiteral("autoWrite")
        << QStringLiteral("resultPopup")
        << QStringLiteral("screenshotPanel");
    QString resultTemplate = QStringLiteral("simple");
    QStringList resultActions = defaultResultActionIds();
    int floatingBarSeconds = 2;
    QString floatingBarStyleOverride = QStringLiteral("inherit");
    int resultPopupSeconds = 0;
};

// 听写、翻译、问答和自定义功能统一使用这一份配置。
struct FunctionSettings
{
    QString id;
    QString name;
    bool builtIn = false;
    QString shortcut;
    QString modelId;
    QString promptId;
    QString prompt;
    ModelSamplingSettings sampling;
    FunctionInputSettings input;
    FunctionRecordingSettings recording;
    FunctionOutputSettings output;
    FunctionNetworkPolicies network;
    FunctionExecutionMode executionMode = FunctionExecutionMode::Classic;
    FunctionFlowState flow;
};

FunctionSettings normalizeFunctionSettings(
    const FunctionSettings &settings
);

#endif // VOCEKIT_FUNCTION_SETTINGS_H
