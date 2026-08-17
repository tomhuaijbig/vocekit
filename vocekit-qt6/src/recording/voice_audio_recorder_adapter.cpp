#include <QtCore>
#include <QtMultimedia>

#include "../file_utils.h"
#include "../storage/history_paths.h"
#include "../tasks/diagnostic_helpers.h"
#include "segmented_recording.h"

static QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

#include "audio_recorder_legacy.h"
#include "voice_audio_recorder_adapter.h"

struct VoiceAudioRecorderAdapter::Impl
{
    AudioRecorder recorder;
};

VoiceAudioRecorderAdapter::VoiceAudioRecorderAdapter()
    : m_impl(new Impl)
{
}

VoiceAudioRecorderAdapter::~VoiceAudioRecorderAdapter()
{
}

VoiceRecordingCaptureHandlers VoiceAudioRecorderAdapter::handlers()
{
    VoiceRecordingCaptureHandlers result;
    result.start = [this](
        const QString &title,
        const QString &directory,
        bool directDirectory,
        QString *error
    ) {
        return directDirectory
            ? m_impl->recorder.startInDirectory(title, directory, error)
            : m_impl->recorder.start(title, directory, error);
    };
    result.stop = [this]() {
        return m_impl->recorder.stop();
    };
    result.lastWavPath = [this]() {
        return m_impl->recorder.lastWavPath();
    };
    result.takePeakLevel = [this]() {
        return m_impl->recorder.takePeakLevel();
    };
    result.setPcmListener = [this](
        const std::function<void(const QByteArray &)> &listener
    ) {
        m_impl->recorder.setPcmListener(listener);
    };
    return result;
}
