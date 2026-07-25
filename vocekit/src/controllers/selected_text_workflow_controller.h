#ifndef VOCEKIT_SELECTED_TEXT_WORKFLOW_CONTROLLER_H
#define VOCEKIT_SELECTED_TEXT_WORKFLOW_CONTROLLER_H

#include "../input/voice_input_collector.h"

#include <QObject>

#include <functional>

struct SelectedTextWorkflowRequest
{
    QString modeId;
    bool strongSelectionEnabled = false;
    bool useVoice = false;
    SelectedTextNativeWindowHandle targetWindow = nullptr;
};

struct SelectedTextWorkflowResult
{
    QString text;
    bool blocked = false;
};

// 选中文字读取依赖通过访问接口注入，控制器本身不依赖窗口或系统 API。
struct SelectedTextWorkflowAccess
{
    std::function<SelectedTextReadResult(
        const SelectedTextReadRequest &,
        const VocabularyPreCorrectionCallback &
    )> readSelectedText;
    VocabularyPreCorrectionCallback preCorrect;
    std::function<void(const QString &, const QString &)> setStatus;
    std::function<void()> hideStatusLater;
    std::function<void(const QString &, const QString &)> showInformation;
    std::function<void(const SelectedTextReadResult &)> recordReadResult;
};

// 负责选中文字的读取状态、词库预修正和缺失输入提示。
class SelectedTextWorkflowController : public QObject
{
public:
    explicit SelectedTextWorkflowController(
        const SelectedTextWorkflowAccess &access,
        QObject *parent = nullptr
    );

    SelectedTextWorkflowResult execute(
        const SelectedTextWorkflowRequest &request
    ) const;

private:
    SelectedTextWorkflowAccess m_access;
};

// Windows 选区读取的默认适配器单独放置，便于控制器行为测试。
SelectedTextWorkflowAccess defaultSelectedTextWorkflowAccess();

#endif // VOCEKIT_SELECTED_TEXT_WORKFLOW_CONTROLLER_H
