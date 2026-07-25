#include <QtTest>

#include "../../src/controllers/selected_text_workflow_controller.h"

#include <QFile>

class SelectedTextWorkflowControllerTests : public QObject
{
    Q_OBJECT

private slots:
    void returnsCorrectedSelectedText();
    void blocksTextOnlyFunctionWhenSelectionIsMissing();
    void allowsVoiceFunctionWhenSelectionIsMissing();
    void voiceControllerNoLongerOwnsSelectionReading();
};

void SelectedTextWorkflowControllerTests::returnsCorrectedSelectedText()
{
    bool readCalled = false;
    bool logged = false;
    QString statusTitle;

    SelectedTextWorkflowAccess access;
    access.readSelectedText = [&](
        const SelectedTextReadRequest &request,
        const VocabularyPreCorrectionCallback &preCorrect
    ) {
        readCalled = request.modeId == QStringLiteral("translate")
            && request.strongSelectionEnabled
            && !request.useVoice;
        SelectedTextReadResult result;
        result.text = preCorrect(
            QStringLiteral("deepseep"),
            request.modeId,
            request.sourceLabel,
            request.useVoice
        );
        return result;
    };
    access.preCorrect = [](
        const QString &text,
        const QString &,
        const QString &,
        bool
    ) {
        return text == QStringLiteral("deepseep")
            ? QStringLiteral("DeepSeek")
            : text;
    };
    access.setStatus = [&](const QString &title, const QString &) {
        statusTitle = title;
    };
    access.recordReadResult = [&](const SelectedTextReadResult &result) {
        logged = result.text.size() == 8;
    };

    SelectedTextWorkflowController controller(access);
    SelectedTextWorkflowRequest request;
    request.modeId = QStringLiteral("translate");
    request.strongSelectionEnabled = true;

    const SelectedTextWorkflowResult result = controller.execute(request);

    QVERIFY(readCalled);
    QVERIFY(logged);
    QCOMPARE(statusTitle, QString::fromUtf8("正在读取选中文字"));
    QCOMPARE(result.text, QStringLiteral("DeepSeek"));
    QVERIFY(!result.blocked);
}

void SelectedTextWorkflowControllerTests::
blocksTextOnlyFunctionWhenSelectionIsMissing()
{
    int hideCount = 0;
    QString informationTitle;
    QString finalStatusTitle;

    SelectedTextWorkflowAccess access;
    access.readSelectedText = [](
        const SelectedTextReadRequest &,
        const VocabularyPreCorrectionCallback &
    ) {
        return SelectedTextReadResult();
    };
    access.setStatus = [&](const QString &title, const QString &) {
        finalStatusTitle = title;
    };
    access.hideStatusLater = [&]() {
        ++hideCount;
    };
    access.showInformation = [&](const QString &title, const QString &) {
        informationTitle = title;
    };

    SelectedTextWorkflowController controller(access);
    SelectedTextWorkflowRequest request;
    request.modeId = QStringLiteral("translate");
    request.useVoice = false;

    const SelectedTextWorkflowResult result = controller.execute(request);

    QVERIFY(result.text.isEmpty());
    QVERIFY(result.blocked);
    QCOMPARE(hideCount, 1);
    QCOMPARE(
        finalStatusTitle,
        QString::fromUtf8("未识别到有选中文字")
    );
    QCOMPARE(informationTitle, finalStatusTitle);
}

void SelectedTextWorkflowControllerTests::
allowsVoiceFunctionWhenSelectionIsMissing()
{
    int hideCount = 0;
    int informationCount = 0;

    SelectedTextWorkflowAccess access;
    access.readSelectedText = [](
        const SelectedTextReadRequest &,
        const VocabularyPreCorrectionCallback &
    ) {
        return SelectedTextReadResult();
    };
    access.hideStatusLater = [&]() {
        ++hideCount;
    };
    access.showInformation = [&](const QString &, const QString &) {
        ++informationCount;
    };

    SelectedTextWorkflowController controller(access);
    SelectedTextWorkflowRequest request;
    request.modeId = QStringLiteral("ask");
    request.useVoice = true;

    const SelectedTextWorkflowResult result = controller.execute(request);

    QVERIFY(result.text.isEmpty());
    QVERIFY(!result.blocked);
    QCOMPARE(hideCount, 0);
    QCOMPARE(informationCount, 0);
}

void SelectedTextWorkflowControllerTests::
voiceControllerNoLongerOwnsSelectionReading()
{
    const QString path = QFINDTESTDATA(
        "../../src/controllers/voice_controller.cpp"
    );
    QVERIFY2(!path.isEmpty(), "找不到 VoiceController 源文件");
    QFile source(path);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();

    QVERIFY(contents.contains("SelectedTextWorkflowController"));
    QVERIFY(!contents.contains("VoiceInputCollector::readSelectedText("));
    QVERIFY(!contents.contains("SelectedTextReadRequest request;"));
}

QTEST_APPLESS_MAIN(SelectedTextWorkflowControllerTests)

#include "selected_text_workflow_controller_tests.moc"
