#include <QtTest>

#include "../../src/ocr/ocr_helper_process.h"
#include "../../src/ocr/ocr_manager.h"
#include "../../src/ocr/ocr_cloud_client.h"
#include "../../src/ocr/ocr_batch_queue.h"
#include "../../src/ocr/ocr_batch_text.h"
#include "../../src/ocr/screenshot_ocr_config.h"
#include "../../src/ocr/ocr_types.h"
#include "../../src/tasks/cancellation_token.h"
#include "../../src/config/app_settings_data.h"
#include "../../src/config/app_settings_defaults.h"
#include "../../src/config/secret_config.h"

#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <QDir>
#include <QImage>
#include <QEventLoop>
#include <QPainter>
#include <QTemporaryDir>
#include <QTimer>

#include <atomic>
#include <thread>

class OcrCoreTests : public QObject
{
    Q_OBJECT

private slots:
    void acceptsSupportedImage();
    void acceptsReadableImageWithUnusualExtension();
    void rejectsCorruptImage();
    void rejectsOversizedFile();
    void acceptsImageWiderThanOldLimit();
    void rejectsExcessivePixelCount();
    void batchQueueDeduplicatesAndNavigates();
    void batchQueueKeepsIndependentResults();
    void batchQueueFindsNextPendingImage();
    void parsesHelperSuccess();
    void parsesHelperTextBlocks();
    void parsesHelperFailure();
    void rejectsMalformedHelperResponse();
    void runsHelperSuccess();
    void rejectsNonExecutableHelperBeforeLaunch();
    void returnsHelperFailure();
    void rejectsMalformedHelperProcessOutput();
    void timesOutHelperProcess();
    void cancelsHelperProcess();
    void cancelsCloudOcrBeforeValidation();
    void cancelsOcrManagerDuringRecognition();
    void recognizesWithWindowsOcrHelper();
    void reportsMissingWindowsOcrLanguage();
    void recognizesWithRapidOcrHelper();
    void reportsMissingRapidOcrModels();
    void automaticFallsBackToWindows();
    void automaticKeepsBothFailureMessages();
    void automaticDoesNotFallbackForCancelledRequest();
    void extractsCloudOcrText();
    void rejectsMalformedCloudOcrResponse();
    void formatsOcrBatchStatusText();
    void formatsOcrBatchCompletionText();
    void mapsScreenshotOcrEngineFromSettings();
    void buildsScreenshotOcrManagerConfig();
};

void OcrCoreTests::acceptsSupportedImage()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString path = directory.filePath(QStringLiteral("sample.png"));
    QImage image(16, 16, QImage::Format_ARGB32);
    image.fill(Qt::white);
    QVERIFY(image.save(path));

    QString error;
    QVERIFY2(validateOcrImage(path, &error), qPrintable(error));
    QVERIFY(error.isEmpty());
}

void OcrCoreTests::acceptsReadableImageWithUnusualExtension()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString path = directory.filePath(QStringLiteral("sample.unusual"));
    QImage image(16, 16, QImage::Format_ARGB32);
    image.fill(Qt::white);
    QVERIFY(image.save(path, "PNG"));

    QString error;
    QVERIFY2(validateOcrImage(path, &error), qPrintable(error));
}

void OcrCoreTests::rejectsCorruptImage()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString path = directory.filePath(QStringLiteral("corrupt.png"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write("not-an-image"), qint64(12));
    file.close();

    QString error;
    QVERIFY(!validateOcrImage(path, &error));
    QVERIFY(error.contains(QStringLiteral("读取")));
}

void OcrCoreTests::rejectsOversizedFile()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString path = directory.filePath(QStringLiteral("oversized.png"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.resize(200LL * 1024LL * 1024LL + 1LL));
    file.close();

    QString error;
    QVERIFY(!validateOcrImage(path, &error));
    QVERIFY(error.contains(QStringLiteral("200 MB")));
}

void OcrCoreTests::acceptsImageWiderThanOldLimit()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString path = directory.filePath(QStringLiteral("wide.png"));
    QImage image(8001, 1, QImage::Format_ARGB32);
    image.fill(Qt::white);
    QVERIFY(image.save(path));

    QString error;
    QVERIFY2(validateOcrImage(path, &error), qPrintable(error));
}

