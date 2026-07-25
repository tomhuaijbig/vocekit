#include <QtTest>

#include "../../src/controllers/screenshot_workflow_controller.h"

#include <QFile>
#include <type_traits>

class ScreenshotWorkflowControllerTests : public QObject
{
    Q_OBJECT

private slots:
    void exposesIndependentWorkflowInterface();
    void voiceControllerDoesNotOwnScreenshotImplementation();
};

void ScreenshotWorkflowControllerTests::exposesIndependentWorkflowInterface()
{
    QVERIFY((std::is_default_constructible<ScreenshotWorkflowAccess>::value));
    QVERIFY((std::is_default_constructible<ScreenshotWorkflowStartRequest>::value));
    QVERIFY((std::is_constructible<
        ScreenshotWorkflowController,
        const ScreenshotWorkflowAccess &,
        FloatingBar *,
        QObject *
    >::value));
}

void ScreenshotWorkflowControllerTests::voiceControllerDoesNotOwnScreenshotImplementation()
{
    const QString voicePath = QFINDTESTDATA(
        "../../src/controllers/voice_controller.cpp"
    );
    const QString screenshotPath = QFINDTESTDATA(
        "../../src/controllers/screenshot_workflow_controller.cpp"
    );
    QVERIFY2(!voicePath.isEmpty(), "找不到 VoiceController 源文件");
    QVERIFY2(!screenshotPath.isEmpty(), "找不到截图工作流控制器源文件");

    QFile voiceSource(voicePath);
    QFile screenshotSource(screenshotPath);
    QVERIFY(voiceSource.open(QIODevice::ReadOnly));
    QVERIFY(screenshotSource.open(QIODevice::ReadOnly));
    const QByteArray voiceContents = voiceSource.readAll();
    const QByteArray screenshotContents = screenshotSource.readAll();

    QVERIFY(voiceContents.contains("ScreenshotWorkflowController"));
    QVERIFY(!voiceContents.contains("m_screenshotOcrManager"));
    QVERIFY(!voiceContents.contains("m_screenshotAiWatcher"));
    QVERIFY(!voiceContents.contains("m_captureOverlay"));
    QVERIFY(!voiceContents.contains("m_screenshotSession"));
    QVERIFY(!voiceContents.contains("handleScreenshotCaptured("));
    QVERIFY(!voiceContents.contains("handleScreenshotOcrFinished("));
    QVERIFY(!voiceContents.contains("handleScreenshotWorkbenchAction("));

    QVERIFY(screenshotContents.contains("OcrManager"));
    QVERIFY(screenshotContents.contains("ScreenCaptureOverlay"));
    QVERIFY(screenshotContents.contains("VoiceScreenshotSession"));
    QVERIFY(screenshotContents.contains("runScreenshotTextActionTask"));
}

QTEST_APPLESS_MAIN(ScreenshotWorkflowControllerTests)

#include "screenshot_workflow_controller_tests.moc"
