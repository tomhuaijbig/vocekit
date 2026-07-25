#include <QtTest>

#include "../../src/controllers/vocabulary_quick_add_controller.h"

#include <QFile>

class VocabularyQuickAddControllerTests : public QObject
{
    Q_OBJECT

private slots:
    void manualModeOpensEditorWithoutCallingAi();
    void aiModeGeneratesAndSavesEntry();
    void askModeCanCancelWithoutSideEffects();
    void hotkeyReportsMissingSelectedText();
    void voiceControllerNoLongerOwnsVocabularyQuickAdd();
};

void VocabularyQuickAddControllerTests::
manualModeOpensEditorWithoutCallingAi()
{
    int aiCalls = 0;
    VocabularyEntry openedEntry;

    VocabularyQuickAddAccess access;
    access.requestSuggestion = [&](
        const VocabularySuggestionTaskRequest &,
        QString *
    ) {
        ++aiCalls;
        return VocabularySuggestion();
    };
    access.openEditor = [&](const VocabularyEntry &entry) {
        openedEntry = entry;
    };

    VocabularyQuickAddController controller(access);
    AppSettingsData settings;
    settings.vocabularyAddMode = QStringLiteral("manual");
    controller.updateConfiguration(settings);

    const VocabularyQuickAddOutcome outcome = controller.addText(
        QStringLiteral(" DeepSeek "),
        QStringLiteral("__global")
    );

    QCOMPARE(outcome, VocabularyQuickAddOutcome::EditorOpened);
    QCOMPARE(aiCalls, 0);
    QCOMPARE(openedEntry.source, QStringLiteral("DeepSeek"));
    QCOMPARE(openedEntry.target, QStringLiteral("DeepSeek"));
    QCOMPARE(openedEntry.scopeId, QStringLiteral("__global"));
}

void VocabularyQuickAddControllerTests::aiModeGeneratesAndSavesEntry()
{
    VocabularySuggestionTaskRequest capturedRequest;
    VocabularyEntry savedEntry;
    int refreshCount = 0;
    int informationCount = 0;

    VocabularyQuickAddAccess access;
    access.vocabularyPrompt = []() {
        return QStringLiteral("lexicon prompt");
    };
    access.requestSuggestion = [&](
        const VocabularySuggestionTaskRequest &request,
        QString *
    ) {
        capturedRequest = request;
        VocabularySuggestion suggestion;
        suggestion.valid = true;
        suggestion.entry.source = QStringLiteral("deepseep");
        suggestion.entry.target = QStringLiteral("DeepSeek");
        suggestion.entry.scopeId = QStringLiteral("__global");
        suggestion.entry.enabled = true;
        return suggestion;
    };
    access.appendEntry = [&](VocabularyEntry *entry, QString *) {
        savedEntry = *entry;
        return true;
    };
    access.notifyVocabularyChanged = [&]() {
        ++refreshCount;
    };
    access.showInformation = [&](const QString &, const QString &) {
        ++informationCount;
    };

    VocabularyQuickAddController controller(access);
    AppSettingsData settings;
    settings.vocabularyAddMode = QStringLiteral("ai");
    settings.useSystemProxy = true;
    controller.updateConfiguration(settings);

    const VocabularyQuickAddOutcome outcome = controller.addText(
        QStringLiteral("deepseep"),
        QStringLiteral("__global")
    );

    QCOMPARE(outcome, VocabularyQuickAddOutcome::Saved);
    QCOMPARE(
        capturedRequest.input.sourceText,
        QStringLiteral("deepseep")
    );
    QCOMPARE(capturedRequest.systemPrompt, QStringLiteral("lexicon prompt"));
    QVERIFY(capturedRequest.useSystemProxy);
    QCOMPARE(savedEntry.target, QStringLiteral("DeepSeek"));
    QCOMPARE(refreshCount, 1);
    QCOMPARE(informationCount, 1);
}

