#include <QtTest>

#include "../../src/controllers/voice_result_presentation_controller.h"

#include <QFile>
#include <type_traits>

class VoiceResultPresentationControllerTests : public QObject
{
    Q_OBJECT

private slots:
    void exposesIndependentPresentationInterface();
    void voiceControllerDoesNotOwnResultWindows();
};

void VoiceResultPresentationControllerTests::
exposesIndependentPresentationInterface()
{
    QVERIFY((std::is_default_constructible<
        VoiceResultPresentationAccess
    >::value));
    QVERIFY((std::is_constructible<
        VoiceResultPresentationController,
        const VoiceResultPresentationAccess &,
        FloatingBar *,
        VoiceRunSession *,
        QObject *
    >::value));
}

void VoiceResultPresentationControllerTests::
voiceControllerDoesNotOwnResultWindows()
{
    const QString voicePath = QFINDTESTDATA(
        "../../src/controllers/voice_controller.cpp"
    );
    const QString presentationPath = QFINDTESTDATA(
        "../../src/controllers/voice_result_presentation_controller.cpp"
    );
    QVERIFY2(!voicePath.isEmpty(), "找不到 VoiceController 源文件");
    QVERIFY2(!presentationPath.isEmpty(), "找不到结果展示控制器源文件");

    QFile voiceSource(voicePath);
    QFile presentationSource(presentationPath);
    QVERIFY(voiceSource.open(QIODevice::ReadOnly));
    QVERIFY(presentationSource.open(QIODevice::ReadOnly));
    const QByteArray voiceContents = voiceSource.readAll();
    const QByteArray presentationContents = presentationSource.readAll();

    QVERIFY(voiceContents.contains("VoiceResultPresentationController"));
    QVERIFY(!voiceContents.contains("streamContextToPopup("));
    QVERIFY(!voiceContents.contains("rerunLastIntoPopup("));
    QVERIFY(!voiceContents.contains("rerunLastIntoScreenshotWindow("));
    QVERIFY(!voiceContents.contains("showScreenshotResultWindow("));
    QVERIFY(!voiceContents.contains("finishFinalOutput("));
    QVERIFY(!voiceContents.contains("showResultChoicePopup("));

    QVERIFY(presentationContents.contains("ResultChoicePopup"));
    QVERIFY(presentationContents.contains("ScreenshotResultWindow"));
    QVERIFY(presentationContents.contains("VoiceResultOutputDispatcher"));
    QVERIFY(presentationContents.contains("VoiceResultStreamExecutor"));
    QVERIFY(presentationContents.contains("VoiceResultRerunExecutor"));
}

QTEST_APPLESS_MAIN(VoiceResultPresentationControllerTests)

#include "voice_result_presentation_controller_tests.moc"