void OcrCoreTests::rejectsExcessivePixelCount()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString path = directory.filePath(QStringLiteral("too-many-pixels.png"));
    QImage image(12001, 10000, QImage::Format_Mono);
    image.fill(0);
    QVERIFY(image.save(path));

    QString error;
    QVERIFY(!validateOcrImage(path, &error));
    QVERIFY(error.contains(QStringLiteral("1.2 亿")));
}

void OcrCoreTests::batchQueueDeduplicatesAndNavigates()
{
    OcrBatchQueue queue;
    queue.replacePaths(QStringList()
        << QStringLiteral("C:/images/one.png")
        << QStringLiteral("C:/images/two.png")
        << QStringLiteral("C:/images/ONE.png"));

    QCOMPARE(queue.count(), 2);
    QCOMPARE(queue.currentIndex(), 0);
    QVERIFY(!queue.movePrevious());
    QVERIFY(queue.moveNext());
    QCOMPARE(queue.currentIndex(), 1);
    QVERIFY(!queue.moveNext());
}

void OcrCoreTests::batchQueueKeepsIndependentResults()
{
    OcrBatchQueue queue;
    queue.replacePaths(QStringList()
        << QStringLiteral("C:/images/one.png")
        << QStringLiteral("C:/images/two.png"));

    OcrResult firstResult;
    firstResult.ok = true;
    firstResult.text = QStringLiteral("first result");
    firstResult.elapsedMs = 42;
    QVERIFY(queue.applyResult(0, firstResult));
    QVERIFY(queue.setEditedText(0, QStringLiteral("edited first")));

    OcrResult secondResult;
    secondResult.ok = false;
    secondResult.errorCode = QStringLiteral("EMPTY_TEXT");
    secondResult.errorMessage = QStringLiteral("no text");
    QVERIFY(queue.applyResult(1, secondResult));

    QCOMPARE(queue.item(0).text, QStringLiteral("edited first"));
    QCOMPARE(queue.item(0).state, OcrBatchItemState::Completed);
    QCOMPARE(queue.item(1).state, OcrBatchItemState::Failed);
    QCOMPARE(queue.item(1).errorCode, QStringLiteral("EMPTY_TEXT"));
}

void OcrCoreTests::batchQueueFindsNextPendingImage()
{
    OcrBatchQueue queue;
    queue.replacePaths(QStringList()
        << QStringLiteral("one.png")
        << QStringLiteral("two.png")
        << QStringLiteral("three.png"));

    OcrResult completed;
    completed.ok = true;
    completed.text = QStringLiteral("done");
    QVERIFY(queue.applyResult(1, completed));

    QCOMPARE(queue.nextPendingIndex(1), 2);
    QVERIFY(queue.markProcessing(2));
    QCOMPARE(queue.nextPendingIndex(0), 0);
    QVERIFY(queue.applyResult(0, completed));
    QCOMPARE(queue.nextPendingIndex(0), -1);
}

void OcrCoreTests::parsesHelperSuccess()
{
    const QByteArray json =
        "{\"requestId\":\"a\",\"ok\":true,\"text\":\"\\u4e2d\\u6587 ABC\",\"elapsedMs\":42}";
    const OcrResult result = parseOcrHelperResponse(json, OcrEngine::RapidOcr);

    QVERIFY(result.ok);
    QCOMPARE(result.engine, OcrEngine::RapidOcr);
    QCOMPARE(result.text, QString::fromUtf8("中文 ABC"));
    QCOMPARE(result.elapsedMs, qint64(42));
}

void OcrCoreTests::parsesHelperTextBlocks()
{
    const QByteArray json =
        "{\"requestId\":\"a\",\"ok\":true,\"text\":\"Alpha\\nBeta\","
        "\"imageWidth\":640,\"imageHeight\":360,"
        "\"blocks\":["
        "{\"text\":\"Alpha\",\"confidence\":0.96,"
        "\"points\":[[10,20],[110,20],[110,50],[10,50]]},"
        "{\"text\":\"Beta\",\"confidence\":0.88,"
        "\"points\":[[12,70],[92,70],[92,100],[12,100]]}"
        "],\"elapsedMs\":42}";
    const OcrResult result = parseOcrHelperResponse(json, OcrEngine::RapidOcr);

    QVERIFY(result.ok);
    QCOMPARE(result.imageSize, QSize(640, 360));
    QCOMPARE(result.blocks.size(), 2);
    QCOMPARE(result.blocks.at(0).text, QStringLiteral("Alpha"));
    QCOMPARE(result.blocks.at(0).points.size(), 4);
    QCOMPARE(result.blocks.at(0).points.at(2), QPoint(110, 50));
    QCOMPARE(result.blocks.at(0).confidence, 0.96);
    QCOMPARE(result.blocks.at(1).boundingRect(), QRect(12, 70, 81, 31));
}

