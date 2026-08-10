#include <QtTest>

#include <QtCore>
#include <QtMultimedia>

#include "../../src/file_utils.h"
#include "../../src/storage/history_paths.h"
#include "../../src/tasks/diagnostic_helpers.h"

static QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

int pcm16PeakLevel(const QByteArray &pcm)
{
    int peak = 0;
    for (int i = 0; i + 1 < pcm.size(); i += 2) {
        const uchar low = static_cast<uchar>(pcm.at(i));
        const uchar high = static_cast<uchar>(pcm.at(i + 1));
        const qint16 sample = static_cast<qint16>(
            (static_cast<int>(high) << 8) | static_cast<int>(low)
        );
        peak = qMax(peak, qAbs(static_cast<int>(sample)));
    }
    return peak;
}

#include "../../src/recording/audio_recorder_legacy.h"

class VoiceAudioRecorderStreamTests : public QObject
{
    Q_OBJECT

private slots:
    void mirrorsOnlyBytesAcceptedByTheFile()
    {
        QTemporaryFile file;
        QVERIFY(file.open());
        QByteArray observed;
        AudioCaptureDevice capture(&file);
        capture.setPcmListener([&observed](const QByteArray &pcm) {
            observed.append(pcm);
        });
        QVERIFY(capture.open(QIODevice::WriteOnly));

        const QByteArray pcm = QByteArray::fromHex("00010203feff");
        QCOMPARE(capture.write(pcm), qint64(pcm.size()));
        file.flush();
        QVERIFY(file.seek(0));

        QCOMPARE(file.readAll(), pcm);
        QCOMPARE(observed, pcm);
    }

    void replacementAndClearNeverCallStaleListener()
    {
        QTemporaryFile file;
        QVERIFY(file.open());
        QByteArray first;
        QByteArray second;
        AudioCaptureDevice capture(&file);
        QVERIFY(capture.open(QIODevice::WriteOnly));

        capture.setPcmListener([&first](const QByteArray &pcm) {
            first.append(pcm);
        });
        QCOMPARE(capture.write(QByteArrayLiteral("one")), qint64(3));

        capture.setPcmListener([&second](const QByteArray &pcm) {
            second.append(pcm);
        });
        QCOMPARE(capture.write(QByteArrayLiteral("two")), qint64(3));

        capture.setPcmListener(std::function<void(const QByteArray &)>());
        QCOMPARE(capture.write(QByteArrayLiteral("three")), qint64(5));

        QCOMPARE(first, QByteArrayLiteral("one"));
        QCOMPARE(second, QByteArrayLiteral("two"));
    }
};

QTEST_MAIN(VoiceAudioRecorderStreamTests)

#include "voice_audio_recorder_stream_tests.moc"