void VocabularyQuickAddControllerTests::
askModeCanCancelWithoutSideEffects()
{
    int aiCalls = 0;
    int editorCalls = 0;
    int appendCalls = 0;

    VocabularyQuickAddAccess access;
    access.askChoice = []() {
        return VocabularyQuickAddChoice::Cancel;
    };
    access.requestSuggestion = [&](
        const VocabularySuggestionTaskRequest &,
        QString *
    ) {
        ++aiCalls;
        return VocabularySuggestion();
    };
    access.openEditor = [&](const VocabularyEntry &) {
        ++editorCalls;
    };
    access.appendEntry = [&](VocabularyEntry *, QString *) {
        ++appendCalls;
        return true;
    };

    VocabularyQuickAddController controller(access);
    AppSettingsData settings;
    settings.vocabularyAddMode = QStringLiteral("ask");
    controller.updateConfiguration(settings);

    const VocabularyQuickAddOutcome outcome = controller.addText(
        QStringLiteral("DeepSeek"),
        QStringLiteral("__global")
    );

    QCOMPARE(outcome, VocabularyQuickAddOutcome::Cancelled);
    QCOMPARE(aiCalls, 0);
    QCOMPARE(editorCalls, 0);
    QCOMPARE(appendCalls, 0);
}

void VocabularyQuickAddControllerTests::
hotkeyReportsMissingSelectedText()
{
    QString informationTitle;
    int hideCount = 0;

    VocabularyQuickAddAccess access;
    access.readSelectedText = [](
        bool,
        SelectedTextNativeWindowHandle
    ) {
        return QString();
    };
    access.showInformation = [&](const QString &title, const QString &) {
        informationTitle = title;
    };
    access.hideStatusLater = [&]() {
        ++hideCount;
    };

    VocabularyQuickAddController controller(access);
    AppSettingsData settings;
    settings.vocabularyAddMode = QStringLiteral("manual");
    controller.updateConfiguration(settings);

    const VocabularyQuickAddOutcome outcome = controller.handleHotkey(
        nullptr,
        false
    );

    QCOMPARE(outcome, VocabularyQuickAddOutcome::MissingSelection);
    QCOMPARE(
        informationTitle,
        QString::fromUtf8("未识别到有选中文字")
    );
    QCOMPARE(hideCount, 1);
}

void VocabularyQuickAddControllerTests::
voiceControllerNoLongerOwnsVocabularyQuickAdd()
{
    const QString path = QFINDTESTDATA(
        "../../src/controllers/voice_controller.cpp"
    );
    QVERIFY2(!path.isEmpty(), "找不到 VoiceController 源文件");
    QFile source(path);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();

    QVERIFY(contents.contains("VocabularyQuickAddController"));
    QVERIFY(!contents.contains("handleVocabularyAddHotkey("));
    QVERIFY(!contents.contains("VocabularyAiChoice"));
    QVERIFY(!contents.contains("addTextToVocabularyBySetting("));

    const int notifyBegin = contents.indexOf(
        "vocabularyAccess.notifyVocabularyChanged = [this]()"
    );
    const int notifyEnd = contents.indexOf(
        "vocabularyAccess.prepareStatus",
        notifyBegin
    );
    QVERIFY(notifyBegin >= 0);
    QVERIFY(notifyEnd > notifyBegin);
    const QByteArray notifyBlock = contents.mid(
        notifyBegin,
        notifyEnd - notifyBegin
    );
    QVERIFY(
        notifyBlock.contains(
            "m_host->notifyVocabularyChangedForVoiceController();"
        )
    );
    QVERIFY(
        !notifyBlock.contains(
            "m_host->notifySettingsChangedForVoiceController();"
        )
    );
    QVERIFY(!contents.contains("refreshVocabularyForVoiceController"));
}

QTEST_APPLESS_MAIN(VocabularyQuickAddControllerTests)

#include "vocabulary_quick_add_controller_tests.moc"
