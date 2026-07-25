#include "ocr_batch_text.h"

namespace {

QString ocrText(const char *text)
{
    return QString::fromUtf8(text);
}

} // namespace

QString ocrBatchStatusText(const OcrBatchItem &item)
{
    if (item.state == OcrBatchItemState::Processing) {
        return ocrText("正在识别");
    }
    if (item.state == OcrBatchItemState::Completed) {
        return item.usedFallback
            ? ocrText("识别完成（已使用备用引擎）")
            : ocrText("识别完成");
    }
    if (item.state == OcrBatchItemState::Failed) {
        return item.errorCode.isEmpty()
            ? ocrText("识别失败")
            : ocrText("识别失败 · ") + item.errorCode;
    }
    if (item.state == OcrBatchItemState::Cancelled) {
        return ocrText("已取消");
    }
    return ocrText("等待识别");
}

QString ocrBatchCompletionText(int failedCount)
{
    if (failedCount > 0) {
        return ocrText("全部处理完成 · ")
            + QString::number(failedCount)
            + ocrText(" 张失败");
    }
    return ocrText("全部识别完成");
}
