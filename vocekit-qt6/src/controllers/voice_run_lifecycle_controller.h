#ifndef VOCEKIT_VOICE_RUN_LIFECYCLE_CONTROLLER_H
#define VOCEKIT_VOICE_RUN_LIFECYCLE_CONTROLLER_H

#include "../config/app_settings_data.h"
#include "../domain/prompt_runtime_library.h"
#include "../domain/voice_history_recorder.h"
#include "../domain/voice_run_executor.h"
#include "../tasks/cancellation_token.h"

#include <QObject>

#include <functional>

class VoiceRunSession;

struct VoiceRunLifecycleHistoryRequest
{
    QString modeId;
    QString input;
    QString output;
    QString error;
    bool draft = false;
    QString modelOverride;
};

// 运行生命周期通过访问接口使用提示词、模型、历史存储和外层通知能力。
struct VoiceRunLifecycleAccess
{
    std::function<PromptRuntimeSnapshot()> promptSnapshot;
    std::function<VoiceModelRuntimeSettings(
        const AppSettingsData &settings,
        const PromptRuntimeSnapshot &prompts,
        const QString &modeId
    )> runtimeSettings;
    std::function<QString(
        const AppSettingsData &settings,
        const QString &modeId,
        const QString &userText,
        bool hasVoiceInput
    )> vocabularyPromptBlock;
    std::function<VoiceModelProcessingResult(
        const VoiceModelProcessingRequest &request
    )> processModelRequest;
    std::function<QString(
        const AppSettingsData &settings,
        const QString &output,
        const QString &modeId,
        bool hasVoiceInput
    )> postCorrectOutput;
    std::function<VoiceHistorySaveResult(
        const VoiceHistorySaveRequest &request
    )> persistHistory;
    std::function<QString(const QString &recordDirectory)>
        resolveHistoryRoot;
    std::function<QString(
        const AppSettingsData &settings,
        const QString &modeId
    )> modeTitle;
    std::function<QString()> fallbackAudioPath;
    std::function<qint64()> elapsedMs;
    std::function<void(const QString &modeDetailPath)> historySaved;
    std::function<void(
        const QString &action,
        const QString &detail,
        qint64 elapsedMs
    )> historyLogged;
};

VoiceRunLifecycleAccess defaultVoiceRunLifecycleAccess();

// 集中管理一次功能运行的模型执行、输出后修正和历史持久化。
class VoiceRunLifecycleController : public QObject
{
public:
    VoiceRunLifecycleController(
        const VoiceRunLifecycleAccess &access,
        VoiceRunSession *runSession,
        QObject *parent = nullptr
    );
    ~VoiceRunLifecycleController() override;

    void updateConfiguration(const AppSettingsData &settings);
    void cancelActiveModel();
    bool lastModelRunCancelled() const;

    QString runModel(
        const VoiceRunContext &context,
        const QString &modelOverride,
        const QString &extraInstruction,
        QString *error,
        const VoiceRunDeltaCallback &onDelta =
            VoiceRunDeltaCallback()
    );
    QString finalizeOutput(
        const VoiceRunContext &context,
        const QString &output
    ) const;
    void saveHistory(
        const VoiceRunContext &context,
        const QString &output,
        const QString &error,
        bool draft = false,
        const QString &modelOverride = QString()
    );
    void saveHistory(const VoiceRunLifecycleHistoryRequest &request);

private:
    VoiceRunLifecycleAccess m_access;
    VoiceRunSession *m_runSession;
    AppSettingsData m_settings;
    CancellationSource m_modelCancellation;
    bool m_lastModelRunCancelled = false;
};

#endif // VOCEKIT_VOICE_RUN_LIFECYCLE_CONTROLLER_H
