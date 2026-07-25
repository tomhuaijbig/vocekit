#include "selected_text_workflow_controller.h"

namespace {

QString workflowText(const char *text)
{
    return QString::fromUtf8(text);
}

} // namespace

SelectedTextWorkflowController::SelectedTextWorkflowController(
    const SelectedTextWorkflowAccess &access,
    QObject *parent
)
    : QObject(parent),
      m_access(access)
{
}

SelectedTextWorkflowResult SelectedTextWorkflowController::execute(
    const SelectedTextWorkflowRequest &request
) const
{
    SelectedTextWorkflowResult result;
    if (m_access.setStatus) {
        m_access.setStatus(
            workflowText("正在读取选中文字"),
            request.strongSelectionEnabled
                ? workflowText("普通读取失败时会使用强力选中")
                : workflowText("不会复制，也不会读取剪贴板")
        );
    }

    SelectedTextReadRequest readRequest;
    readRequest.modeId = request.modeId;
    readRequest.sourceLabel = workflowText("选中文字");
    readRequest.strongSelectionEnabled = request.strongSelectionEnabled;
    readRequest.useVoice = request.useVoice;
    readRequest.targetWindow = request.targetWindow;

    SelectedTextReadResult readResult;
    if (m_access.readSelectedText) {
        readResult = m_access.readSelectedText(
            readRequest,
            m_access.preCorrect
        );
    }
    result.text = readResult.text.trimmed();
    readResult.text = result.text;
    if (m_access.recordReadResult) {
        m_access.recordReadResult(readResult);
    }

    if (!result.text.isEmpty() || request.useVoice) {
        return result;
    }

    result.blocked = true;
    const QString title = workflowText("未识别到有选中文字");
    const QString message =
        workflowText("请先用鼠标拖选要处理的文字，再按快捷键。");
    if (m_access.setStatus) {
        m_access.setStatus(title, message);
    }
    if (m_access.hideStatusLater) {
        m_access.hideStatusLater();
    }
    if (m_access.showInformation) {
        m_access.showInformation(title, message);
    }
    return result;
}
