#include "ocr_manager.h"

#include "ocr_helper_process.h"

#include <QFuture>
#include <QFutureWatcher>
#include <QMetaObject>
#include <QtConcurrent>

namespace {

qint64 combinedElapsed(qint64 first, qint64 second)
{
    if (first < 0) {
        return second;
    }
    if (second < 0) {
        return first;
    }
    return first + second;
}

OcrResult managerError(
    OcrEngine engine,
    const QString &code,
    const QString &message)
{
    OcrResult result;
    result.engine = engine;
    result.errorCode = code;
    result.errorMessage = message;
    return result;
}

}

bool shouldFallbackFromRapidOcr(const QString &errorCode)
{
    static const QStringList fallbackCodes = {
        QStringLiteral("HELPER_MISSING"),
        QStringLiteral("PROGRAM_MISSING"),
        QStringLiteral("MODEL_MISSING"),
        QStringLiteral("RUNTIME_MISSING"),
        QStringLiteral("MODEL_LOAD_FAILED"),
        QStringLiteral("IMAGE_DECODE_FAILED"),
        QStringLiteral("RECOGNITION_FAILED"),
        QStringLiteral("TIMEOUT"),
        QStringLiteral("INVALID_RESPONSE"),
        QStringLiteral("PROCESS_CRASHED")
    };
    return fallbackCodes.contains(errorCode);
}

OcrResult combineAutomaticOcrResults(
    const OcrResult &rapidResult,
    const OcrResult &windowsResult)
{
    if (windowsResult.ok) {
        OcrResult result = windowsResult;
        result.usedFallback = true;
        result.elapsedMs = combinedElapsed(rapidResult.elapsedMs, windowsResult.elapsedMs);
        return result;
    }

    OcrResult result;
    result.engine = OcrEngine::Automatic;
    result.errorCode = QStringLiteral("ALL_ENGINES_FAILED");
    result.errorMessage =
        QStringLiteral("RapidOCR：") + rapidResult.errorMessage
        + QStringLiteral("\nWindows OCR：") + windowsResult.errorMessage;
    result.elapsedMs = combinedElapsed(rapidResult.elapsedMs, windowsResult.elapsedMs);
    result.usedFallback = true;
    return result;
}

OcrManager::OcrManager(QObject *parent)
    : QObject(parent)
{
}

OcrManager::~OcrManager()
{
    cancel();
    if (m_watcher) {
        m_watcher->disconnect(this);
    }
}

void OcrManager::setConfig(const OcrManagerConfig &config)
{
    m_config = config;
}

OcrManagerConfig OcrManager::config() const
{
    return m_config;
}

bool OcrManager::isBusy() const
{
    return m_busy;
}

void OcrManager::recognize(const OcrRequest &request)
{
    if (m_busy) {
        if (finishedCallback) {
            finishedCallback(managerError(
                request.engine,
                QStringLiteral("BUSY"),
                QStringLiteral("已有一张图片正在识别，请等待当前任务完成。")
            ));
        }
        return;
    }

    QString validationError;
    if (!validateOcrImage(request.imagePath, &validationError)) {
        if (finishedCallback) {
            finishedCallback(managerError(
                request.engine,
                QStringLiteral("INVALID_IMAGE"),
                validationError
            ));
        }
        return;
    }

    m_busy = true;
    m_cancellation = CancellationSource();
    if (statusCallback) {
        statusCallback(QStringLiteral("正在识别"));
    }

    const OcrManagerConfig currentConfig = m_config;
    const CancellationToken cancellation = m_cancellation.token();
    m_watcher = new QFutureWatcher<OcrResult>(this);

    connect(m_watcher, &QFutureWatcher<OcrResult>::finished, this, [this]() {
        const OcrResult result = m_watcher->result();
        m_watcher->deleteLater();
        m_watcher = nullptr;
        m_busy = false;

        if (statusCallback) {
            statusCallback(result.ok ? QStringLiteral("识别完成") : QStringLiteral("识别失败"));
        }
        if (finishedCallback) {
            finishedCallback(result);
        }
    });

    const QFuture<OcrResult> future = QtConcurrent::run(
        [request, currentConfig, cancellation]() {
            return runRecognition(request, currentConfig, cancellation);
        }
    );
    m_watcher->setFuture(future);
}

void OcrManager::cancel()
{
    if (m_busy) {
        m_cancellation.cancel();
    }
}

OcrResult OcrManager::runRecognition(
    const OcrRequest &request,
    const OcrManagerConfig &config,
    const CancellationToken &cancellation)
{
    if (cancellation.isCancellationRequested()) {
        return cancelledResult(request.engine);
    }

    if (request.engine == OcrEngine::RapidOcr) {
        OcrHelperProcess helper;
        return helper.recognize(
            config.rapidOcrProgram,
            config.rapidOcrArguments,
            request,
            config.timeoutMs,
            cancellation
        );
    }

    if (request.engine == OcrEngine::WindowsOcr) {
        OcrHelperProcess helper;
        return helper.recognize(
            config.windowsOcrProgram,
            config.windowsOcrArguments,
            request,
            config.timeoutMs,
            cancellation
        );
    }

    if (request.engine == OcrEngine::CustomCloud) {
        OcrCloudConfig cloudConfig = config.customCloud;
        cloudConfig.timeoutMs = config.timeoutMs;
        OcrCloudClient client;
        return client.recognize(
            cloudConfig,
            request,
            cancellation
        );
    }

    if (request.engine != OcrEngine::Automatic) {
        return managerError(
            request.engine,
            QStringLiteral("UNSUPPORTED_ENGINE"),
            QStringLiteral("当前 OCR 引擎尚未接入。")
        );
    }

    OcrRequest rapidRequest = request;
    rapidRequest.engine = OcrEngine::RapidOcr;
    OcrHelperProcess rapidHelper;
    const OcrResult rapidResult = rapidHelper.recognize(
        config.rapidOcrProgram,
        config.rapidOcrArguments,
        rapidRequest,
        config.timeoutMs,
        cancellation
    );
    if (rapidResult.ok || !shouldFallbackFromRapidOcr(rapidResult.errorCode)) {
        return rapidResult;
    }

    if (cancellation.isCancellationRequested()) {
        return cancelledResult(OcrEngine::Automatic);
    }

    OcrRequest windowsRequest = request;
    windowsRequest.engine = OcrEngine::WindowsOcr;
    OcrHelperProcess windowsHelper;
    const OcrResult windowsResult = windowsHelper.recognize(
        config.windowsOcrProgram,
        config.windowsOcrArguments,
        windowsRequest,
        config.timeoutMs,
        cancellation
    );
    return combineAutomaticOcrResults(rapidResult, windowsResult);
}

OcrResult OcrManager::cancelledResult(OcrEngine engine)
{
    return managerError(
        engine,
        QStringLiteral("CANCELLED"),
        QStringLiteral("OCR 识别已取消。")
    );
}
