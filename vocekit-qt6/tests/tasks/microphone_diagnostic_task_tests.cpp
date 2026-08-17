#include <QtTest>

#include "../../src/tasks/microphone_diagnostic_task.h"

namespace {

QByteArray pcmWithSample(qint16 value, int sampleCount = 16000)
{
    QByteArray pcm;
    pcm.reserve(sampleCount * 2);
    for (int i = 0; i < sampleCount; ++i) {
        pcm.append(char(value & 0xff));
        pcm.append(char((value >> 8) & 0xff));
    }
    return pcm;
}

QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

} // namespace

class MicrophoneDiagnosticTaskTests : public QObject
{
    Q_OBJECT

private slots:
    void emptyPcmReportsFailureAndWarning()
    {
        MicrophoneDiagnosticRequest request;

        const MicrophoneDiagnosticResult result = runMicrophoneDiagnosticTask(request);

        QVERIFY(result.showWarning);
        QCOMPARE(result.warningTitle, tr8("麦克风测试失败"));
        QVERIFY(result.displayText.contains(tr8("失败")));
    }

    void quietPcmReportsLowVolume()
    {
        MicrophoneDiagnosticRequest request;
        request.pcm = pcmWithSample(500);

        const MicrophoneDiagnosticResult result = runMicrophoneDiagnosticTask(request);

        QVERIFY(!result.showWarning);
        QVERIFY(result.displayText.contains(tr8("声音偏低")));
        QVERIFY(result.peakPercent > 0);
    }

    void normalPcmReportsPass()
    {
        MicrophoneDiagnosticRequest request;
        request.pcm = pcmWithSample(4000);

        const MicrophoneDiagnosticResult result = runMicrophoneDiagnosticTask(request);

        QVERIFY(!result.showWarning);
        QVERIFY(result.displayText.contains(tr8("通过")));
    }

    void keptSamplePathIsIncluded()
    {
        MicrophoneDiagnosticRequest request;
        request.pcm = pcmWithSample(4000, 8000);
        request.keepSample = true;
        request.samplePath = QStringLiteral("C:/tmp/sample.wav");

        const MicrophoneDiagnosticResult result = runMicrophoneDiagnosticTask(request);

        QVERIFY(result.displayText.contains(QStringLiteral("sample.wav")));
    }
};

QTEST_MAIN(MicrophoneDiagnosticTaskTests)
#include "microphone_diagnostic_task_tests.moc"
