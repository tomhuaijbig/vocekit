#include <QtTest>

#include "../../src/ui/diagnostics_settings_snapshot.h"

class DiagnosticsSettingsSnapshotTests : public QObject
{
    Q_OBJECT

private slots:
    void copiesRuntimeSettings();
    void clampsRuntimeLimits();
};

void DiagnosticsSettingsSnapshotTests::copiesRuntimeSettings()
{
    AppSettingsData settings;
    settings.useSystemProxy = true;
    settings.ocrEngine = QStringLiteral("windows");
    settings.ocrTimeoutMs = 18000;
    settings.floatingBarEnabled = false;

    FunctionSettings dictate;
    dictate.id = QStringLiteral("dictate");
    dictate.output.floatingBarSeconds = 7;
    settings.functions.append(dictate);

    const DiagnosticsSettingsSnapshot snapshot =
        buildDiagnosticsSettingsSnapshot(settings);

    QVERIFY(snapshot.useSystemProxy);
    QCOMPARE(snapshot.ocrEngine, QStringLiteral("windows"));
    QCOMPARE(snapshot.ocrTimeoutMs, 18000);
    QVERIFY(!snapshot.floatingBarEnabled);
    QCOMPARE(snapshot.dictateFloatingBarSeconds, 7);
}

void DiagnosticsSettingsSnapshotTests::clampsRuntimeLimits()
{
    AppSettingsData settings;
    settings.ocrTimeoutMs = 1000;

    FunctionSettings dictate;
    dictate.id = QStringLiteral("dictate");
    dictate.output.floatingBarSeconds = 90;
    settings.functions.append(dictate);

    DiagnosticsSettingsSnapshot snapshot =
        buildDiagnosticsSettingsSnapshot(settings);
    QCOMPARE(snapshot.ocrTimeoutMs, 5000);
    QCOMPARE(snapshot.dictateFloatingBarSeconds, 60);

    settings.ocrTimeoutMs = 300000;
    dictate.output.floatingBarSeconds = -2;
    settings.functions[0] = dictate;
    snapshot = buildDiagnosticsSettingsSnapshot(settings);
    QCOMPARE(snapshot.ocrTimeoutMs, 120000);
    QCOMPARE(snapshot.dictateFloatingBarSeconds, 0);
}

QTEST_APPLESS_MAIN(DiagnosticsSettingsSnapshotTests)

#include "diagnostics_settings_snapshot_tests.moc"
