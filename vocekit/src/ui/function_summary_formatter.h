#ifndef VOCEKIT_FUNCTION_SUMMARY_FORMATTER_H
#define VOCEKIT_FUNCTION_SUMMARY_FORMATTER_H

#include <QString>

// 功能摘要的只读展示数据。界面先收集配置值，格式化器只负责生成稳定文案。
struct FunctionSummaryViewData
{
    QString shortcut;
    QString modelTitle;
    bool useSelection = false;
    bool useVoice = false;
    bool useScreenshot = false;
    QString outputModeTitle;
    QString resultTemplateTitle;
    int floatingBarSeconds = 0;
    int resultPopupSeconds = 0;
    int countdownSeconds = 0;
    bool recordingBeepEnabled = false;
    QString recordingTriggerMode;
    bool longRecordingEnabled = false;
    QString promptTitle;
};

QString functionInputModeSummary(const FunctionSummaryViewData &data);
QString functionSummaryText(const FunctionSummaryViewData &data);

#endif // VOCEKIT_FUNCTION_SUMMARY_FORMATTER_H
