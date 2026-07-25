#include "segmented_recording.h"

#include <QDataStream>
#include <QStringList>

bool canStartRecordingSegment(int index)
{
    return index >= 1 && index <= 33;
}

QByteArray wavFromPcm(
    const QByteArray &pcm,
    int sampleRate,
    int channels,
    int bitsPerSample
)
{
    QByteArray wav;
    QDataStream stream(&wav, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    const int byteRate = sampleRate * channels * bitsPerSample / 8;
    const int blockAlign = channels * bitsPerSample / 8;
    const int dataSize = pcm.size();

    stream.writeRawData("RIFF", 4);
    stream << quint32(36 + dataSize);
    stream.writeRawData("WAVE", 4);
    stream.writeRawData("fmt ", 4);
    stream << quint32(16);
    stream << quint16(1);
    stream << quint16(channels);
    stream << quint32(sampleRate);
    stream << quint32(byteRate);
    stream << quint16(blockAlign);
    stream << quint16(bitsPerSample);
    stream.writeRawData("data", 4);
    stream << quint32(dataSize);
    wav.append(pcm);
    return wav;
}

QByteArray pcm16FromWavData(const QByteArray &wavData, QString *error)
{
    const auto read16 = [&wavData](int offset) {
        const uchar *data = reinterpret_cast<const uchar *>(wavData.constData() + offset);
        return quint16(data[0]) | (quint16(data[1]) << 8);
    };
    const auto read32 = [&wavData](int offset) {
        const uchar *data = reinterpret_cast<const uchar *>(wavData.constData() + offset);
        return quint32(data[0])
            | (quint32(data[1]) << 8)
            | (quint32(data[2]) << 16)
            | (quint32(data[3]) << 24);
    };
    const auto fail = [error](const QString &message) {
        if (error) {
            *error = message;
        }
        return QByteArray();
    };

    if (wavData.size() < 12
        || wavData.mid(0, 4) != QByteArrayLiteral("RIFF")
        || wavData.mid(8, 4) != QByteArrayLiteral("WAVE")) {
        return fail(QStringLiteral("录音文件不是有效的 WAV。"));
    }

    bool formatSupported = false;
    int offset = 12;
    while (offset + 8 <= wavData.size()) {
        const QByteArray chunkId = wavData.mid(offset, 4);
        const quint32 chunkSize = read32(offset + 4);
        const qint64 dataOffset = qint64(offset) + 8;
        const qint64 nextOffset = dataOffset + chunkSize + (chunkSize & 1U);
        if (chunkSize > quint32(wavData.size())
            || dataOffset + chunkSize > wavData.size()
            || nextOffset > wavData.size() + 1LL) {
            return fail(QStringLiteral("WAV 分块长度无效。"));
        }

        if (chunkId == QByteArrayLiteral("fmt ")) {
            if (chunkSize < 16) {
                return fail(QStringLiteral("WAV 格式信息不完整。"));
            }
            const quint16 format = read16(static_cast<int>(dataOffset));
            const quint16 channels = read16(static_cast<int>(dataOffset + 2));
            const quint16 bits = read16(static_cast<int>(dataOffset + 14));
            formatSupported = format == 1 && channels == 1 && bits == 16;
        } else if (chunkId == QByteArrayLiteral("data")) {
            if (!formatSupported) {
                return fail(QStringLiteral("只支持 16 位单声道 PCM WAV。"));
            }
            if (error) {
                error->clear();
            }
            return wavData.mid(static_cast<int>(dataOffset), static_cast<int>(chunkSize));
        }
        offset = static_cast<int>(nextOffset);
    }

    return fail(QStringLiteral("WAV 中没有找到音频数据。"));
}

void SegmentedRecordingState::clear()
{
    m_segments.clear();
}

void SegmentedRecordingState::addSegment(
    int index,
    const QString &wavPath
)
{
    if (index <= 0) {
        return;
    }
    RecordingSegment entry;
    entry.index = index;
    entry.wavPath = wavPath;
    m_segments.insert(index, entry);
}

bool SegmentedRecordingState::markAttemptStarted(int index)
{
    if (!m_segments.contains(index)) {
        return false;
    }
    RecordingSegment &entry = m_segments[index];
    if (!entry.text.isEmpty() || entry.attempts >= 2) {
        return false;
    }
    ++entry.attempts;
    entry.error.clear();
    return true;
}

void SegmentedRecordingState::recordResult(
    int index,
    const QString &text,
    const QString &error,
    qint64 recognitionElapsedMs
)
{
    if (!m_segments.contains(index)) {
        return;
    }
    RecordingSegment &entry = m_segments[index];
    entry.text = text.trimmed();
    entry.error = entry.text.isEmpty() ? error.trimmed() : QString();
    entry.recognitionElapsedMs = recognitionElapsedMs;
}

void SegmentedRecordingState::recordTerminalFailure(
    int index,
    const QString &error,
    qint64 recognitionElapsedMs
)
{
    if (!m_segments.contains(index)) {
        return;
    }
    RecordingSegment &entry = m_segments[index];
    entry.text.clear();
    entry.error = error.trimmed();
    entry.recognitionElapsedMs = recognitionElapsedMs;
    entry.attempts = 2;
}

int SegmentedRecordingState::nextPendingIndex() const
{
    for (auto it = m_segments.constBegin(); it != m_segments.constEnd(); ++it) {
        const RecordingSegment &entry = it.value();
        if (!entry.text.isEmpty()) {
            continue;
        }
        if (entry.attempts == 0
            || (!entry.error.isEmpty() && entry.attempts < 2)) {
            return it.key();
        }
    }
    return -1;
}

RecordingSegment SegmentedRecordingState::segment(int index) const
{
    return m_segments.value(index);
}

QVector<RecordingSegment> SegmentedRecordingState::segments() const
{
    QVector<RecordingSegment> result;
    result.reserve(m_segments.size());
    for (auto it = m_segments.constBegin(); it != m_segments.constEnd(); ++it) {
        result.append(it.value());
    }
    return result;
}

QString SegmentedRecordingState::mergedText() const
{
    QStringList parts;
    for (auto it = m_segments.constBegin(); it != m_segments.constEnd(); ++it) {
        const RecordingSegment &entry = it.value();
        if (!entry.text.isEmpty()) {
            parts.append(entry.text);
        } else if (entry.attempts >= 2 || !entry.error.isEmpty()) {
            parts.append(
                QStringLiteral("[第 %1 段识别失败]").arg(entry.index)
            );
        }
    }
    return parts.join(QStringLiteral("\n"));
}

int SegmentedRecordingState::successfulSegmentCount() const
{
    int count = 0;
    for (auto it = m_segments.constBegin(); it != m_segments.constEnd(); ++it) {
        if (!it.value().text.isEmpty()) {
            ++count;
        }
    }
    return count;
}

int SegmentedRecordingState::failedSegmentCount() const
{
    int count = 0;
    for (auto it = m_segments.constBegin(); it != m_segments.constEnd(); ++it) {
        if (it.value().text.isEmpty()
            && (it.value().attempts >= 2 || !it.value().error.isEmpty())) {
            ++count;
        }
    }
    return count;
}
