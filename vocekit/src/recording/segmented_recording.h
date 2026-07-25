#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QVector>

// 录音功能配置：供内置功能和自定义功能共用，并集中约束可保存的数值范围。
struct RecordingFunctionSettings
{
    QString triggerMode = QStringLiteral("toggle");
    bool longRecordingEnabled = false;
    int segmentSeconds = 55;
    int maxRecordingMinutes = 30;
};

// 长录音分段状态：识别队列和历史记录都使用同一份结构。
struct RecordingSegment
{
    int index = 0;
    QString wavPath;
    QString text;
    QString error;
    qint64 recognitionElapsedMs = -1;
    int attempts = 0;
};

inline QString normalizeRecordingTriggerMode(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    return normalized == QStringLiteral("hold")
        ? QStringLiteral("hold")
        : QStringLiteral("toggle");
}

inline int normalizeRecordingSegmentSeconds(int seconds)
{
    return qBound(20, seconds, 55);
}

inline int normalizeMaxRecordingMinutes(int minutes)
{
    return qBound(1, minutes, 30);
}

bool canStartRecordingSegment(int index);
QByteArray wavFromPcm(
    const QByteArray &pcm,
    int sampleRate,
    int channels,
    int bitsPerSample
);
QByteArray pcm16FromWavData(const QByteArray &wavData, QString *error);

inline RecordingFunctionSettings recordingFunctionSettingsFromJson(
    const QJsonObject &object
)
{
    RecordingFunctionSettings settings;
    settings.triggerMode = normalizeRecordingTriggerMode(
        object.value(QStringLiteral("recordingTriggerMode")).toString()
    );
    settings.longRecordingEnabled = object.value(
        QStringLiteral("longRecordingEnabled")
    ).toBool(false);
    settings.segmentSeconds = normalizeRecordingSegmentSeconds(
        object.value(QStringLiteral("segmentSeconds")).toInt(55)
    );
    settings.maxRecordingMinutes = normalizeMaxRecordingMinutes(
        object.value(QStringLiteral("maxRecordingMinutes")).toInt(30)
    );
    return settings;
}

inline QJsonObject recordingFunctionSettingsToJson(
    const RecordingFunctionSettings &settings
)
{
    QJsonObject object;
    object.insert(
        QStringLiteral("recordingTriggerMode"),
        normalizeRecordingTriggerMode(settings.triggerMode)
    );
    object.insert(
        QStringLiteral("longRecordingEnabled"),
        settings.longRecordingEnabled
    );
    object.insert(
        QStringLiteral("segmentSeconds"),
        normalizeRecordingSegmentSeconds(settings.segmentSeconds)
    );
    object.insert(
        QStringLiteral("maxRecordingMinutes"),
        normalizeMaxRecordingMinutes(settings.maxRecordingMinutes)
    );
    return object;
}

// 分段识别状态：保证识别重试和最终文本始终按录音段号处理。
class SegmentedRecordingState
{
public:
    void clear();
    void addSegment(int index, const QString &wavPath);
    bool markAttemptStarted(int index);
    void recordResult(
        int index,
        const QString &text,
        const QString &error,
        qint64 recognitionElapsedMs
    );
    void recordTerminalFailure(
        int index,
        const QString &error,
        qint64 recognitionElapsedMs = -1
    );

    int nextPendingIndex() const;
    RecordingSegment segment(int index) const;
    QVector<RecordingSegment> segments() const;
    QString mergedText() const;
    int successfulSegmentCount() const;
    int failedSegmentCount() const;

private:
    QMap<int, RecordingSegment> m_segments;
};