void OcrCoreTests::parsesHelperFailure()
{
    const QByteArray json =
        "{\"requestId\":\"a\",\"ok\":false,\"errorCode\":\"MODEL_MISSING\","
        "\"errorMessage\":\"model file is missing\"}";
    const OcrResult result = parseOcrHelperResponse(json, OcrEngine::RapidOcr);

    QVERIFY(!result.ok);
    QCOMPARE(result.errorCode, QStringLiteral("MODEL_MISSING"));
    QCOMPARE(result.errorMessage, QStringLiteral("model file is missing"));
}

void OcrCoreTests::rejectsMalformedHelperResponse()
{
    const OcrResult result =
        parseOcrHelperResponse(QByteArrayLiteral("{not-json"), OcrEngine::WindowsOcr);

    QVERIFY(!result.ok);
    QCOMPARE(result.engine, OcrEngine::WindowsOcr);
    QCOMPARE(result.errorCode, QStringLiteral("INVALID_RESPONSE"));
    QVERIFY(!result.errorMessage.isEmpty());
}

static QString fakeHelperPath()
{
    return QCoreApplication::applicationDirPath()
        + QStringLiteral("/fake_ocr_helper.exe");
}

static OcrRequest helperRequest()
{
    OcrRequest request;
    request.requestId = QStringLiteral("request-1");
    request.imagePath = QStringLiteral("unused.png");
    request.languages = QStringList()
        << QStringLiteral("zh-Hans")
        << QStringLiteral("en");
    request.engine = OcrEngine::RapidOcr;
    return request;
}

void OcrCoreTests::runsHelperSuccess()
{
    OcrHelperProcess helper;
    const OcrResult result =
        helper.recognize(fakeHelperPath(), { QStringLiteral("success") }, helperRequest(), 2000);

    QVERIFY2(result.ok, qPrintable(result.errorMessage));
    QCOMPARE(result.text, QString::fromUtf8("测试 ABC"));
}

void OcrCoreTests::rejectsNonExecutableHelperBeforeLaunch()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("unexpected-helper.py"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write("print('must never launch')\n") > 0);
    file.close();

    OcrHelperProcess helper;
    const OcrResult result = helper.recognize(
        path,
        QStringList(),
        helperRequest(),
        1000
    );
    QVERIFY(!result.ok);
    QCOMPARE(result.errorCode, QStringLiteral("PROGRAM_MISSING"));
    QVERIFY(result.errorMessage.contains(QStringLiteral(".exe")));
}

void OcrCoreTests::returnsHelperFailure()
{
    OcrHelperProcess helper;
    const OcrResult result =
        helper.recognize(fakeHelperPath(), { QStringLiteral("failure") }, helperRequest(), 2000);

    QVERIFY(!result.ok);
    QCOMPARE(result.errorCode, QStringLiteral("MODEL_MISSING"));
}

void OcrCoreTests::rejectsMalformedHelperProcessOutput()
{
    OcrHelperProcess helper;
    const OcrResult result =
        helper.recognize(fakeHelperPath(), { QStringLiteral("malformed") }, helperRequest(), 2000);

    QVERIFY(!result.ok);
    QCOMPARE(result.errorCode, QStringLiteral("INVALID_RESPONSE"));
}

void OcrCoreTests::timesOutHelperProcess()
{
    OcrHelperProcess helper;
    const OcrResult result =
        helper.recognize(fakeHelperPath(), { QStringLiteral("timeout") }, helperRequest(), 100);

    QVERIFY(!result.ok);
    QCOMPARE(result.errorCode, QStringLiteral("TIMEOUT"));
    QVERIFY(result.elapsedMs < 1500);
}

void OcrCoreTests::cancelsHelperProcess()
{
    CancellationSource cancellation;
    std::thread cancelThread([&cancellation]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
        cancellation.cancel();
    });

    OcrHelperProcess helper;
    const OcrResult result = helper.recognize(
        fakeHelperPath(),
        { QStringLiteral("timeout") },
        helperRequest(),
        2000,
        cancellation.token()
    );
    cancelThread.join();

    QVERIFY(!result.ok);
    QCOMPARE(result.errorCode, QStringLiteral("CANCELLED"));
    QVERIFY(result.elapsedMs < 1000);
}

