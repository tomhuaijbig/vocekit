#ifndef VOCEKIT_VOICE_RUN_CONTEXT_H
#define VOCEKIT_VOICE_RUN_CONTEXT_H

#include "../ocr/ocr_types.h"
#include "../result_flow_config.h"

#include <QImage>
#include <QRect>
#include <QString>
#include <QVector>

// 单次功能执行的上下文。语音、选中文字、截图 OCR 和网络策略都通过这个
// 结构在“输入收集 -> 语音识别 -> 模型处理 -> 结果输出”之间传递。
struct VoiceRunContext
{
    QString modeId;
    QString selectedText;
    QString voiceText;
    QString textOnlyInput;
    bool textOnly = false;
    bool screenshotInput = false;
    QImage screenshotImage;
    QVector<OcrTextBlock> screenshotBlocks;
    QString screenshotRecognizedText;
    OcrEngine screenshotOcrEngine = OcrEngine::Automatic;
    qint64 screenshotOcrElapsedMs = -1;
    bool screenshotOcrUsedFallback = false;
    QRect screenshotRect;
    FunctionNetworkPolicies networkPolicies;

    bool hasSelectedText() const;
    bool hasVoiceText() const;
    bool hasTextOnlyInput() const;
    bool hasScreenshotText() const;
};

#endif // VOCEKIT_VOICE_RUN_CONTEXT_H
