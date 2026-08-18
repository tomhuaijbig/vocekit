#ifndef VOCEKIT_FUNCTION_COMMAND_PAGE_H
#define VOCEKIT_FUNCTION_COMMAND_PAGE_H

#include "prompt_settings_adapter.h"
#include "function_flow_settings_access.h"
#include "../domain/function_flow_runtime_types.h"

#include "../domain/app_legacy_types.h"

#include <QString>
#include <QStringList>
#include <QList>
#include <QWidget>

#include <functional>

class HubSettingsState;
class FunctionCanvasEditor;
struct FunctionFlowPlacementDefaults;
class QComboBox;
class QLayout;
class QScrollArea;
class QSpinBox;
class QStackedWidget;
class QVBoxLayout;

// 功能配置页只编辑类型化设置状态；持久化和自定义功能管理由主窗口装配。
struct FunctionCommandPageAccess
{
    HubSettingsState *settings = nullptr;
    PromptSettingsAccess prompts;
    FunctionFlowSettingsAccess flows;
    std::function<void()> saveSettings;
    std::function<void(const QString &)> functionRenamed;
    std::function<void(const QString &)> functionRemoved;
    std::function<void(const OperationError &)> operationFailed;
};

class FunctionCommandPage : public QWidget
{
  public:
    explicit FunctionCommandPage(const FunctionCommandPageAccess &access,
                                 QWidget *parent = nullptr);

    QString functionId() const;
    bool setFunctionId(const QString &id);
    void refresh();
    void refreshCanvasState();
    bool flushPendingFlowDraft();
    void discardPendingFlowDraft();
    bool applyFunctionFlowRuntimeEvent(
        const FunctionFlowNodeExecutionEvent &event
    );
    bool applyFunctionFlowRunEvent(
        const FunctionFlowRunExecutionEvent &event
    );
    FunctionCanvasEditor *canvasEditor() const;

  private:
    QString functionTitle(const QString &id) const;
    CustomFunctionDef customFunction(const QString &id, bool *customOut = nullptr) const;
    void saveSettings();
    void clearLayout(QLayout *layout);
    QWidget *commandField(const QString &label, QWidget *control);
    QWidget *commandAccordionCard(const QString &title, const QString &description,
                                  const QString &statusText, bool checked, bool showToggle,
                                  bool expanded, QWidget *body,
                                  const std::function<void(bool)> &onToggle);
    QWidget *commandControlSection(const QString &title, const QString &hint,
                                   const QList<QWidget *> &rows,
                                   const QStringList &rowIds = QStringList(),
                                   const std::function<void(const QStringList &)>
                                       &onOrderChanged =
                                           std::function<void(const QStringList &)>());
    QComboBox *modelCombo(const QString &currentModel);
    QComboBox *resultTemplateCombo(const QString &currentTemplate);
    QSpinBox *displayTimeSpinBox(int seconds, bool allowManualClose,
                                 const QString &zeroText = QString());
    FunctionCanvasEditor *ensureCanvasEditor();
    bool setCanvasMode(bool enabled);
    bool changeExecutionMode(FunctionExecutionMode mode);
    void reportFlowFailure(const OperationError &error);
    FunctionFlowPlacementDefaults flowPlacementDefaults(
        const QString &functionId
    ) const;

    FunctionCommandPageAccess m_access;
    QStackedWidget *m_pageStack = nullptr;
    QScrollArea *m_scroll = nullptr;
    QWidget *m_canvasHost = nullptr;
    QVBoxLayout *m_settingsLayout = nullptr;
    QVBoxLayout *m_canvasLayout = nullptr;
    QVBoxLayout *m_contentLayout = nullptr;
    QString m_functionId;
    bool m_canvasMode = false;
    FunctionCanvasEditor *m_canvasEditor = nullptr;
};

#endif // VOCEKIT_FUNCTION_COMMAND_PAGE_H