void OcrCoreTests::cancelsCloudOcrBeforeValidation()
{
    CancellationSource cancellation;
    cancellation.cancel();

    OcrCloudClient client;
    const OcrResult result = client.recognize(
        OcrCloudConfig(),
        helperRequest(),
        cancellation.token()
    );

    QVERIFY(!result.ok);
    QCOMPARE(result.errorCode, QStringLiteral("CANCELLED"));
}

void OcrCoreTests::cancelsOcrManagerDuringRecognition()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString imagePath =
        directory.filePath(QStringLiteral("cancel-manager.png"));
    QImage image(16, 16, QImage::Format_ARGB32);
    image.fill(Qt::white);
    QVERIFY(image.save(imagePath));

    OcrManagerConfig config;
    config.rapidOcrProgram = fakeHelperPath();
    config.rapidOcrArguments =
        QStringList() << QStringLiteral("timeout");
    config.timeoutMs = 2000;

    OcrRequest request = helperRequest();
    request.imagePath = imagePath;

    OcrManager manager;
    manager.setConfig(config);
    OcrResult completedResult;
    bool completed = false;
    QEventLoop loop;
    manager.finishedCallback = [&](const OcrResult &result) {
        completedResult = result;
        completed = true;
        loop.quit();
    };

    QTimer::singleShot(80, &manager, [&manager]() {
        manager.cancel();
    });
    QTimer::singleShot(1500, &loop, &QEventLoop::quit);
    manager.recognize(request);
    loop.exec();

    QVERIFY(completed);
    QVERIFY(!completedResult.ok);
    QCOMPARE(
        completedResult.errorCode,
        QStringLiteral("CANCELLED")
    );
    QVERIFY(completedResult.elapsedMs < 1000);
    QVERIFY(!manager.isBusy());
}

void OcrCoreTests::recognizesWithWindowsOcrHelper()
{
    const QString imagePath = QDir::cleanPath(
        QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(
            QStringLiteral("../../../Project_RapidOcrOnnx-1.2.2/images/1.jpg")
        )
    );
    QVERIFY(QFileInfo::exists(imagePath));

    OcrRequest request;
    request.requestId = QStringLiteral("windows-1");
    request.imagePath = QDir::toNativeSeparators(imagePath);
    request.languages = QStringList()
        << QStringLiteral("zh-Hans")
        << QStringLiteral("en");
    request.engine = OcrEngine::WindowsOcr;

    const QString helperPath = QDir(QCoreApplication::applicationDirPath())
        .absoluteFilePath(QStringLiteral("../../../helpers/bin/vocekit-windows-ocr.exe"));
    OcrHelperProcess helper;
    const OcrResult result = helper.recognize(helperPath, QStringList(), request, 15000);

    QVERIFY2(result.ok, qPrintable(result.errorCode + QStringLiteral(": ") + result.errorMessage));
    QVERIFY(!result.text.trimmed().isEmpty());
}

void OcrCoreTests::reportsMissingWindowsOcrLanguage()
{
    OcrRequest request;
    request.requestId = QStringLiteral("windows-language-1");
    request.imagePath = QStringLiteral("unused.png");
    request.languages = QStringList() << QStringLiteral("xx-Invalid-Language");
    request.engine = OcrEngine::WindowsOcr;

    const QString helperPath = QDir(QCoreApplication::applicationDirPath())
        .absoluteFilePath(QStringLiteral("../../../helpers/bin/vocekit-windows-ocr.exe"));
    OcrHelperProcess helper;
    const OcrResult result = helper.recognize(helperPath, QStringList(), request, 5000);

    QVERIFY(!result.ok);
    QCOMPARE(result.errorCode, QStringLiteral("LANGUAGE_NOT_INSTALLED"));
}

