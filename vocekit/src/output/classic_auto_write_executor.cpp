#include "classic_auto_write_executor.h"

namespace {

QString text8(const char *text)
{
    return QString::fromUtf8(text);
}

QString failureReason(const QString &code)
{
    if (code == QStringLiteral("flow_target_window_unavailable")) {
        return text8("没有找到可写入的目标窗口。");
    }
    if (code
        == QStringLiteral("flow_target_window_activation_failed")) {
        return text8("无法激活原来的目标窗口。");
    }
    if (code == QStringLiteral("flow_clipboard_unavailable")
        || code == QStringLiteral("flow_clipboard_wrong_thread")) {
        return text8("系统剪贴板当前不可用。");
    }
    if (code == QStringLiteral("flow_input_injection_failed")) {
        return text8("系统输入指令发送失败。");
    }
    return text8("写入能力当前不可用。");
}

} // namespace

ClipboardWriteResult ClassicAutoWriteExecutor::execute(
    const ClassicAutoWriteRequest &request,
    const ClassicAutoWriteAccess &access)
{
    ClipboardWriteResult result;
    if (access.checkedWrite) {
        result = access.checkedWrite(
            request.text,
            request.replaceSelection,
            request.hasSelection
        );
    } else {
        result.errorCode = QStringLiteral("flow_auto_write_failed");
    }

    if (result.ok) {
        if (access.setStatus) {
            access.setStatus(
                text8("写入指令已发送"),
                text8("已完成目标、剪贴板和系统输入检查")
            );
        }
        if (access.log) {
            access.log(
                text8("完成"),
                QStringLiteral("status=ok")
            );
        }
        return result;
    }

    if (result.errorCode.trimmed().isEmpty()) {
        result.errorCode = QStringLiteral("flow_auto_write_failed");
    }
    if (access.setStatus) {
        access.setStatus(
            text8("未能自动写入，结果已保留"),
            failureReason(result.errorCode)
        );
    }
    if (access.log) {
        access.log(
            text8("失败"),
            QStringLiteral("error=") + result.errorCode
        );
    }
    if (request.popupFallbackEnabled
        && access.showFallbackPopup) {
        access.showFallbackPopup(request.text);
    }
    return result;
}
