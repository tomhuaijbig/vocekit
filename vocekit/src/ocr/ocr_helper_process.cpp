#include "ocr_helper_process.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QVariant>

namespace {

const qint64 kMaximumHelperOutputBytes = 1024LL * 1024LL;

OcrResult helperError(
    OcrEngine engine,
    const QString &code,
    const QString &message,
    qint64 elapsedMs = -1)
{
    OcrResult result;
    result.engine = engine;
    result.errorCode = code;
    result.errorMessage = message;
    result.elapsedMs = elapsedMs;
    return result;
}

QByteArray requestPayload(const OcrRequest &request)
{
    QJsonObject root;
    root.insert(QStringLiteral("requestId"), request.requestId);
    root.insert(QStringLiteral("imagePath"), request.imagePath);

    QJsonArray languages;
    for (const QString &language : request.languages) {
        languages.append(language);
    }
    root.insert(QStringLiteral("languages"), languages);

    QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Compact);
    payload.append('\n');
    return payload;
}

}

OcrResult parseOcrHelperResponse(const QByteArray &payload, OcrEngine engine)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload.trimmed(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return helperError(
            engine,
            QStringLiteral("INVALID_RESPONSE"),
            QStringLiteral("OCR 助手返回了无法解析的数据。")
        );
    }

    const QJsonObject root = document.object();
    if (!root.contains(QStringLiteral("ok"))
        || !root.value(QStringLiteral("ok")).isBool()) {
        return helperError(
            engine,
            QStringLiteral("INVALID_RESPONSE"),
            QStringLiteral("OCR 助手返回内容缺少必要字段。")
        );
    }

    OcrResult result;
    result.ok = root.value(QStringLiteral("ok")).toBool();
    result.engine = engine;
    result.text = root.value(QStringLiteral("text")).toString();
    result.errorCode = root.value(QStringLiteral("errorCode")).toString();
    result.errorMessage = root.value(QStringLiteral("errorMessage")).toString();
    result.elapsedMs = root.value(QStringLiteral("elapsedMs")).toVariant().toLongLong();
    result.imageSize = QSize(
        root.value(QStringLiteral("imageWidth")).toInt(),
        root.value(QStringLiteral("imageHeight")).toInt()
    );
    const QJsonArray blocks = root.value(QStringLiteral("blocks")).toArray();
    for (const QJsonValue &blockValue : blocks) {
        const QJsonObject blockObject = blockValue.toObject();
        OcrTextBlock block;
        block.text = blockObject.value(QStringLiteral("text")).toString();
        block.confidence = blockObject.value(QStringLiteral("confidence")).toDouble(-1.0);
        const QJsonArray points = blockObject.value(QStringLiteral("points")).toArray();
        for (const QJsonValue &pointValue : points) {
            const QJsonArray point = pointValue.toArray();
            if (point.size() < 2) {
                continue;
            }
            block.points.append(QPoint(point.at(0).toInt(), point.at(1).toInt()));
        }
        if (!block.text.trimmed().isEmpty()) {
            result.blocks.append(block);
        }
    }

    if (result.ok && result.text.trimmed().isEmpty()) {
        result.ok = false;
        result.errorCode = QStringLiteral("EMPTY_TEXT");
        result.errorMessage = QStringLiteral("OCR 助手没有识别到文字。");
    } else if (!result.ok && result.errorCode.isEmpty()) {
        result.errorCode = QStringLiteral("HELPER_FAILED");
        if (result.errorMessage.isEmpty()) {
            result.errorMessage = QStringLiteral("OCR 助手处理失败。");
        }
    }

    return result;
}

OcrHelperProcess::OcrHelperProcess(QObject *parent)
    : QObject(parent)
{
}

