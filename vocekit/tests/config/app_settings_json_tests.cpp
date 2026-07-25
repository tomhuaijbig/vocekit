#include <QtTest>

#include "../../src/config/app_settings_data.h"
#include "../../src/config/app_settings_json.h"
#include "../../src/config/app_settings_store.h"

#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>

namespace {

QJsonObject readFixture(const QString &relativePath)
{
    QFile file(QFINDTESTDATA(relativePath.toUtf8().constData()));
    if (!file.open(QIODevice::ReadOnly)) {
        return QJsonObject();
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    return document.object();
}

} // namespace

class AppSettingsJsonTests : public QObject
{
    Q_OBJECT

private slots:
    void loadsCurrentSettingsShape()
    {
        QStringList warnings;
        const AppSettingsData data = appSettingsDataFromJson(
            readFixture("../fixtures/settings/current_settings.json"),
            &warnings
        );

        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
        QCOMPARE(data.functions.size(), 4);
        QCOMPARE(data.function(QStringLiteral("dictate")).input.useVoice, true);
        QCOMPARE(
            data.function(QStringLiteral("translate")).input.useSelection,
            true
        );
        QCOMPARE(
            data.function(QStringLiteral("custom_1")).builtIn,
            false
        );
        QCOMPARE(
            data.function(QStringLiteral("custom_1")).name,
            QStringLiteral("自定义功能 1")
        );
        QCOMPARE(
            data.function(QStringLiteral("custom_1")).prompt,
            QStringLiteral("请根据输入内容完成任务。")
        );
        QCOMPARE(
            data.applicationHotkeys.value(QStringLiteral("hub")),
            QStringLiteral("Ctrl+Alt+S")
        );
    }

    void writesCurrentSettingsShape()
    {
        const AppSettingsData original = appSettingsDataFromJson(
            readFixture("../fixtures/settings/current_settings.json")
        );

        const QJsonObject written = appSettingsDataToJson(original);
        QVERIFY(written.contains(QStringLiteral("models")));
        QVERIFY(written.contains(QStringLiteral("inputModes")));
        QVERIFY(written.contains(QStringLiteral("displayTimes")));
        QVERIFY(written.contains(QStringLiteral("recordingModes")));
        QVERIFY(written.contains(QStringLiteral("customFunctions")));
        QCOMPARE(
            written.value(QStringLiteral("hotkeys"))
                .toObject()
                .value(QStringLiteral("dictate"))
                .toString(),
            QStringLiteral("Alt+X")
        );
    }

    void roundTripsFunctionAndWindowSettings()
    {
        AppSettingsData original = appSettingsDataFromJson(
            readFixture("../fixtures/settings/current_settings.json")
        );
        original.windows.hasFloatingBarPosition = true;
        original.windows.floatingBarPosition = QPoint(120, 240);
        original.windows.hasResultPopupGeometry = true;
        original.windows.resultPopupGeometry = QRect(10, 20, 780, 540);

        const AppSettingsData restored = appSettingsDataFromJson(
            appSettingsDataToJson(original)
        );

        QCOMPARE(restored.functions.size(), original.functions.size());
        QCOMPARE(
            restored.function(QStringLiteral("ask")).network.model,
            QStringLiteral("systemProxy")
        );
        QCOMPARE(
            restored.function(QStringLiteral("dictate"))
                .recording
                .longRecordingEnabled,
            true
        );
        QCOMPARE(
            restored.function(QStringLiteral("translate"))
                .output
                .resultTemplate,
            QStringLiteral("compare")
        );
        QVERIFY(restored.windows.hasFloatingBarPosition);
        QCOMPARE(restored.windows.floatingBarPosition, QPoint(120, 240));
        QVERIFY(restored.windows.hasResultPopupGeometry);
        QCOMPARE(
            restored.windows.resultPopupGeometry,
            QRect(10, 20, 780, 540)
        );
    }

    void keepsUnknownRootValuesDuringMigration()
    {
        QJsonObject root =
            readFixture("../fixtures/settings/current_settings.json");
        root.insert(
            QStringLiteral("futureSetting"),
            QJsonObject{{QStringLiteral("enabled"), true}}
        );

        const AppSettingsData data = appSettingsDataFromJson(root);
        const QJsonObject written = appSettingsDataToJson(data);

        QCOMPARE(
            written.value(QStringLiteral("futureSetting"))
                .toObject()
                .value(QStringLiteral("enabled"))
                .toBool(),
            true
        );
    }

    void preservesExistingTimingAndVocabularyRanges()
    {
        QJsonObject root =
            readFixture("../fixtures/settings/current_settings.json");
        QJsonObject displayTimes =
            root.value(QStringLiteral("displayTimes")).toObject();
        QJsonObject dictate =
            displayTimes.value(QStringLiteral("dictate")).toObject();
        dictate.insert(QStringLiteral("resultPopupSeconds"), 500);
        dictate.insert(QStringLiteral("countdownSeconds"), 45);
        displayTimes.insert(QStringLiteral("dictate"), dictate);
        root.insert(QStringLiteral("displayTimes"), displayTimes);
        root.insert(QStringLiteral("vocabularyPromptEntryLimit"), 0);

        const AppSettingsData data = appSettingsDataFromJson(root);

        QCOMPARE(
            data.function(QStringLiteral("dictate"))
                .output
                .resultPopupSeconds,
            500
        );
        QCOMPARE(
            data.function(QStringLiteral("dictate"))
                .recording
                .countdownSeconds,
            45
        );
        QCOMPARE(data.vocabularyPromptEntryLimit, 0);
    }

