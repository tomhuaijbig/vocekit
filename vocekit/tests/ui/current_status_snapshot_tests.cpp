#include <QtTest>

#include "../../src/ui/current_status_snapshot.h"

class CurrentStatusSnapshotTests : public QObject
{
    Q_OBJECT

private slots:
    void copiesApplicationState();
    void countsOnlyVoiceRecordingModes();
    void detectsCustomHistoryLocation();
};

void CurrentStatusSnapshotTests::copiesApplicationState()
{
    AppSettingsData settings;
    settings.trayResident = false;
    settings.autoStartEnabled = true;
    settings.strongSelectionEnabled = true;
    settings.vocabularyEnabled = false;
    settings.vocabularyAddMode = QStringLiteral("direct");
    settings.dictatePolishEnabled = true;
    settings.speechProvider = QStringLiteral("xfyun");
    settings.useSystemProxy = true;
    settings.floatingBarEnabled = false;

    const CurrentStatusSnapshot snapshot = buildCurrentStatusSnapshot(settings);

    QVERIFY(!snapshot.trayResident);
    QVERIFY(snapshot.autoStartEnabled);
    QVERIFY(snapshot.strongSelectionEnabled);
    QVERIFY(!snapshot.vocabularyEnabled);
    QCOMPARE(snapshot.vocabularyAddMode, QStringLiteral("direct"));
    QVERIFY(snapshot.dictatePolishEnabled);
    QCOMPARE(snapshot.speechProvider, QStringLiteral("xfyun"));
    QVERIFY(snapshot.useSystemProxy);
    QVERIFY(!snapshot.floatingBarEnabled);
}

void CurrentStatusSnapshotTests::countsOnlyVoiceRecordingModes()
{
    AppSettingsData settings;

    FunctionSettings holdVoice;
    holdVoice.id = QStringLiteral("holdVoice");
    holdVoice.input.useVoice = true;
    holdVoice.recording.triggerMode = QStringLiteral("hold");
    settings.functions.append(holdVoice);

    FunctionSettings longVoice;
    longVoice.id = QStringLiteral("longVoice");
    longVoice.input.useVoice = true;
    longVoice.recording.longRecordingEnabled = true;
    settings.functions.append(longVoice);

    FunctionSettings inactive;
    inactive.id = QStringLiteral("inactive");
    inactive.input.useVoice = false;
    inactive.recording.triggerMode = QStringLiteral("hold");
    inactive.recording.longRecordingEnabled = true;
    settings.functions.append(inactive);

    const CurrentStatusSnapshot snapshot = buildCurrentStatusSnapshot(settings);

    QCOMPARE(snapshot.holdToTalkFunctionCount, 1);
    QCOMPARE(snapshot.longRecordingFunctionCount, 1);
}

void CurrentStatusSnapshotTests::detectsCustomHistoryLocation()
{
    AppSettingsData settings;
    QVERIFY(buildCurrentStatusSnapshot(settings).usesDefaultRecordDirectory);

    settings.recordDirectory = QStringLiteral("C:/records");
    QVERIFY(!buildCurrentStatusSnapshot(settings).usesDefaultRecordDirectory);
}

QTEST_APPLESS_MAIN(CurrentStatusSnapshotTests)

#include "current_status_snapshot_tests.moc"
