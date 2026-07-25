#ifndef VOCEKIT_SCREENSHOT_TEXT_ACTION_TASK_H
#define VOCEKIT_SCREENSHOT_TEXT_ACTION_TASK_H

#include "../domain/app_legacy_types.h"
#include "cancellation_token.h"
#include "screenshot_text_action_plan.h"

#include <QString>

struct ScreenshotTextActionTaskRequest
{
    QString model;
    QString systemPrompt;
    QString sourceText;
    bool useSystemProxy = false;
    CancellationToken cancellation;
};

// 截图工具栏里的智能整理、翻译、润色、总结共用这一套任务。
OcrAiTaskResult runScreenshotTextActionTask(
    const ScreenshotTextActionTaskRequest &request
);

#endif // VOCEKIT_SCREENSHOT_TEXT_ACTION_TASK_H
