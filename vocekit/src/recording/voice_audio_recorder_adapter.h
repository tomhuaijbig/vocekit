#ifndef VOCEKIT_VOICE_AUDIO_RECORDER_ADAPTER_H
#define VOCEKIT_VOICE_AUDIO_RECORDER_ADAPTER_H

#include "voice_recording_capture.h"

#include <memory>

// Qt 麦克风适配器：把底层录音器接入录音采集模块的处理接口。
class VoiceAudioRecorderAdapter
{
public:
    VoiceAudioRecorderAdapter();
    ~VoiceAudioRecorderAdapter();

    VoiceRecordingCaptureHandlers handlers();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

#endif // VOCEKIT_VOICE_AUDIO_RECORDER_ADAPTER_H
