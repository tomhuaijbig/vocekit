#include "selected_text_diagnostic_task.h"

#include "diagnostic_helpers.h"

namespace {

QString stTr8(const char *text)
{
    return QString::fromUtf8(text);
}

QString readModeTitle(bool strongMode)
{
    return strongMode ? stTr8("强力读取") : stTr8("普通读取");
}

} // namespace

SelectedTextDiagnosticResult runSelectedTextDiagnosticTask(
    const SelectedTextDiagnosticRequest &request
)
{
    const QString selected = request.selectedText.trimmed();

    SelectedTextDiagnosticResult result;
    result.characterCount = selected.size();
    result.success = !selected.isEmpty();

    if (!result.success) {
        result.displayText = diagnosticStatusLine(
            readModeTitle(request.strongMode),
            stTr8("未识别到"),
            stTr8("没有读取到选中文字。请确认拖选完成后没有再次点击页面。")
        );
        return result;
    }

    result.displayText = diagnosticStatusLine(
        readModeTitle(request.strongMode),
        stTr8("通过"),
        stTr8("读取到 ") + QString::number(result.characterCount)
            + stTr8(" 个字符：\n") + compactDiagnosticError(selected)
    );
    return result;
}