void OcrCoreTests::recognizesWithRapidOcrHelper()
{
    OcrRequest request;
    request.requestId = QStringLiteral("rapid-1");
    request.imagePath = QDir(QCoreApplication::applicationDirPath())
        .absoluteFilePath(
            QStringLiteral("../../../Project_RapidOcrOnnx-1.2.2/images/1.jpg")
        );
    request.languages = QStringList()
        << QStringLiteral("zh-Hans")
        << QStringLiteral("en");
    request.engine = OcrEngine::RapidOcr;

    const QString helperPath = QDir(QCoreApplication::applicationDirPath())
        .absoluteFilePath(QStringLiteral("../../../helpers/bin/vocekit-rapidocr.exe"));
    OcrHelperProcess helper;
    const OcrResult result = helper.recognize(helperPath, QStringList(), request, 45000);

    QVERIFY2(result.ok, qPrintable(result.errorCode + QStringLiteral(": ") + result.errorMessage));
    QVERIFY(!result.text.trimmed().isEmpty());
}

void OcrCoreTests::reportsMissingRapidOcrModels()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    OcrRequest request = helperRequest();
    request.requestId = QStringLiteral("rapid-models-1");
    request.imagePath = QDir(QCoreApplication::applicationDirPath())
        .absoluteFilePath(
            QStringLiteral("../../../Project_RapidOcrOnnx-1.2.2/images/1.jpg")
        );

    const QString helperPath = QDir(QCoreApplication::applicationDirPath())
        .absoluteFilePath(QStringLiteral("../../../helpers/bin/vocekit-rapidocr.exe"));
    OcrHelperProcess helper;
    const OcrResult result = helper.recognize(
        helperPath,
        QStringList() << QStringLiteral("--models") << directory.path(),
        request,
        10000
    );

    QVERIFY(!result.ok);
    QCOMPARE(result.errorCode, QStringLiteral("MODEL_MISSING"));
}

void OcrCoreTests::automaticFallsBackToWindows()
{
    OcrResult rapidResult;
    rapidResult.engine = OcrEngine::RapidOcr;
    rapidResult.errorCode = QStringLiteral("MODEL_MISSING");
    rapidResult.errorMessage = QStringLiteral("缺少模型");

    OcrResult windowsResult;
    windowsResult.ok = true;
    windowsResult.engine = OcrEngine::WindowsOcr;
    windowsResult.text = QStringLiteral("fallback text");

    const OcrResult result = combineAutomaticOcrResults(rapidResult, windowsResult);
    QVERIFY(result.ok);
    QVERIFY(result.usedFallback);
    QCOMPARE(result.engine, OcrEngine::WindowsOcr);
    QCOMPARE(result.text, QStringLiteral("fallback text"));
}

void OcrCoreTests::automaticKeepsBothFailureMessages()
{
    OcrResult rapidResult;
    rapidResult.engine = OcrEngine::RapidOcr;
    rapidResult.errorCode = QStringLiteral("MODEL_LOAD_FAILED");
    rapidResult.errorMessage = QStringLiteral("模型加载失败");

    OcrResult windowsResult;
    windowsResult.engine = OcrEngine::WindowsOcr;
    windowsResult.errorCode = QStringLiteral("LANGUAGE_NOT_INSTALLED");
    windowsResult.errorMessage = QStringLiteral("语言包未安装");

    const OcrResult result = combineAutomaticOcrResults(rapidResult, windowsResult);
    QVERIFY(!result.ok);
    QVERIFY(result.usedFallback);
    QVERIFY(result.errorMessage.contains(QStringLiteral("RapidOCR：模型加载失败")));
    QVERIFY(result.errorMessage.contains(QStringLiteral("Windows OCR：语言包未安装")));
}

void OcrCoreTests::automaticDoesNotFallbackForCancelledRequest()
{
    QVERIFY(shouldFallbackFromRapidOcr(QStringLiteral("TIMEOUT")));
    QVERIFY(shouldFallbackFromRapidOcr(QStringLiteral("MODEL_MISSING")));
    QVERIFY(!shouldFallbackFromRapidOcr(QStringLiteral("CANCELLED")));
    QVERIFY(!shouldFallbackFromRapidOcr(QStringLiteral("INVALID_REQUEST")));
}

void OcrCoreTests::extractsCloudOcrText()
{
    QString error;
    QCOMPARE(
        extractCloudOcrText(QByteArrayLiteral("{\"text\":\"alpha\"}"), &error),
        QStringLiteral("alpha")
    );
    QCOMPARE(
        extractCloudOcrText(QByteArrayLiteral("{\"result\":\"beta\"}"), &error),
        QStringLiteral("beta")
    );
    QCOMPARE(
        extractCloudOcrText(QByteArrayLiteral("{\"content\":\"gamma\"}"), &error),
        QStringLiteral("gamma")
    );
    QCOMPARE(
        extractCloudOcrText(QByteArrayLiteral("{\"data\":{\"text\":\"delta\"}}"), &error),
        QStringLiteral("delta")
    );
    QCOMPARE(
        extractCloudOcrText(QByteArrayLiteral("{\"data\":{\"result\":\"epsilon\"}}"), &error),
        QStringLiteral("epsilon")
    );
}

