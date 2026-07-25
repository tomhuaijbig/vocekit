#include "voice_long_recording_result_builder.h"

namespace {

QString textUtf8(const char *text)
{
    return QString::fromUtf8(text);
}

QString noSuccessfulSegmentError()
{
    return textUtf8(
        "\xE6\x89\x80\xE6\x9C\x89\xE5\xBD\x95\xE9\x9F\xB3\xE5\x88\x86\xE6\xAE\xB5"
        "\xE9\x83\xBD\xE8\xAF\x86\xE5\x88\xAB\xE5\xA4\xB1\xE8\xB4\xA5\xEF\xBC\x8C"
        "\xE8\xAF\xB7\xE6\xA3\x80\xE6\x9F\xA5\xE7\xBD\x91\xE7\xBB\x9C\xE3\x80\x81"
        "\xE8\xAF\xAD\xE9\x9F\xB3\xE6\x8E\xA5\xE5\x8F\xA3\xE6\x88\x96\xE9\xBA\xA6"
        "\xE5\x85\x8B\xE9\xA3\x8E\xE5\x90\x8E\xE9\x87\x8D\xE8\xAF\x95\xE3\x80\x82"
    );
}

} // namespace

VoiceLongRecordingBuildResult VoiceLongRecordingResultBuilder::build(
    const SegmentedRecordingState &state,
    const QMap<int, QByteArray> &segmentPcm
)
{
    VoiceLongRecordingBuildResult result;
    result.segments = state.segments();
    for (const RecordingSegment &segment : result.segments) {
        result.completePcm.append(segmentPcm.value(segment.index));
    }
    result.mergedText = state.mergedText();
    result.successfulSegmentCount = state.successfulSegmentCount();
    result.failedSegmentCount = state.failedSegmentCount();
    result.noSuccessfulSegmentError = noSuccessfulSegmentError();
    return result;
}
