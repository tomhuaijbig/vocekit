#include <QtTest>

#include "../../src/controllers/voice_recording_workflow_controller.h"

#include <QFile>
#include <type_traits>

class VoiceRecordingWorkflowControllerTests : public QObject
{
    Q_OBJECT

private slots:
    void exposesIndependentWorkflowInterface();
    void voiceControllerDoesNotOwnRecordingImplementation();
};

void VoiceRecordingWorkflowControllerTests::exposesIndependentWorkflowInterface()
{
    QVERIFY((std::is_default_constructible<
        VoiceRecordingWorkflowAccess
    >::value));
    QVERIFY((std::is_constructible<
        VoiceRecordingWorkflowController,
        const VoiceRecordingWorkflowAccess &,
        FloatingBar *,
        VoiceRunSession *,
        QObject *
    >::value));
}

void VoiceRecordingWorkflowControllerTests::
voiceControllerDoesNotOwnRecordingImplementation()
{
    const QString voicePath = QFINDTESTDATA(
        "../../src/controllers/voice_controller.cpp"
    );
    const QString workflowPath = QFINDTESTDATA(
        "../../src/controllers/voice_recording_workflow_controller.cpp"
    );
    QVERIFY2(!voicePath.isEmpty(), "找不到 VoiceController 源文件");
    QVERIFY2(!workflowPath.isEmpty(), "找不到录音工作流控制器源文件");

    QFile voiceSource(voicePath);
    QFile workflowSource(workflowPath);
    QVERIFY(voiceSource.open(QIODevice::ReadOnly));
    QVERIFY(workflowSource.open(QIODevice::ReadOnly));
    const QByteArray voiceContents = voiceSource.readAll();
    const QByteArray workflowContents = workflowSource.readAll();

    QVERIFY(voiceContents.contains("VoiceRecordingWorkflowController"));
    QVERIFY(!voiceContents.contains("VoiceAudioRecorderAdapter"));
    QVERIFY(!voiceContents.contains("VoiceRecordingCapture"));
    QVERIFY(!voiceContents.contains("VoiceRecordingLifecycle"));
    QVERIFY(!voiceContents.contains("VoiceRecordingCoordinator"));
    QVERIFY(!voiceContents.contains(
        "VoiceLongRecordingRecognitionCoordinator"
    ));
    QVERIFY(!voiceContents.contains("beginRecordingWithPreparation("));
    QVERIFY(!voiceContents.contains("rotateLongRecordingSegment("));
    QVERIFY(!voiceContents.contains("stopLongRecordingAndProcess("));
    QVERIFY(!voiceContents.contains("finishLongRecordingRecognition("));
    QVERIFY(!voiceContents.contains("stopAndProcess("));

    QVERIFY(workflowContents.contains("VoiceAudioRecorderAdapter"));
    QVERIFY(workflowContents.contains("VoiceRecordingCapture"));
    QVERIFY(workflowContents.contains("VoiceRecordingLifecycle"));
    QVERIFY(workflowContents.contains("VoiceRecordingCoordinator"));
    QVERIFY(workflowContents.contains(
        "VoiceLongRecordingRecognitionCoordinator"
    ));
    QVERIFY(workflowContents.contains("VoiceRecordingCompletionExecutor"));
    QVERIFY(workflowContents.contains(
        "VoiceLongRecordingCompletionExecutor"
    ));
}

QTEST_APPLESS_MAIN(VoiceRecordingWorkflowControllerTests)

#include "voice_recording_workflow_controller_tests.moc"
