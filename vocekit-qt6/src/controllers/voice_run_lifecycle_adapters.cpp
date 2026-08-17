#include "voice_run_lifecycle_controller.h"

#include "../domain/function_catalog.h"
#include "../domain/vocabulary_runtime.h"
#include "../domain/voice_history_recorder.h"
#include "../storage/history_paths.h"
#include "../tasks/voice_model_processing_task.h"
#include "../tasks/voice_model_runtime_settings.h"

VoiceRunLifecycleAccess defaultVoiceRunLifecycleAccess()
{
    VoiceRunLifecycleAccess access;
    access.runtimeSettings = [](
        const AppSettingsData &,
        const PromptRuntimeSnapshot &prompts,
        const QString &modeId
    ) {
        return buildVoiceModelRuntimeSettings(
            prompts.settings,
            modeId,
            promptRuntimeForFunction(
                prompts,
                modeId,
                defaultVoiceModelSystemPrompt(modeId)
            )
        );
    };
    access.vocabularyPromptBlock = [](
        const AppSettingsData &settings,
        const QString &modeId,
        const QString &userText,
        bool hasVoiceInput
    ) {
        return addVocabularyPromptBlockForRun(
            settings,
            modeId,
            userText,
            hasVoiceInput
        );
    };
    access.processModelRequest =
        [](const VoiceModelProcessingRequest &request) {
            return processVoiceModelRequest(request);
        };
    access.postCorrectOutput = [](
        const AppSettingsData &settings,
        const QString &output,
        const QString &modeId,
        bool hasVoiceInput
    ) {
        return applyVocabularyPostCorrectionForRun(
            settings,
            output,
            modeId,
            hasVoiceInput
        );
    };
    access.persistHistory =
        [](const VoiceHistorySaveRequest &request) {
            return VoiceHistoryRecorder::save(request);
        };
    access.resolveHistoryRoot = [](const QString &recordDirectory) {
        return historyRootPath(recordDirectory);
    };
    access.modeTitle = [](
        const AppSettingsData &settings,
        const QString &modeId
    ) {
        return functionDisplayTitle(
            settings,
            modeId,
            QString::fromUtf8("自定义功能")
        );
    };
    return access;
}
