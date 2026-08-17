#include "function_canvas_visual_style.h"

#include "../domain/function_flow_graph.h"
#include "../domain/function_flow_runtime_types.h"

namespace {

QColor color(const char *value)
{
    return QColor(QString::fromLatin1(value));
}

QString recordingSummary(const FunctionFlowNode &node)
{
    if (node.config.voice.recording.longRecordingEnabled) {
        return QStringLiteral("系统麦克风 · 长语音");
    }
    if (node.config.voice.recording.triggerMode
        == QStringLiteral("hold")) {
        return QStringLiteral("系统麦克风 · 按住说话");
    }
    return QStringLiteral("系统麦克风 · 按键说话");
}

QString inputRoleName(const QString &role)
{
    if (role == QStringLiteral("source")) {
        return QStringLiteral("原文");
    }
    if (role == QStringLiteral("instruction")) {
        return QStringLiteral("指令");
    }
    if (role == QStringLiteral("screenshot")) {
        return QStringLiteral("截图文字");
    }
    if (role == QStringLiteral("system")) {
        return QStringLiteral("系统消息");
    }
    return QStringLiteral("自定义");
}

} // namespace

QString functionCanvasNodeDisplayName(FunctionFlowNodeType type)
{
    switch (type) {
    case FunctionFlowNodeType::VoiceSource:
        return QStringLiteral("语音采集");
    case FunctionFlowNodeType::SelectionSource:
        return QStringLiteral("选中文字");
    case FunctionFlowNodeType::ScreenshotSource:
        return QStringLiteral("截图识别");
    case FunctionFlowNodeType::Input:
        return QStringLiteral("输入节点");
    case FunctionFlowNodeType::Model:
        return QStringLiteral("调用大模型");
    case FunctionFlowNodeType::Output:
        return QStringLiteral("输出节点");
    case FunctionFlowNodeType::ResultPopup:
        return QStringLiteral("结果小框");
    case FunctionFlowNodeType::ScreenshotPanel:
        return QStringLiteral("截图对照窗");
    case FunctionFlowNodeType::AutoWrite:
        return QStringLiteral("自动写入");
    }
    return QStringLiteral("未知节点");
}

QString functionCanvasNodeGlyph(FunctionFlowNodeType type)
{
    switch (type) {
    case FunctionFlowNodeType::VoiceSource:
        return QStringLiteral("声");
    case FunctionFlowNodeType::SelectionSource:
        return QStringLiteral("选");
    case FunctionFlowNodeType::ScreenshotSource:
        return QStringLiteral("图");
    case FunctionFlowNodeType::Input:
        return QStringLiteral("入");
    case FunctionFlowNodeType::Model:
        return QStringLiteral("模");
    case FunctionFlowNodeType::Output:
        return QStringLiteral("出");
    case FunctionFlowNodeType::ResultPopup:
        return QStringLiteral("显");
    case FunctionFlowNodeType::ScreenshotPanel:
        return QStringLiteral("照");
    case FunctionFlowNodeType::AutoWrite:
        return QStringLiteral("写");
    }
    return QStringLiteral("?");
}

QString functionCanvasNodeSummary(const FunctionFlowNode &node)
{
    switch (node.type) {
    case FunctionFlowNodeType::VoiceSource:
        return recordingSummary(node);
    case FunctionFlowNodeType::SelectionSource:
        return QStringLiteral("读取当前选中文字");
    case FunctionFlowNodeType::ScreenshotSource:
        return QStringLiteral("自动识别 · 简体中文");
    case FunctionFlowNodeType::Input:
        return QStringLiteral("内容角色：%1 · %2")
            .arg(
                inputRoleName(node.config.input.role),
                node.config.input.required
                    ? QStringLiteral("必需")
                    : QStringLiteral("可选")
            );
    case FunctionFlowNodeType::Model:
        return QStringLiteral("等待全部输入");
    case FunctionFlowNodeType::Output:
        return QStringLiteral("整理最终结果");
    case FunctionFlowNodeType::ResultPopup:
        if (node.config.popup.displaySeconds > 0) {
            return QStringLiteral("显示结果 · %1 秒后关闭")
                .arg(node.config.popup.displaySeconds);
        }
        return QStringLiteral("显示结果 · 手动关闭");
    case FunctionFlowNodeType::ScreenshotPanel:
        if (node.config.screenshotPanel.displaySeconds > 0) {
            return QStringLiteral("显示截图与识别结果 · %1 秒")
                .arg(node.config.screenshotPanel.displaySeconds);
        }
        return QStringLiteral("显示截图与识别结果");
    case FunctionFlowNodeType::AutoWrite:
        if (node.config.autoWrite.writeMode
            == QStringLiteral("replace")) {
            return QStringLiteral("替换当前选中文字");
        }
        if (node.config.autoWrite.writeMode
            == QStringLiteral("insert")) {
            return QStringLiteral("插入到当前光标位置");
        }
        return QStringLiteral("写入当前窗口");
    }
    return QStringLiteral("暂无摘要");
}

QColor functionCanvasNodeAccent(FunctionFlowNodeType type)
{
    switch (type) {
    case FunctionFlowNodeType::VoiceSource:
        return color("#2563eb");
    case FunctionFlowNodeType::SelectionSource:
        return color("#0ea5e9");
    case FunctionFlowNodeType::ScreenshotSource:
        return color("#0891b2");
    case FunctionFlowNodeType::Input:
        return color("#7c3aed");
    case FunctionFlowNodeType::Model:
        return color("#8b5cf6");
    case FunctionFlowNodeType::Output:
        return color("#a855f7");
    case FunctionFlowNodeType::ResultPopup:
        return color("#16a34a");
    case FunctionFlowNodeType::ScreenshotPanel:
        return color("#059669");
    case FunctionFlowNodeType::AutoWrite:
        return color("#0d9488");
    }
    return color("#64748b");
}

QColor functionCanvasPortColor(const QString &portId)
{
    if (portId == QStringLiteral("text_in")
        || portId == QStringLiteral("text_out")) {
        return color("#0891b2");
    }
    if (portId == QStringLiteral("action_in")
        || portId == QStringLiteral("action_out")) {
        return color("#7c3aed");
    }
    return color("#64748b");
}

QColor functionCanvasRuntimeColor(FunctionFlowNodeState state)
{
    switch (state) {
    case FunctionFlowNodeState::Pending:
        return color("#94a3b8");
    case FunctionFlowNodeState::Ready:
        return color("#64748b");
    case FunctionFlowNodeState::Running:
        return color("#2563eb");
    case FunctionFlowNodeState::Cancelling:
        return color("#f59e0b");
    case FunctionFlowNodeState::Succeeded:
        return color("#16a34a");
    case FunctionFlowNodeState::Skipped:
        return color("#64748b");
    case FunctionFlowNodeState::Failed:
        return color("#dc2626");
    case FunctionFlowNodeState::Blocked:
        return color("#ea580c");
    case FunctionFlowNodeState::Cancelled:
        return color("#64748b");
    }
    return color("#64748b");
}

QColor functionCanvasSurfaceColor()
{
    return color("#f8fafc");
}

QColor functionCanvasPanelBorderColor()
{
    return color("#d8dee8");
}
