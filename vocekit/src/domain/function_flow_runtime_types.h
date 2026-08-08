#ifndef VOCEKIT_FUNCTION_FLOW_RUNTIME_TYPES_H
#define VOCEKIT_FUNCTION_FLOW_RUNTIME_TYPES_H

#include "execution_types.h"
#include "function_flow_graph.h"
#include "function_flow_ports.h"
#include "operation_error.h"
#include "../ocr/ocr_types.h"
#include "../recording/segmented_recording.h"
#include "../tasks/cancellation_token.h"

#include <QImage>
#include <QList>
#include <QMap>
#include <QRect>
#include <QSharedPointer>
#include <QStringList>
#include <QVector>

#include <functional>

enum class FunctionFlowTrigger
{
    MainHotkey,
    ScreenshotHotkey,
    ScreenshotLauncher
};

QString functionFlowTriggerId(FunctionFlowTrigger trigger);

using FunctionFlowTargetWindowHandle = void *;

struct FunctionFlowTriggerPlan
{
    FunctionFlowTrigger trigger = FunctionFlowTrigger::MainHotkey;
    bool available = false;
    QStringList activeSourceNodeIds;
    QStringList acquisitionNodeIds;
    bool usesHoldToTalk = false;
};

struct FunctionFlowTriggerRequest
{
    QString functionId;
    FunctionFlowTrigger trigger = FunctionFlowTrigger::MainHotkey;
    FunctionFlowTargetWindowHandle targetWindow = nullptr;
    bool classicWorkflowBusy = false;
};

struct FunctionFlowVoicePayload
{
    QString sourceAudioPath;
    QVector<RecordingSegment> segments;
    qint64 speechElapsedMs = -1;
    QString recordingTriggerMode;
    bool longRecording = false;
};

struct FunctionFlowScreenshotPayload
{
    QImage image;
    QVector<OcrTextBlock> blocks;
    QString recognizedText;
    OcrEngine engine = OcrEngine::Automatic;
    qint64 elapsedMs = -1;
    bool usedFallback = false;
    QRect rect;
};

struct FunctionFlowValue
{
    QString text;
    QString sourceNodeId;
    QString role;
    int sequence = 0;
    QSharedPointer<const FunctionFlowVoicePayload> voice;
    QSharedPointer<const FunctionFlowScreenshotPayload> screenshot;
};

struct FunctionFlowResolvedNodeSettings
{
    QString modelId;
    QString systemPrompt;
    QString promptVersion;
    QString speechProviderId;
    QString ocrEngineId;
    QString effectiveNetworkPolicy;
    bool strongSelectionEnabled = false;
};

struct FunctionFlowResolvedDependencies
{
    QMap<QString, FunctionFlowResolvedNodeSettings> byNodeId;
    QString functionTitle;
    QString recordDirectory;
    int inheritedResultPopupOpacity = 100;
    int inheritedScreenshotPanelOpacity = 92;
    bool hasResultPopupGeometry = false;
    QRect resultPopupGeometry;
    bool hasScreenshotPanelGeometry = false;
    QRect screenshotPanelGeometry;
    QMap<QString, FunctionFlowNodeConfig> nodeConfigs;
};

struct FunctionFlowRunContext
{
    ExecutionId runId;
    QString functionId;
    int publishedRevision = 0;
    QString publishedHash;
    FunctionFlowTrigger trigger = FunctionFlowTrigger::MainHotkey;
    FunctionFlowTargetWindowHandle targetWindow = nullptr;
    CancellationToken cancellation;
    QSharedPointer<const FunctionFlowResolvedDependencies> dependencies;
};

enum class FunctionFlowNodeState
{
    Pending,
    Ready,
    Running,
    Cancelling,
    Succeeded,
    Skipped,
    Failed,
    Blocked,
    Cancelled
};

enum class FunctionFlowStartOutcome
{
    Started,
    CancelledExisting,
    NotAvailable,
    Busy,
    TargetUnavailable,
    ConfigurationError
};

struct FunctionFlowNodeResult
{
    FunctionFlowNodeState state = FunctionFlowNodeState::Failed;
    QList<FunctionFlowValue> values;
    QList<FunctionFlowValue> historyObservations;
    OperationError error;
};

struct FunctionFlowResultActionRequest
{
    FunctionFlowValue output;
    QString canonicalInput;
    bool collectedSelection = false;
};

using FunctionFlowNodeCompletion =
    std::function<void(const FunctionFlowNodeResult &)>;

struct FunctionFlowNodeTrace
{
    QString nodeId;
    QString nodeType;
    QString state;
    qint64 elapsedMs = -1;
    QString errorCode;
    QString modelId;
    QString promptVersion;
};

struct FunctionFlowNodeExecutionEvent
{
    ExecutionId runId;
    QString functionId;
    int publishedRevision = 0;
    QString publishedHash;
    FunctionFlowTrigger trigger = FunctionFlowTrigger::MainHotkey;
    QString nodeId;
    FunctionFlowNodeType nodeType = FunctionFlowNodeType::Input;
    FunctionFlowNodeState state = FunctionFlowNodeState::Pending;
    qint64 elapsedMs = -1;
    QString errorCode;
    QString modelId;
    QString promptVersion;
};

Q_DECLARE_METATYPE(FunctionFlowNodeExecutionEvent)

struct FunctionFlowRunExecutionEvent
{
    ExecutionId runId;
    QString functionId;
    int publishedRevision = 0;
    QString publishedHash;
    FunctionFlowTrigger trigger = FunctionFlowTrigger::MainHotkey;
    bool running = false;
    bool cancelled = false;
    OperationError terminalError;
};

Q_DECLARE_METATYPE(FunctionFlowRunExecutionEvent)

struct FunctionFlowHistoryRequest
{
    ExecutionId runId;
    QString functionId;
    QString functionTitle;
    QString recordDirectory;
    int publishedRevision = 0;
    QString publishedHash;
    QString trigger;
    QString canonicalInput;
    QString finalOutput;
    QString pendingEditedText;
    OperationError terminalError;
    QVector<FunctionFlowNodeTrace> traces;
    QString failedNodeId;
    QString failedNodeType;
    QString sourceAudioPath;
    QVector<RecordingSegment> recordingSegments;
    qint64 speechElapsedMs = -1;
    QString recordingTriggerMode;
    bool longRecording = false;
    QString ocrEngineId;
    qint64 ocrElapsedMs = -1;
    bool ocrUsedFallback = false;
    QRect screenshotRect;
    bool cancelled = false;
};

struct FunctionFlowHistorySaveResult
{
    bool ok = false;
    QString detailPath;
    bool alreadyExists = false;
    OperationError error;
};

struct FunctionFlowHistoryEditRequest
{
    ExecutionId runId;
    QString recordDirectory;
    QString detailPath;
    QString editedText;
};

struct FunctionFlowHistoryEditResult
{
    bool ok = false;
    OperationError error;
};

struct FunctionFlowExecutionOptions
{
    int cancellationGraceMs = 3000;
};

#endif // VOCEKIT_FUNCTION_FLOW_RUNTIME_TYPES_H
