#ifndef VOCEKIT_SCREENSHOT_TEXT_ACTION_PLAN_H
#define VOCEKIT_SCREENSHOT_TEXT_ACTION_PLAN_H

#include "../config/app_settings_data.h"

#include <QString>

struct ScreenshotTextActionPlan
{
    QString model;
    QString systemPrompt;
};

// 根据明确的功能设置选择截图文字动作使用的模型和提示词。
ScreenshotTextActionPlan buildScreenshotTextActionPlan(
    const AppSettingsData &settings,
    const QString &action
);

#endif // VOCEKIT_SCREENSHOT_TEXT_ACTION_PLAN_H
