#ifndef VOCEKIT_VOICE_RECORDING_WORKFLOW_CONTROLLER_H
#define VOCEKIT_VOICE_RECORDING_WORKFLOW_CONTROLLER_H

#include "../domain/function_flow_compiler.h"
#include "../providers/streaming_speech_session_factory.h"
#include "../recording/voice_recording_capture.h"
#include "../tasks/voice_speech_recognition_executor.h"

#include <QObject>
#include <QSet>
#include <QString>
#include <QtGlobal>

#include <functional>

class FloatingBar;
class VoiceRunSession;
struct AppSettingsData;

using VoiceRecordingFlowSpeechCompletion =
    std::function<void(const VoiceSpeechRecognitionResult &)>;

// 录音工作流只通过这些回调接入外层功能执行，不依赖 VoiceController。
struct VoiceRecordingWorkflowAccess
{
    std::function<qint64()> elapsedMs;
    std::function<bool()> externalProcessing;
    std::function<void(bool)> processingChanged;
    std::function<void(const QString &)> showFailure;
    std::function<void(const QString &, const QString &)>
        showWindowsSpeechFailure;
    std::function<void(const QString &, const QString &)> saveFailureHistory;
    std::function<void(const QString &, const QString &)> processRecognizedSpeech;
    VoiceRecordingCaptureHandlers recordingCapture;
    VoiceSpeechRecognitionHandlers speechRecognition;
    // A playable path selects the configured sound; an empty path requests
    // the system-beep fallback.
    std::function<void(const QString &)> playRecordingBeep;
    std::function<void(
        const VoiceSpeechRecognitionRequest &,
        const VoiceSpeechRecognitionHandlers &,
        const VoiceRecordingFlowSpeechCompletion &
    )> runSpeechRecognition;
    std::function<StreamingSpeechSessionCreation(
        const StreamingSpeechSessionRequest &,
        const StreamingSpeechCallbacks &
    )> createStreamingSpeechSession;
    int streamingFinalTimeoutMs = 5000;
    int holdReleaseWarmupTimeoutMs = 500;
};

// 完整管理录音准备、采集、长录音分段、语音识别和录音结果元数据。
class VoiceRecordingWorkflowController : public QObject
{
public:
    VoiceRecordingWorkflowController(
        const VoiceRecordingWorkflowAccess &access,
        FloatingBar *bar,
        VoiceRunSession *runSession,
        QObject *parent = nullptr
    );
    ~VoiceRecordingWorkflowController() override;

    void updateConfiguration(const AppSettingsData &settings);
    void setActiveHoldFunctions(const QSet<QString> &ids);

    bool begin(const QString &functionId);
    bool beginForFlow(
        const FunctionFlowRunContext &run,
        const FunctionFlowCompiledNode &node,
        const FunctionFlowNodeCompletion &completion
    );
    bool ownsPress(const QString &functionId) const;
    bool handleHotkey(const QString &functionId);
    bool handleFlowHotkeyReleased(const QString &functionId);
    bool handleHotkeyReleased(const QString &functionId);
    bool confirmActiveRecording();
    bool cancelActiveRecording();

    bool isBusy() const;
    bool isPreparing() const;
    bool isRecording() const;
    QString lastWavPath() const;

private:
    class Impl;
    Impl *d;
};

#endif // VOCEKIT_VOICE_RECORDING_WORKFLOW_CONTROLLER_H
