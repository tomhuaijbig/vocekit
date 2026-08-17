#ifndef VOCEKIT_VOICE_LONG_RECORDING_RESULT_BUILDER_H
#define VOCEKIT_VOICE_LONG_RECORDING_RESULT_BUILDER_H

#include "../recording/segmented_recording.h"

#include <QByteArray>
#include <QMap>
#include <QString>
#include <QVector>

struct VoiceLongRecordingBuildResult
{
    QVector<RecordingSegment> segments;
    QByteArray completePcm;
    QString mergedText;
    QString noSuccessfulSegmentError;
    int successfulSegmentCount = 0;
    int failedSegmentCount = 0;
};

class VoiceLongRecordingResultBuilder
{
public:
    static VoiceLongRecordingBuildResult build(
        const SegmentedRecordingState &state,
        const QMap<int, QByteArray> &segmentPcm
    );
};

#endif // VOCEKIT_VOICE_LONG_RECORDING_RESULT_BUILDER_H
