#ifndef VOCEKIT_VOICE_LONG_RECORDING_SESSION_H
#define VOCEKIT_VOICE_LONG_RECORDING_SESSION_H

#include "segmented_recording.h"

#include <QByteArray>
#include <QMap>
#include <QString>

// 长录音会话模块：集中维护分段状态、PCM 数据和会话生命周期。
class VoiceLongRecordingSession
{
public:
    void begin(
        bool enabled,
        const QString &audioDirectory,
        const QString &fileBase
    );
    void disable();

    bool isActive() const;
    bool isFinalizing() const;
    int currentSegmentIndex() const;
    QString audioDirectory() const;
    QString fileBase() const;

    void addCurrentSegment(
        const QString &wavPath,
        const QByteArray &pcm
    );
    void recordCurrentTerminalFailure(
        const QString &error,
        qint64 recognitionElapsedMs = -1
    );
    bool advanceToNextSegment();
    bool beginFinalizing();
    void complete();

    SegmentedRecordingState &recognitionState();
    const SegmentedRecordingState &recognitionState() const;
    const QMap<int, QByteArray> &pcmBySegment() const;

private:
    SegmentedRecordingState m_recognitionState;
    QMap<int, QByteArray> m_pcmBySegment;
    bool m_active = false;
    bool m_finalizing = false;
    int m_currentSegmentIndex = 0;
    QString m_audioDirectory;
    QString m_fileBase;
};

#endif // VOCEKIT_VOICE_LONG_RECORDING_SESSION_H
