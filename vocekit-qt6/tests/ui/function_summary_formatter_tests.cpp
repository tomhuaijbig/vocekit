#include <QtTest>

#include "../../src/ui/function_summary_formatter.h"

class FunctionSummaryFormatterTests : public QObject
{
    Q_OBJECT

private slots:
    void formatsEnabledInputModes();
    void formatsDisabledInputModes();
    void formatsCompleteFunctionSummary();
};

void FunctionSummaryFormatterTests::formatsEnabledInputModes()
{
    FunctionSummaryViewData data;
    data.useSelection = true;
    data.useVoice = true;
    data.useScreenshot = true;

    QCOMPARE(
        functionInputModeSummary(data),
        QString::fromUtf8("选中文字 + 语音 + 截图")
    );
}

void FunctionSummaryFormatterTests::formatsDisabledInputModes()
{
    const FunctionSummaryViewData data;

    QCOMPARE(
        functionInputModeSummary(data),
        QString::fromUtf8("未启用")
    );
}

void FunctionSummaryFormatterTests::formatsCompleteFunctionSummary()
{
    FunctionSummaryViewData data;
    data.shortcut = QStringLiteral("Ctrl + Alt + 1");
    data.modelTitle = QString::fromUtf8("DeepSeek 快速模型");
    data.useSelection = true;
    data.useScreenshot = true;
    data.outputModeTitle = QString::fromUtf8("结果小框");
    data.resultTemplateTitle = QString::fromUtf8("简洁");
    data.floatingBarSeconds = 0;
    data.floatingBarStyleTitle = QString::fromUtf8("实时文字卡片");
    data.resultPopupSeconds = 0;
    data.countdownSeconds = 3;
    data.recordingBeepEnabled = false;
    data.recordingTriggerMode = QStringLiteral("hold");
    data.longRecordingEnabled = true;
    data.promptTitle = QString::fromUtf8("润色提示词");

    QCOMPARE(
        functionSummaryText(data),
        QString::fromUtf8(
            "Ctrl + Alt + 1 · 模型：DeepSeek 快速模型"
            " · 输入：选中文字 + 截图"
            " · 展现：结果小框"
            " · 模板：简洁"
            " · 浮动条：不调用"
            " · 漂浮窗：实时文字卡片"
            " · 结果小框：手动关闭"
            " · 倒计时：3 秒"
            " · 提示音：关闭"
            " · 录音方式：按住说话"
            " · 长录音：开启"
            " · 提示词：润色提示词"
        )
    );
}

QTEST_MAIN(FunctionSummaryFormatterTests)
#include "function_summary_formatter_tests.moc"