void OcrCoreTests::rejectsMalformedCloudOcrResponse()
{
    QString error;
    QVERIFY(extractCloudOcrText(QByteArrayLiteral("{broken"), &error).isEmpty());
    QVERIFY(error.contains(QStringLiteral("JSON")));

    QVERIFY(extractCloudOcrText(QByteArrayLiteral("{\"ok\":true}"), &error).isEmpty());
    QVERIFY(error.contains(QStringLiteral("文字")));
}

void OcrCoreTests::formatsOcrBatchStatusText()
{
    OcrBatchItem item;
    QCOMPARE(ocrBatchStatusText(item), QString::fromUtf8("等待识别"));

    item.state = OcrBatchItemState::Processing;
    QCOMPARE(ocrBatchStatusText(item), QString::fromUtf8("正在识别"));

    item.state = OcrBatchItemState::Completed;
    item.usedFallback = true;
    QCOMPARE(ocrBatchStatusText(item), QString::fromUtf8("识别完成（已使用备用引擎）"));

    item.state = OcrBatchItemState::Failed;
    item.errorCode = QStringLiteral("E001");
    QCOMPARE(ocrBatchStatusText(item), QString::fromUtf8("识别失败 · E001"));

    item.state = OcrBatchItemState::Cancelled;
    QCOMPARE(ocrBatchStatusText(item), QString::fromUtf8("已取消"));
}

void OcrCoreTests::formatsOcrBatchCompletionText()
{
    QCOMPARE(ocrBatchCompletionText(0), QString::fromUtf8("全部识别完成"));
    QCOMPARE(ocrBatchCompletionText(2), QString::fromUtf8("全部处理完成 · 2 张失败"));
}

void OcrCoreTests::mapsScreenshotOcrEngineFromSettings()
{
    AppSettingsData settings;
    settings.ocrEngine = ocrEngineRapid();
    QCOMPARE(screenshotOcrEngineFromSettings(settings), OcrEngine::RapidOcr);

    settings.ocrEngine = ocrEngineWindows();
    QCOMPARE(screenshotOcrEngineFromSettings(settings), OcrEngine::WindowsOcr);

    settings.ocrEngine = ocrEngineCustomCloud();
    QCOMPARE(screenshotOcrEngineFromSettings(settings), OcrEngine::CustomCloud);

    settings.ocrEngine = ocrEngineVision();
    QCOMPARE(screenshotOcrEngineFromSettings(settings), OcrEngine::VisionModel);

    settings.ocrEngine.clear();
    QCOMPARE(screenshotOcrEngineFromSettings(settings), OcrEngine::Automatic);
}

void OcrCoreTests::buildsScreenshotOcrManagerConfig()
{
    AppSettingsData settings;
    settings.ocrTimeoutMs = 9000;
    settings.useSystemProxy = true;

    SecretConfig secrets;
    secrets.customOcrUrl = QStringLiteral("https://ocr.example.test");
    secrets.customOcrApiKey = QStringLiteral("ocr-key");
    secrets.customOcrModel = QStringLiteral("ocr-model");

    const OcrManagerConfig config =
        buildScreenshotOcrManagerConfig(settings, secrets);

    QCOMPARE(config.timeoutMs, 9000);
    QVERIFY(config.rapidOcrProgram.endsWith(QStringLiteral("vocekit-rapidocr.exe")));
    QVERIFY(config.windowsOcrProgram.endsWith(QStringLiteral("vocekit-windows-ocr.exe")));
    QCOMPARE(config.customCloud.url, secrets.customOcrUrl);
    QCOMPARE(config.customCloud.apiKey, secrets.customOcrApiKey);
    QCOMPARE(config.customCloud.model, secrets.customOcrModel);
    QCOMPARE(config.customCloud.timeoutMs, 9000);
    QVERIFY(config.customCloud.useSystemProxy);
}

QTEST_MAIN(OcrCoreTests)

#include "ocr_core_tests.moc"
