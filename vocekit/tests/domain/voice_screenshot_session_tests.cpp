#include <QtTest>

#include "../../src/domain/voice_screenshot_session.h"

class VoiceScreenshotSessionTests : public QObject
{
    Q_OBJECT

private slots:
    void beginningWorkflowClearsPreviousCaptureState()
    {
        VoiceScreenshotSession session;
        session.beginWorkflow();
        const int previousGeneration = session.generation();

        OcrResult result;
        result.ok = true;
        result.text = QStringLiteral(" old text ");
        result.engine = OcrEngine::RapidOcr;
        session.setCapture(QImage(3, 2, QImage::Format_ARGB32), QRect(1, 2, 3, 2));
        session.setRecognitionResult(result);
        session.setCloudConsent(true);
        session.queuePendingCapture(
            QImage(1, 1, QImage::Format_ARGB32),
            QRect(5, 6, 1, 1)
        );

        session.beginWorkflow();

        QVERIFY(session.isActive());
        QVERIFY(session.generation() > previousGeneration);
        QVERIFY(!session.matchesGeneration(previousGeneration));
        QVERIFY(!session.hasRecognizedText());
        QVERIFY(!session.hasPendingContext());
        QVERIFY(!session.hasPendingCapture());
        QVERIFY(!session.cloudConsent());
    }

    void pendingCaptureKeepsOnlyLatestSelection()
    {
        VoiceScreenshotSession session;
        session.beginWorkflow();
        QImage first(2, 2, QImage::Format_ARGB32);
        first.fill(Qt::red);
        QImage second(4, 3, QImage::Format_ARGB32);
        second.fill(Qt::blue);

        session.queuePendingCapture(first, QRect(1, 1, 2, 2));
        session.queuePendingCapture(second, QRect(9, 8, 4, 3));

        QVERIFY(session.hasPendingCapture());
        const VoiceScreenshotCapture capture = session.takePendingCapture();
        QCOMPARE(capture.image.size(), QSize(4, 3));
        QCOMPARE(capture.rect, QRect(9, 8, 4, 3));
        QVERIFY(!session.hasPendingCapture());
        QVERIFY(session.takePendingCapture().image.isNull());
    }

    void recognitionResultIsAppliedToRunContextOnlyOnce()
    {
        VoiceScreenshotSession session;
        session.beginWorkflow();
        const QImage image(8, 6, QImage::Format_ARGB32);
        const QRect rect(10, 20, 8, 6);
        OcrTextBlock block;
        block.text = QStringLiteral("block");
        OcrResult result;
        result.ok = true;
        result.text = QStringLiteral(" recognized text \n");
        result.engine = OcrEngine::WindowsOcr;
        result.elapsedMs = 321;
        result.usedFallback = true;
        result.blocks.append(block);

        session.setCapture(image, rect);
        session.setRecognitionResult(result);

        VoiceRunContext context;
        QVERIFY(session.applyPendingContext(&context));
        QVERIFY(context.screenshotInput);
        QCOMPARE(context.screenshotImage.size(), QSize(8, 6));
        QCOMPARE(context.screenshotRect, rect);
        QCOMPARE(context.screenshotRecognizedText, QStringLiteral("recognized text"));
        QCOMPARE(context.screenshotBlocks.size(), 1);
        QCOMPARE(context.screenshotOcrEngine, OcrEngine::WindowsOcr);
        QCOMPARE(context.screenshotOcrElapsedMs, qint64(321));
        QVERIFY(context.screenshotOcrUsedFallback);
        QVERIFY(!session.hasPendingContext());
        QVERIFY(!session.applyPendingContext(&context));
    }

    void resetCanPreserveContextWhileInvalidatingCallbacks()
    {
        VoiceScreenshotSession session;
        session.beginWorkflow();
        const int generation = session.generation();
        OcrResult result;
        result.ok = true;
        result.text = QStringLiteral("keep me");
        session.setCapture(QImage(2, 1, QImage::Format_ARGB32), QRect(3, 4, 2, 1));
        session.setRecognitionResult(result);

        session.reset(true);

        QVERIFY(!session.isActive());
        QVERIFY(!session.matchesGeneration(generation));
        QVERIFY(session.hasPendingContext());
        QCOMPARE(session.recognizedText(), QStringLiteral("keep me"));

        VoiceRunContext context;
        QVERIFY(session.applyPendingContext(&context));
        QCOMPARE(context.screenshotRecognizedText, QStringLiteral("keep me"));
    }

    void newCaptureAttemptInvalidatesOldResultAndGeneration()
    {
        VoiceScreenshotSession session;
        session.beginWorkflow();
        OcrResult result;
        result.ok = true;
        result.text = QStringLiteral("stale");
        session.setRecognitionResult(result);
        const int generation = session.generation();

        session.beginCaptureAttempt();

        QVERIFY(!session.matchesGeneration(generation));
        QVERIFY(!session.hasRecognizedText());
        QVERIFY(!session.hasPendingContext());
    }
};

QTEST_APPLESS_MAIN(VoiceScreenshotSessionTests)
#include "voice_screenshot_session_tests.moc"