    void preservesExistingOcrEngineIds()
    {
        const QStringList ids = QStringList()
            << QStringLiteral("rapid")
            << QStringLiteral("windows")
            << QStringLiteral("customCloud")
            << QStringLiteral("vision");
        for (const QString &id : ids) {
            QJsonObject root =
                readFixture("../fixtures/settings/current_settings.json");
            root.insert(QStringLiteral("ocrEngine"), id);
            const AppSettingsData data = appSettingsDataFromJson(root);
            QCOMPARE(data.ocrEngine, id);
        }
    }

    void loadsUpdatesAndSavesASettingsFile()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const QString path =
            temporaryDirectory.filePath(QStringLiteral("settings.json"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QJsonDocument(
            readFixture("../fixtures/settings/current_settings.json")
        ).toJson(QJsonDocument::Indented));
        file.close();

        AppSettingsStore settings(path);
        OperationError error;
        QVERIFY2(settings.load(&error), qPrintable(error.message));

        FunctionSettings dictate =
            settings.function(QStringLiteral("dictate"));
        dictate.modelId = QStringLiteral("deepseek-v4-pro");
        dictate.output.floatingBarSeconds = 9;
        QVERIFY(settings.updateFunction(dictate));

        AppSettingsData globals = settings.snapshot();
        globals.historyInitialLoadCount = 36;
        globals.windows.hasFloatingBarPosition = true;
        globals.windows.floatingBarPosition = QPoint(30, 40);
        settings.updateGlobal(globals);
        QVERIFY2(settings.save(&error), qPrintable(error.message));

        QFile savedFile(path);
        QVERIFY(savedFile.open(QIODevice::ReadOnly));
        QJsonParseError parseError;
        const QJsonDocument savedDocument = QJsonDocument::fromJson(
            savedFile.readAll(),
            &parseError
        );
        QCOMPARE(parseError.error, QJsonParseError::NoError);
        QVERIFY(savedDocument.isObject());

        AppSettingsStore restored(path);
        QVERIFY2(restored.load(&error), qPrintable(error.message));
        QCOMPARE(
            restored.function(QStringLiteral("dictate")).modelId,
            QStringLiteral("deepseek-v4-pro")
        );
        QCOMPARE(
            restored.function(QStringLiteral("dictate"))
                .output
                .floatingBarSeconds,
            9
        );
        QCOMPARE(restored.snapshot().historyInitialLoadCount, 36);
        QCOMPARE(
            restored.snapshot().windows.floatingBarPosition,
            QPoint(30, 40)
        );
        QCOMPARE(restored.snapshot().functions.size(), 4);
    }

    void createsDefaultSettingsWhenTheFileCannotBeLoaded()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const QString path =
            temporaryDirectory.filePath(QStringLiteral("settings.json"));

        AppSettingsStore settings(path);
        OperationError error;
        QVERIFY2(settings.loadOrCreateDefaults(&error), qPrintable(error.message));
        QVERIFY(QFileInfo::exists(path));
        QCOMPARE(
            settings.function(QStringLiteral("dictate")).id,
            QStringLiteral("dictate")
        );
        QCOMPARE(
            settings.function(QStringLiteral("translate")).id,
            QStringLiteral("translate")
        );
        QCOMPARE(
            settings.function(QStringLiteral("ask")).id,
            QStringLiteral("ask")
        );
    }

    void replacesFullSnapshotAndSaves()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const QString path =
            temporaryDirectory.filePath(QStringLiteral("settings.json"));

        AppSettingsData data = appSettingsDataFromJson(
            readFixture("../fixtures/settings/current_settings.json")
        );
        data.trayResident = false;
        data.speechProvider = QStringLiteral("xfyun");
        const int dictateIndex = data.functionIndex(QStringLiteral("dictate"));
        QVERIFY(dictateIndex >= 0);
        data.functions[dictateIndex].shortcut = QStringLiteral("Alt+D");

        AppSettingsStore settings(path);
        settings.replaceSnapshot(data);
        OperationError error;
        QVERIFY2(settings.save(&error), qPrintable(error.message));

        AppSettingsStore restored(path);
        QVERIFY2(restored.load(&error), qPrintable(error.message));
        QCOMPARE(restored.snapshot().trayResident, false);
        QCOMPARE(restored.snapshot().speechProvider, QStringLiteral("xfyun"));
        QCOMPARE(
            restored.function(QStringLiteral("dictate")).shortcut,
            QStringLiteral("Alt+D")
        );
        QCOMPARE(restored.snapshot().functions.size(), data.functions.size());
    }

    void rollsBackSnapshotWhenReplaceAndSaveFails()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString blockingPath =
            temporaryDirectory.filePath(QStringLiteral("not-a-directory"));
        QFile blockingFile(blockingPath);
        QVERIFY(blockingFile.open(QIODevice::WriteOnly));
        blockingFile.write("block");
        blockingFile.close();

        AppSettingsData original = appSettingsDataFromJson(
            readFixture("../fixtures/settings/current_settings.json")
        );
        original.speechProvider = QStringLiteral("baidu");

        AppSettingsData updated = original;
        updated.speechProvider = QStringLiteral("xfyun");

        AppSettingsStore settings(
            QDir(blockingPath).filePath(QStringLiteral("settings.json"))
        );
        settings.replaceSnapshot(original);

        OperationError error;
        QVERIFY(!settings.replaceAndSave(updated, &error));
        QVERIFY(!error.code.isEmpty());
        QCOMPARE(
            settings.snapshot().speechProvider,
            QStringLiteral("baidu")
        );
    }
};

QTEST_MAIN(AppSettingsJsonTests)
#include "app_settings_json_tests.moc"