OcrResult OcrHelperProcess::recognize(
    const QString &program,
    const QStringList &arguments,
    const OcrRequest &request,
    int timeoutMs,
    const CancellationToken &cancellation)
{
    if (cancellation.isCancellationRequested()) {
        return helperError(
            request.engine,
            QStringLiteral("CANCELLED"),
            QStringLiteral("OCR 识别已取消。"),
            0
        );
    }

    const QFileInfo programInfo(program);
    if (program.trimmed().isEmpty()
        || (programInfo.isAbsolute() && !programInfo.exists())) {
        return helperError(
            request.engine,
            QStringLiteral("PROGRAM_MISSING"),
            QStringLiteral("OCR 助手程序不存在。")
        );
    }

    QProcess process;
    m_process = &process;
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start(program, arguments, QIODevice::ReadWrite);

    QElapsedTimer timer;
    timer.start();

    if (!process.waitForStarted(qMin(timeoutMs, 5000))) {
        m_process.clear();
        return helperError(
            request.engine,
            QStringLiteral("START_FAILED"),
            QStringLiteral("无法启动 OCR 助手程序：") + process.errorString(),
            timer.elapsed()
        );
    }

    const QByteArray input = requestPayload(request);
    if (process.write(input) != input.size() || !process.waitForBytesWritten(3000)) {
        process.kill();
        process.waitForFinished(1000);
        m_process.clear();
        return helperError(
            request.engine,
            QStringLiteral("WRITE_FAILED"),
            QStringLiteral("无法向 OCR 助手发送识别请求。"),
            timer.elapsed()
        );
    }
    process.closeWriteChannel();

    QByteArray standardOutput;
    QByteArray standardError;
    bool outputTooLarge = false;

    while (process.state() != QProcess::NotRunning) {
        if (cancellation.isCancellationRequested()) {
            process.terminate();
            if (!process.waitForFinished(250)) {
                process.kill();
                process.waitForFinished(1000);
            }
            m_process.clear();
            return helperError(
                request.engine,
                QStringLiteral("CANCELLED"),
                QStringLiteral("OCR 识别已取消。"),
                timer.elapsed()
            );
        }

        const int remainingMs = timeoutMs - int(timer.elapsed());
        if (remainingMs <= 0) {
            process.terminate();
            if (!process.waitForFinished(250)) {
                process.kill();
                process.waitForFinished(1000);
            }
            m_process.clear();
            return helperError(
                request.engine,
                QStringLiteral("TIMEOUT"),
                QStringLiteral("OCR 识别超时，请重试或更换识别引擎。"),
                timer.elapsed()
            );
        }

        process.waitForReadyRead(qMin(remainingMs, 50));
        standardOutput.append(process.readAllStandardOutput());
        standardError.append(process.readAllStandardError());
        if (standardOutput.size() > kMaximumHelperOutputBytes
            || standardError.size() > kMaximumHelperOutputBytes) {
            outputTooLarge = true;
            process.kill();
            process.waitForFinished(1000);
            break;
        }
    }

    standardOutput.append(process.readAllStandardOutput());
    standardError.append(process.readAllStandardError());
    m_process.clear();

    if (outputTooLarge
        || standardOutput.size() > kMaximumHelperOutputBytes
        || standardError.size() > kMaximumHelperOutputBytes) {
        return helperError(
            request.engine,
            QStringLiteral("OUTPUT_TOO_LARGE"),
            QStringLiteral("OCR 助手返回的数据超过 1 MB，已停止处理。"),
            timer.elapsed()
        );
    }

    if (process.exitStatus() == QProcess::CrashExit) {
        return helperError(
            request.engine,
            QStringLiteral("PROCESS_CRASHED"),
            QStringLiteral("OCR 助手异常退出。") + QString::fromUtf8(standardError).trimmed(),
            timer.elapsed()
        );
    }

    if (standardOutput.trimmed().isEmpty()) {
        return helperError(
            request.engine,
            QStringLiteral("EMPTY_RESPONSE"),
            QStringLiteral("OCR 助手没有返回结果。") + QString::fromUtf8(standardError).trimmed(),
            timer.elapsed()
        );
    }

    OcrResult result = parseOcrHelperResponse(standardOutput, request.engine);
    if (result.elapsedMs < 0) {
        result.elapsedMs = timer.elapsed();
    }
    return result;
}

void OcrHelperProcess::stop()
{
    if (!m_process || m_process->state() == QProcess::NotRunning) {
        return;
    }

    m_process->terminate();
    if (!m_process->waitForFinished(250)) {
        m_process->kill();
        m_process->waitForFinished(1000);
    }
}
