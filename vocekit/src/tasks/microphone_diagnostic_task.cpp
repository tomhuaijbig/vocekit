#include "microphone_diagnostic_task.h"

#include "diagnostic_helpers.h"

#include <QDir>
#include <QtGlobal>

namespace {

QString mdTr8(const char *text)
{
    return QString::fromUtf8(text);
}

struct PcmStatistics
{
    int peak = 0;
    int peakPercent = 0;
    int averagePercent = 0;
    int sampleCount = 0;
    int clippedSamples = 0;
    bool clipped = false;
};

PcmStatistics calculatePcmStatistics(const QByteArray &pcm)
{
    PcmStatistics statistics;
    statistics.peak = pcm16PeakLevel(pcm);

    qint64 absoluteSum = 0;
    for (int i = 0; i + 1 < pcm.size(); i += 2) {
        const uchar lo = static_cast<uchar>(pcm.at(i));
        const uchar hi = static_cast<uchar>(pcm.at(i + 1));
        const qint16 sample = static_cast<qint16>(
            (static_cast<int>(hi) << 8) | static_cast<int>(lo)
        );
        const int absolute = qAbs(static_cast<int>(sample));
        absoluteSum += absolute;
        ++statistics.sampleCount;
        if (absolute >= 32700) {
            ++statistics.clippedSamples;
        }
    }

    statistics.peakPercent = qBound(
        0,
        qRound(statistics.peak * 100.0 / 32767.0),
        100
    );
    statistics.averagePercent = statistics.sampleCount > 0
        ? qBound(
            0,
            qRound((absoluteSum / static_cast<double>(statistics.sampleCount))
                * 100.0 / 32767.0),
            100
        )
        : 0;
    statistics.clipped = statistics.clippedSamples > qMax(2, statistics.sampleCount / 1000);
    return statistics;
}

QString buildDetail(
    const MicrophoneDiagnosticRequest &request,
    const PcmStatistics &statistics
)
{
    const int sampleRate = request.sampleRate > 0 ? request.sampleRate : 16000;
    const double seconds = request.pcm.size() / 2.0 / sampleRate;
    QString detail = mdTr8("录音时长：%1 秒\n峰值音量：%2%\n平均音量：%3%\n削波：%4")
        .arg(QString::number(seconds, 'f', 1))
        .arg(statistics.peakPercent)
        .arg(statistics.averagePercent)
        .arg(statistics.clipped ? mdTr8("检测到") : mdTr8("未检测到"));
    if (request.keepSample) {
        detail += mdTr8("\n样本路径：") + QDir::toNativeSeparators(request.samplePath);
    } else {
        detail += mdTr8("\n测试样本已自动清理。");
    }
    return detail;
}

} // namespace

MicrophoneDiagnosticResult runMicrophoneDiagnosticTask(
    const MicrophoneDiagnosticRequest &request
)
{
    const PcmStatistics statistics = calculatePcmStatistics(request.pcm);
    const QString detail = buildDetail(request, statistics);

    MicrophoneDiagnosticResult result;
    result.peakPercent = statistics.peakPercent;
    result.averagePercent = statistics.averagePercent;
    result.clipped = statistics.clipped;

    if (request.pcm.isEmpty() || statistics.peak < 200) {
        result.displayText = diagnosticStatusLine(
            mdTr8("麦克风测试"),
            mdTr8("失败"),
            detail + mdTr8("没有检测到有效声音。")
        );
        result.showWarning = true;
        result.warningTitle = mdTr8("麦克风测试失败");
        result.warningMessage = mdTr8(
            "没有检测到有效声音。请检查默认麦克风、系统录音权限和输入音量。"
        );
        return result;
    }

    if (statistics.peak < 1200) {
        result.displayText = diagnosticStatusLine(
            mdTr8("麦克风测试"),
            mdTr8("声音偏低"),
            detail + mdTr8("可以录到声音，但输入音量偏低。")
        );
        return result;
    }

    result.displayText = diagnosticStatusLine(
        mdTr8("麦克风测试"),
        mdTr8("通过"),
        detail
    );
    return result;
}
