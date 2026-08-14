#ifndef VOCEKIT_VOICE_CONTROLLER_H
#define VOCEKIT_VOICE_CONTROLLER_H

#include "../config/app_settings_data.h"
#include "../domain/function_flow_runtime_types.h"
#include "../domain/prompt_runtime_library.h"
#include "selected_text_workflow_controller.h"

#include <QObject>
#include <QSet>
#include <QString>

#include <functional>

class FloatingBar;
class VoiceControllerHost;
struct FunctionFlowCompiledNode;
struct VocabularySuggestion;

// 控制器只读取运行快照；少量窗口状态通过统一回调保存。
struct VoiceControllerAccess
{
    std::function<AppSettingsData()> settingsSnapshotProvider;
    std::function<PromptRuntimeSnapshot()> promptSnapshotProvider;
    std::function<bool(const AppSettingsData &)> applyAndSave;
    std::function<FunctionFlowStartOutcome(
        const FunctionFlowTriggerRequest &
    )> startPublishedFlow;
    std::function<bool()> flowProcessing;
};

// 语音业务控制器的公开外壳。
// 具体实现放在 voice_controller.cpp 的 Impl 中，主窗口只依赖这组稳定方法。
class VoiceController : public QObject
{
public:
    VoiceController(
        const VoiceControllerAccess &access,
        FloatingBar *bar,
        VoiceControllerHost *host,
        QObject *parent = nullptr
    );
    ~VoiceController();

    void reload();
    void setActiveHoldFunctions(const QSet<QString> &ids);
    void handleHotkeyPressed(const QString &id);
    void handleHotkeyReleased(const QString &id);
    void handleScreenshotTrigger(const QString &id);
    void handleScreenshotLauncherTrigger(
        const QString &id,
        FunctionFlowTargetWindowHandle targetWindow
    );
    bool beginVoiceForFlow(
        const FunctionFlowRunContext &run,
        const FunctionFlowCompiledNode &node,
        const FunctionFlowNodeCompletion &completion
    );
    bool beginScreenshotForFlow(
        const FunctionFlowRunContext &run,
        const FunctionFlowCompiledNode &node,
        const FunctionFlowNodeCompletion &completion
    );
    SelectedTextWorkflowResult readSelectedTextForFlow(
        const SelectedTextWorkflowRequest &request
    ) const;
    void addVocabularyForFlow(
        const QString &sourceText,
        const QString &scopeId,
        const QString &editedText
    );
    void addVocabularyLocallyForFlow(
        const QString &sourceText,
        const QString &scopeId,
        const QString &editedText
    );
    VocabularySuggestion suggestVocabularyEntry(
        const QString &sourceText,
        const QString &scopeId,
        QString *error,
        const QString &editedText = QString(),
        const QString &extraContext = QString()
    );
    void handleHotkey(const QString &id);

private:
    class Impl;
    Impl *d;
};

#endif // VOCEKIT_VOICE_CONTROLLER_H
