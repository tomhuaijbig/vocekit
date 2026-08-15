#include <QtTest>

#include "../../src/config/app_settings_data.h"
#include "../../src/config/app_settings_defaults.h"
#include "../../src/config/app_settings_json.h"
#include "../../src/config/app_settings_store.h"
#include "../../src/domain/selection_context_actions.h"

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
    void legacyOrderMigratesWithoutChangingOrder()
    {
        QJsonObject selection;
        selection.insert(
            QStringLiteral("actionOrder"),
            QJsonArray()
                << QStringLiteral("copy")
                << QStringLiteral("translate")
        );
        QJsonObject root;
        root.insert(QStringLiteral("selectionContextToolbar"), selection);

        const AppSettingsData restored = appSettingsDataFromJson(root);

        QCOMPARE(
            restored.selectionContext.actionOrder.first(),
            selectionContextActionCopy()
        );
        QCOMPARE(restored.selectionContext.actionCustomizations.size(), 5);
    }

    void customizationRoundTripPreservesUnicodeAndUnknownJson()
    {
        AppSettingsData data;
        SelectionContextActionCustomization explain = data.selectionContext
            .actionCustomizations.value(selectionContextActionExplain());
        explain.displayName = QString::fromUtf8("给我讲明白");
        explain.modelId = QStringLiteral("openai:gpt-5.6-sol");
        explain.promptOverride = QString::fromUtf8("用三个层次解释");
        data.selectionContext.actionCustomizations.insert(
            selectionContextActionExplain(),
            explain
        );
        data.retainedRootValues = QJsonObject{
            {QStringLiteral("selectionContextToolbar"), QJsonObject{
                {QStringLiteral("futureField"), 7},
                {QStringLiteral("actionCustomizations"), QJsonObject{
                    {selectionContextActionExplain(), QJsonObject{
                        {QStringLiteral("futurePerAction"), 9}
                    }},
                    {QStringLiteral("future-action"), QJsonObject{
                        {QStringLiteral("futureKey"), true}
                    }}
                }}
            }}
        };

        const QJsonObject json = appSettingsDataToJson(data);
        const AppSettingsData restored = appSettingsDataFromJson(json);
        const QJsonObject selection = json
            .value(QStringLiteral("selectionContextToolbar")).toObject();
        const QJsonObject actions = selection
            .value(QStringLiteral("actionCustomizations")).toObject();

        QCOMPARE(
            restored.selectionContext.actionCustomizations
                .value(selectionContextActionExplain()).displayName,
            QString::fromUtf8("给我讲明白")
        );
        QCOMPARE(
            restored.selectionContext.actionCustomizations
                .value(selectionContextActionExplain()).modelId,
            QStringLiteral("openai:gpt-5.6-sol")
        );
        QCOMPARE(
            restored.selectionContext.actionCustomizations
                .value(selectionContextActionExplain()).promptOverride,
            QString::fromUtf8("用三个层次解释")
        );
        QCOMPARE(selection.value(QStringLiteral("futureField")).toInt(), 7);
        QCOMPARE(
            actions.value(selectionContextActionExplain()).toObject()
                .value(QStringLiteral("futurePerAction")).toInt(),
            9
        );
        QVERIFY(
            actions.value(QStringLiteral("future-action")).toObject()
                .value(QStringLiteral("futureKey")).toBool()
        );
    }

    void selectionContextSettingsRoundTripAndNormalize()
    {
        AppSettingsData data;
        data.selectionContext.enabled = true;
        data.selectionContext.networkConsentAcknowledged = true;
        data.selectionContext.minimumTextLength = 0;
        data.selectionContext.pauseMinutes = 2000;
        data.selectionContext.blockedApplications =
            QStringList() << QStringLiteral(" WeChat.exe ")
                          << QStringLiteral("wechat.exe")
                          << QStringLiteral("C:/Apps/CHROME.EXE");
        data.selectionContext.actionOrder =
            QStringList() << QStringLiteral("unknown")
                          << selectionContextActionCopy();

        const QJsonObject written = appSettingsDataToJson(data);
        const AppSettingsData restored = appSettingsDataFromJson(written);

        QVERIFY(restored.selectionContext.enabled);
        QVERIFY(restored.selectionContext.networkConsentAcknowledged);
        QCOMPARE(restored.selectionContext.minimumTextLength, 1);
        QCOMPARE(restored.selectionContext.pauseMinutes, 1440);
        QCOMPARE(
            restored.selectionContext.blockedApplications,
            QStringList() << QStringLiteral("wechat.exe")
                          << QStringLiteral("chrome.exe")
        );
        QCOMPARE(
            restored.selectionContext.actionOrder.first(),
            selectionContextActionCopy()
        );
        QCOMPARE(restored.selectionContext.actionOrder.size(), 5);
    }

    void selectionContextUnknownNestedFieldsSurviveKnownFieldUpdates()
    {
        const QJsonObject futureObject{
            {QStringLiteral("mode"), QStringLiteral("future")},
            {QStringLiteral("enabled"), true}
        };
        const QJsonArray futureArray{
            QStringLiteral("one"),
            QJsonObject{{QStringLiteral("two"), 2}}
        };
        QJsonObject nested;
        nested.insert(QStringLiteral("enabled"), false);
        nested.insert(QStringLiteral("futureObject"), futureObject);
        nested.insert(QStringLiteral("futureArray"), futureArray);
        QJsonObject root;
        root.insert(QStringLiteral("selectionContextToolbar"), nested);

        AppSettingsData data = appSettingsDataFromJson(root);
        data.selectionContext.enabled = true;
        const QJsonObject written = appSettingsDataToJson(data);
        const QJsonObject restored = written
            .value(QStringLiteral("selectionContextToolbar"))
            .toObject();

        QVERIFY(restored.value(QStringLiteral("enabled")).toBool());
        QCOMPARE(restored.value(QStringLiteral("futureObject")),
                 QJsonValue(futureObject));
        QCOMPARE(restored.value(QStringLiteral("futureArray")),
                 QJsonValue(futureArray));
    }

    void missingStreamingSpeechSettingDefaultsToEnabled()
    {
        const AppSettingsData restored = appSettingsDataFromJson(
            QJsonObject()
        );

        QVERIFY(restored.streamingSpeechRecognitionEnabled);
    }

    void streamingSpeechSettingRoundTripsFalse()
    {
        AppSettingsData original;
        original.streamingSpeechRecognitionEnabled = false;

        const AppSettingsData restored = appSettingsDataFromJson(
            appSettingsDataToJson(original)
        );

        QVERIFY(!restored.streamingSpeechRecognitionEnabled);
    }

    void missingWindowsSpeechLanguageUsesFollowWindows()
    {
        const AppSettingsData restored = appSettingsDataFromJson(
            QJsonObject()
        );

        QCOMPARE(
            restored.windowsSpeechLanguage,
            windowsSpeechLanguageFollowWindows()
        );
    }

    void windowsSpeechSettingsRoundTrip()
    {
        AppSettingsData original;
        original.speechProvider = speechProviderWindowsLocal();
        original.windowsSpeechLanguage = QStringLiteral(" EN-us ");

        const QJsonObject written = appSettingsDataToJson(original);
        const AppSettingsData restored = appSettingsDataFromJson(written);

        QCOMPARE(
            written.value(QStringLiteral("speechProvider")).toString(),
            speechProviderWindowsLocal()
        );
        QCOMPARE(
            written.value(QStringLiteral("windowsSpeechLanguage")).toString(),
            windowsSpeechLanguageEnglish()
        );
        QCOMPARE(restored.speechProvider, speechProviderWindowsLocal());
        QCOMPARE(
            restored.windowsSpeechLanguage,
            windowsSpeechLanguageEnglish()
        );
    }

    void invalidWindowsSpeechLanguageNormalizesToFollowWindows()
    {
        QJsonObject root;
        root.insert(
            QStringLiteral("windowsSpeechLanguage"),
            QStringLiteral("fr-FR")
        );

        const AppSettingsData restored = appSettingsDataFromJson(root);

        QCOMPARE(
            restored.windowsSpeechLanguage,
            windowsSpeechLanguageFollowWindows()
        );
        QCOMPARE(
            appSettingsDataToJson(restored)
                .value(QStringLiteral("windowsSpeechLanguage"))
                .toString(),
            windowsSpeechLanguageFollowWindows()
        );
    }

    void unknownSpeechProviderStillFallsBackToBaidu()
    {
        QJsonObject root;
        root.insert(
            QStringLiteral("speechProvider"),
            QStringLiteral("future-provider")
        );

        const AppSettingsData restored = appSettingsDataFromJson(root);

        QCOMPARE(restored.speechProvider, speechProviderBaidu());
    }

    void missingFloatingPreferencesUseMigrationSafeDefaults()
    {
        const AppSettingsData restored = appSettingsDataFromJson(
            QJsonObject()
        );

        QCOMPARE(
            restored.floatingBarStyle,
            floatingBarStyleStatusPill()
        );
        QVERIFY(restored.writeFailurePopupFallbackEnabled);
        QCOMPARE(
            restored.function(QStringLiteral("dictate"))
                .output.floatingBarStyleOverride,
            floatingBarStyleInherit()
        );
    }

    void floatingPreferencesRoundTripWithoutChangingOtherOutputFields()
    {
        AppSettingsData original = appSettingsDataFromJson(
            readFixture("../fixtures/settings/current_settings.json")
        );
        original.floatingBarStyle =
            floatingBarStyleLiveTranscriptCard();
        original.writeFailurePopupFallbackEnabled = false;
        const int customIndex =
            original.functionIndex(QStringLiteral("custom_1"));
        QVERIFY(customIndex >= 0);
        const QString oldOutputMode =
            original.functions.at(customIndex).output.outputMode;
        const QJsonObject oldFlow = appSettingsDataToJson(original)
            .value(QStringLiteral("functionFlows"))
            .toObject()
            .value(QStringLiteral("custom_1"))
            .toObject();
        original.functions[customIndex]
            .output.floatingBarStyleOverride =
                floatingBarStyleStatusPill();

        const QJsonObject written = appSettingsDataToJson(original);
        const AppSettingsData restored = appSettingsDataFromJson(written);

        QCOMPARE(
            written.value(QStringLiteral("floatingBarStyle")).toString(),
            floatingBarStyleLiveTranscriptCard()
        );
        QCOMPARE(
            written.value(
                QStringLiteral("writeFailurePopupFallbackEnabled")
            ).toBool(true),
            false
        );
        QCOMPARE(
            restored.floatingBarStyle,
            floatingBarStyleLiveTranscriptCard()
        );
        QVERIFY(!restored.writeFailurePopupFallbackEnabled);
        QCOMPARE(
            restored.function(QStringLiteral("custom_1"))
                .output.floatingBarStyleOverride,
            floatingBarStyleStatusPill()
        );
        QCOMPARE(
            restored.function(QStringLiteral("custom_1"))
                .output.outputMode,
            oldOutputMode
        );
        QCOMPARE(
            written.value(QStringLiteral("functionFlows"))
                .toObject()
                .value(QStringLiteral("custom_1"))
                .toObject(),
            oldFlow
        );
    }

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
        QVERIFY(written.contains(QStringLiteral("outputOrders")));
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
        const int translateIndex =
            original.functionIndex(QStringLiteral("translate"));
        QVERIFY(translateIndex >= 0);
        original.functions[translateIndex].input.order =
            QStringList()
                << QStringLiteral("selection")
                << QStringLiteral("screenshot")
                << QStringLiteral("voice");
        original.functions[translateIndex].output.order =
            QStringList()
                << QStringLiteral("resultPopup")
                << QStringLiteral("ai")
                << QStringLiteral("screenshotPanel")
                << QStringLiteral("autoWrite");

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
        QCOMPARE(
            restored.function(QStringLiteral("translate")).input.order,
            QStringList()
                << QStringLiteral("selection")
                << QStringLiteral("screenshot")
                << QStringLiteral("voice")
        );
        QCOMPARE(
            restored.function(QStringLiteral("translate")).output.order,
            QStringList()
                << QStringLiteral("resultPopup")
                << QStringLiteral("ai")
                << QStringLiteral("screenshotPanel")
                << QStringLiteral("autoWrite")
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

    void rollsBackSnapshotWhenReplaceNonFlowSettingsAndSaveFails()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());

        const QString blockingPath =
            temporaryDirectory.filePath(QStringLiteral("not-a-directory"));
        QFile blockingFile(blockingPath);
        QVERIFY(blockingFile.open(QIODevice::WriteOnly));
        blockingFile.write("block");
        blockingFile.close();

        AppSettingsData current = appSettingsDataFromJson(
            readFixture("../fixtures/settings/current_settings.json")
        );
        const int customIndex =
            current.functionIndex(QStringLiteral("custom_1"));
        QVERIFY(customIndex >= 0);
        current.speechProvider = QStringLiteral("baidu");
        current.functions[customIndex].shortcut =
            QStringLiteral("Ctrl+Alt+M");
        current.functions[customIndex].flow.draft.revision = 11;
        current.functions[customIndex].flow.draft.graphHash =
            QString(64, QLatin1Char('c'));
        current.functions[customIndex].executionMode =
            FunctionExecutionMode::Canvas;
        current.functions[customIndex] =
            normalizeFunctionSettings(current.functions.at(customIndex));

        AppSettingsData edited = current;
        edited.speechProvider = QStringLiteral("xfyun");
        edited.functions[customIndex].shortcut = QStringLiteral("Alt+M");
        edited.functions[customIndex].flow.draft.revision = 10;
        edited.functions[customIndex].flow.draft.graphHash =
            QString(64, QLatin1Char('d'));
        edited.functions[customIndex].executionMode =
            FunctionExecutionMode::Classic;
        edited.functions[customIndex] =
            normalizeFunctionSettings(edited.functions.at(customIndex));

        AppSettingsStore settings(
            QDir(blockingPath).filePath(QStringLiteral("settings.json"))
        );
        settings.replaceSnapshot(current);

        OperationError error;
        QVERIFY(
            !settings.replaceNonFlowSettingsAndSave(edited, &error)
        );
        QVERIFY(!error.code.isEmpty());
        QCOMPARE(
            settings.snapshot().speechProvider,
            QStringLiteral("baidu")
        );
        const FunctionSettings rolledBackFunction =
            settings.function(QStringLiteral("custom_1"));
        QCOMPARE(
            rolledBackFunction.shortcut,
            QStringLiteral("Ctrl+Alt+M")
        );
        QVERIFY(
            rolledBackFunction.executionMode
                == FunctionExecutionMode::Canvas
        );
        QVERIFY(rolledBackFunction.flow.enabled);
        QCOMPARE(rolledBackFunction.flow.draft.revision, 11);
        QCOMPARE(
            rolledBackFunction.flow.draft.graphHash,
            QString(64, QLatin1Char('c'))
        );
    }

    void mergesOnlyNonFlowSettingsFromAnOlderSnapshot()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const QString path =
            temporaryDirectory.filePath(QStringLiteral("settings.json"));

        AppSettingsData current = appSettingsDataFromJson(
            readFixture("../fixtures/settings/current_settings.json")
        );
        const int customIndex =
            current.functionIndex(QStringLiteral("custom_1"));
        QVERIFY(customIndex >= 0);
        current.functions[customIndex].flow.draft.revision = 4;
        const QString currentDraftHash = functionFlowGraphHash(
            current.functions.at(customIndex).flow.draft.graph
        );
        current.functions[customIndex].flow.draft.graphHash =
            currentDraftHash;
        current.functions[customIndex].executionMode =
            FunctionExecutionMode::Canvas;
        current.functions[customIndex] =
            normalizeFunctionSettings(current.functions.at(customIndex));
        QJsonObject orphan;
        orphan.insert(QStringLiteral("future"), 9);
        current.retainedOrphanFunctionFlows.insert(
            QStringLiteral("removed"),
            orphan
        );

        AppSettingsData stale = current;
        stale.functions[customIndex].flow.draft.revision = 3;
        stale.functions[customIndex].flow.draft.graphHash =
            QString(64, QLatin1Char('b'));
        stale.functions[customIndex].executionMode =
            FunctionExecutionMode::Classic;
        stale.functions[customIndex] =
            normalizeFunctionSettings(stale.functions.at(customIndex));
        const QString editedShortcut = QStringLiteral("Ctrl+Shift+9");
        stale.functions[customIndex].shortcut = editedShortcut;
        stale.functions[customIndex].builtIn =
            !current.functions.at(customIndex).builtIn;
        stale.trayResident = false;
        stale.retainedOrphanFunctionFlows = QJsonObject();

        AppSettingsStore store(path);
        store.replaceSnapshot(current);
        OperationError error;
        QVERIFY(store.replaceNonFlowSettingsAndSave(stale, &error));

        QCOMPARE(store.snapshot().trayResident, false);
        const FunctionSettings savedFunction =
            store.function(QStringLiteral("custom_1"));
        QCOMPARE(savedFunction.shortcut, editedShortcut);
        QCOMPARE(
            savedFunction.builtIn,
            current.functions.at(customIndex).builtIn
        );
        QCOMPARE(
            functionExecutionModeId(savedFunction.executionMode),
            QStringLiteral("canvas")
        );
        const FunctionFlowState savedFlow = savedFunction.flow;
        QCOMPARE(savedFlow.draft.revision, 4);
        QCOMPARE(
            savedFlow.draft.graphHash,
            currentDraftHash
        );
        QVERIFY(savedFlow.enabled);
        QCOMPARE(
            store.snapshot().retainedOrphanFunctionFlows
                .value(QStringLiteral("removed")).toObject(),
            orphan
        );

        AppSettingsStore restored(path);
        QVERIFY2(restored.load(&error), qPrintable(error.message));
        QCOMPARE(restored.snapshot().trayResident, false);
        const FunctionSettings restoredFunction =
            restored.function(QStringLiteral("custom_1"));
        QCOMPARE(restoredFunction.shortcut, editedShortcut);
        QCOMPARE(
            restoredFunction.builtIn,
            current.functions.at(customIndex).builtIn
        );
        QVERIFY(
            restoredFunction.executionMode
                == FunctionExecutionMode::Canvas
        );
        QVERIFY(restoredFunction.flow.enabled);
        QCOMPARE(restoredFunction.flow.draft.revision, 4);
        QCOMPARE(
            restoredFunction.flow.draft.graphHash,
            currentDraftHash
        );
        QCOMPARE(
            restored.snapshot().retainedOrphanFunctionFlows
                .value(QStringLiteral("removed")).toObject(),
            orphan
        );
    }

    void rejectsStaleFunctionSetsWithoutSaving()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const QString path =
            temporaryDirectory.filePath(QStringLiteral("settings.json"));
        AppSettingsData current = appSettingsDataFromJson(
            readFixture("../fixtures/settings/current_settings.json")
        );
        AppSettingsData stale = current;
        stale.functions.removeLast();
        stale.trayResident = false;

        AppSettingsStore store(path);
        store.replaceSnapshot(current);
        OperationError error;
        QVERIFY(!store.replaceNonFlowSettingsAndSave(stale, &error));
        QCOMPARE(
            error.code,
            QStringLiteral("settings_function_set_stale")
        );
        QCOMPARE(store.snapshot().functions.size(), current.functions.size());
        QCOMPARE(store.snapshot().trayResident, current.trayResident);
        QVERIFY(!QFileInfo::exists(path));
    }
};

QTEST_MAIN(AppSettingsJsonTests)
#include "app_settings_json_tests.moc"
