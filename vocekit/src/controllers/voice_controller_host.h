#ifndef VOCEKIT_VOICE_CONTROLLER_HOST_H
#define VOCEKIT_VOICE_CONTROLLER_HOST_H

#include <QString>

class QWidget;
struct VocabularyEntry;

// VoiceControllerHost 是语音控制器依赖主界面的最小接口。
// 这样 VoiceController 后续可以搬到独立 .h/.cpp，而不需要包含完整 HubWindow。
class VoiceControllerHost
{
public:
    virtual ~VoiceControllerHost() {}

    virtual QWidget *voiceControllerHostWidget() = 0;
    virtual void showVoiceAssistantHub() = 0;
    virtual void notifyVocabularyChangedForVoiceController() = 0;
    virtual void notifySettingsChangedForVoiceController() = 0;
    virtual void openVocabularyEntryEditorForVoiceController(
        const VocabularyEntry &entry
    ) = 0;
    virtual void notifyHistoryRecordSavedForVoiceController(
        const QString &filePath
    ) = 0;
};

#endif // VOCEKIT_VOICE_CONTROLLER_HOST_H
