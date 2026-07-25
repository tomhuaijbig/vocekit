#ifndef VOCEKIT_HISTORY_SEGMENTS_WIDGET_H
#define VOCEKIT_HISTORY_SEGMENTS_WIDGET_H

#include "../recording/segmented_recording.h"

#include <QVector>
#include <functional>

class QFrame;
class QPushButton;
class QWidget;

struct HistorySegmentsCallbacks
{
    std::function<QString(qint64)> elapsedText;
    std::function<void(const RecordingSegment &)> playSegment;
    std::function<void(const RecordingSegment &)> copySegment;
    std::function<void(const RecordingSegment &, QPushButton *)> retrySegment;
};

// 历史详情中的长录音分段卡片：只负责显示分段、播放/复制/重试按钮入口。
// 具体重试识别仍由上层回调完成，避免 UI 模块直接依赖语音接口。
QFrame *createHistorySegmentsCard(
    const QVector<RecordingSegment> &segments,
    const HistorySegmentsCallbacks &callbacks,
    QWidget *context,
    QWidget *parent = nullptr
);

#endif // VOCEKIT_HISTORY_SEGMENTS_WIDGET_H
