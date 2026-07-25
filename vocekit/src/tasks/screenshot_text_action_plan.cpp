#include "screenshot_text_action_plan.h"

namespace {

QString s(const char *text)
{
    return QString::fromUtf8(text);
}

QString modelFor(
    const AppSettingsData &settings,
    const QString &functionId)
{
    return settings.function(functionId).modelId.trimmed();
}

} // namespace

ScreenshotTextActionPlan buildScreenshotTextActionPlan(
    const AppSettingsData &settings,
    const QString &action)
{
    ScreenshotTextActionPlan plan;
    plan.model = modelFor(settings, QStringLiteral("dictate"));
    if (action == QStringLiteral("translate")) {
        plan.model = modelFor(settings, QStringLiteral("translate"));
        plan.systemPrompt = s(
            "你是翻译助手。把输入文字翻译成简体中文，只输出译文；"
            "如果原文主要是中文，则翻译成英文。"
        );
    } else if (action == QStringLiteral("polish")) {
        plan.systemPrompt = s(
            "你是文字润色助手。修正 OCR 错字和标点，改善表达，"
            "但不要添加原文没有的信息，只输出结果。"
        );
    } else if (action == QStringLiteral("summarize")) {
        plan.model = modelFor(settings, QStringLiteral("ask"));
        plan.systemPrompt = s(
            "你是内容总结助手。准确提炼输入文字的核心信息，只输出总结结果。"
        );
    } else {
        plan.systemPrompt = s(
            "你是 OCR 文本整理助手。修正明显识别错误，恢复合理段落和标点，"
            "不改变原意，只输出整理后的文字。"
        );
    }

    return plan;
}
