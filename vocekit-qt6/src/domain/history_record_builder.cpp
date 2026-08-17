#include "history_record_builder.h"

#include <QFileInfo>
#include <QJsonArray>

namespace {

QString hrbText(const char *text)
{
    return QString::fromUtf8(text);
}

QJsonObject rectToJson(const QRect &rect)
{
    QJsonObject object;
    object.insert(QStringLiteral("x"), rect.x());
    object.insert(QStringLiteral("y"), rect.y());
    object.insert(QStringLiteral("width"), rect.width());
    object.insert(QStringLiteral("height"), rect.height());
    return object;
}

} // namespace

QJsonObject HistoryRecordBuilder::buildMetadata(
    const HistoryRecordMetadataRequest &request
)
{
    QJsonObject item;
    item.insert(QStringLiteral("input"), request.input);
    item.insert(QStringLiteral("output"), request.output);
    item.insert(QStringLiteral("error"), request.error);
    item.insert(QStringLiteral("model"), request.model);
    item.insert(QStringLiteral("elapsedMs"), static_cast<double>(request.elapsedMs));
    item.insert(QStringLiteral("speechElapsedMs"), static_cast<double>(request.speechElapsedMs));
    item.insert(QStringLiteral("modelElapsedMs"), static_cast<double>(request.modelElapsedMs));
    item.insert(QStringLiteral("promptVersion"), request.promptVersion);

    const bool screenshotInput = request.runContext.screenshotInput;
    item.insert(
        QStringLiteral("inputSource"),
        screenshotInput
            ? QStringLiteral("screenshot")
            : (request.actionHadRecording
                ? QStringLiteral("voice")
                : QStringLiteral("text"))
    );
    item.insert(
        QStringLiteral("ocrEngine"),
        screenshotInput
            ? ocrEngineName(request.runContext.screenshotOcrEngine)
            : QString()
    );
    item.insert(
        QStringLiteral("ocrElapsedMs"),
        screenshotInput
            ? static_cast<double>(request.runContext.screenshotOcrElapsedMs)
            : -1
    );
    item.insert(
        QStringLiteral("ocrUsedFallback"),
        screenshotInput && request.runContext.screenshotOcrUsedFallback
    );
    if (screenshotInput && request.runContext.screenshotRect.isValid()) {
        item.insert(
            QStringLiteral("screenshotRect"),
            rectToJson(request.runContext.screenshotRect)
        );
    }

    item.insert(QStringLiteral("recordingTriggerMode"), request.recordingTriggerMode);
    item.insert(QStringLiteral("longRecording"), request.longRecording);
    item.insert(QStringLiteral("segmentCount"), request.recordingSegments.size());

    int failedSegmentCount = 0;
    QJsonArray failedSegments;
    QJsonArray segmentItems;
    for (const RecordingSegment &segment : request.recordingSegments) {
        QJsonObject segmentItem;
        segmentItem.insert(QStringLiteral("index"), segment.index);
        segmentItem.insert(QStringLiteral("audio"), segment.wavPath);
        segmentItem.insert(QStringLiteral("text"), segment.text);
        segmentItem.insert(QStringLiteral("error"), segment.error);
        segmentItem.insert(
            QStringLiteral("recognitionElapsedMs"),
            static_cast<double>(segment.recognitionElapsedMs)
        );
        segmentItem.insert(QStringLiteral("attempts"), segment.attempts);
        segmentItems.append(segmentItem);
        if (segment.text.trimmed().isEmpty()) {
            ++failedSegmentCount;
            failedSegments.append(segment.index);
        }
    }
    item.insert(QStringLiteral("failedSegmentCount"), failedSegmentCount);
    item.insert(QStringLiteral("failedSegments"), failedSegments);
    item.insert(QStringLiteral("segments"), segmentItems);

    if (!request.flowRunId.trimmed().isEmpty()) {
        item.insert(
            QStringLiteral("flowRunId"),
            request.flowRunId
        );
        item.insert(
            QStringLiteral("flowPublishedRevision"),
            request.flowPublishedRevision
        );
        item.insert(
            QStringLiteral("flowPublishedHash"),
            request.flowPublishedHash
        );
        item.insert(
            QStringLiteral("flowTrigger"),
            request.flowTrigger
        );
        item.insert(
            QStringLiteral("flowFailedNodeId"),
            request.flowFailedNodeId
        );
        item.insert(
            QStringLiteral("flowFailedNodeType"),
            request.flowFailedNodeType
        );
        item.insert(
            QStringLiteral("flowCancelled"),
            request.flowCancelled
        );
        QJsonArray traces;
        for (const HistoryFlowNodeTrace &trace :
             request.flowNodeTraces) {
            QJsonObject traceItem;
            traceItem.insert(
                QStringLiteral("nodeId"),
                trace.nodeId
            );
            traceItem.insert(
                QStringLiteral("nodeType"),
                trace.nodeType
            );
            traceItem.insert(
                QStringLiteral("state"),
                trace.state
            );
            traceItem.insert(
                QStringLiteral("elapsedMs"),
                static_cast<double>(trace.elapsedMs)
            );
            traceItem.insert(
                QStringLiteral("errorCode"),
                trace.errorCode
            );
            traceItem.insert(
                QStringLiteral("modelId"),
                trace.modelId
            );
            traceItem.insert(
                QStringLiteral("promptVersion"),
                trace.promptVersion
            );
            traces.append(traceItem);
        }
        item.insert(QStringLiteral("flowNodeTraces"), traces);
    }
    return item;
}

QJsonObject HistoryRecordBuilder::buildOcrPageMetadata(
    const OcrPageHistoryMetadataRequest &request
)
{
    const OcrResult &result = request.result;

    QJsonObject item;
    item.insert(QStringLiteral("input"), result.text);
    item.insert(QStringLiteral("output"), QString());
    item.insert(QStringLiteral("error"), result.ok
        ? QString()
        : result.errorCode + QStringLiteral(": ") + result.errorMessage);
    item.insert(QStringLiteral("model"), QString());
    item.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
    item.insert(QStringLiteral("speechElapsedMs"), -1);
    item.insert(QStringLiteral("modelElapsedMs"), -1);
    item.insert(QStringLiteral("ocrEngine"), ocrEngineName(result.engine));
    item.insert(QStringLiteral("ocrLanguages"), QJsonArray::fromStringList(
        request.languages.isEmpty()
            ? QStringList() << QStringLiteral("zh-Hans") << QStringLiteral("en")
            : request.languages
    ));
    item.insert(QStringLiteral("ocrElapsedMs"), static_cast<double>(result.elapsedMs));
    item.insert(QStringLiteral("ocrUsedFallback"), result.usedFallback);
    item.insert(
        QStringLiteral("imageFileName"),
        QFileInfo(request.imagePath).fileName()
    );
    item.insert(QStringLiteral("promptVersion"), QString());
    item.insert(QStringLiteral("favoriteFolder"), QString());
    return item;
}

QString HistoryRecordBuilder::ocrEngineName(OcrEngine engine)
{
    if (engine == OcrEngine::WindowsOcr) {
        return QStringLiteral("Windows OCR");
    }
    if (engine == OcrEngine::CustomCloud) {
        return hrbText("自定义云 OCR");
    }
    if (engine == OcrEngine::VisionModel) {
        return hrbText("AI 图片识别");
    }
    if (engine == OcrEngine::Automatic) {
        return hrbText("自动选择");
    }
    return QStringLiteral("RapidOCR");
}
