#ifndef VOCEKIT_FUNCTION_COMMAND_PAGE_H
#define VOCEKIT_FUNCTION_COMMAND_PAGE_H

#include "prompt_settings_adapter.h"

#include "../domain/app_legacy_types.h"

#include <QString>
#include <QList>
#include <QWidget>

#include <functional>

class HubSettingsState;
class QComboBox;
class QLayout;
class QScrollArea;
class QSpinBox;
class QVBoxLayout;

// 功能配置页只编辑类型化设置状态；持久化和自定义功能管理由主窗口装配。
struct FunctionCommandPageAccess
{
    HubSettingsState *settings = nullptr;
    PromptSettingsAccess prompts;
    std::function<void()> saveSettings;
    std::function<void(const QString &, const QString &, const CustomFunctionDef &)>
        manageCustomFunction;
};

class FunctionCommandPage : public QWidget
{
  public:
    explicit FunctionCommandPage(const FunctionCommandPageAccess &access,
                                 QWidget *parent = nullptr);

    QString functionId() const;
    void setFunctionId(const QString &id);
    void refresh();

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
                                   const QList<QWidget *> &rows);
    QComboBox *modelCombo(const QString &currentModel);
    QComboBox *resultTemplateCombo(const QString &currentTemplate);
    QSpinBox *displayTimeSpinBox(int seconds, bool allowManualClose,
                                 const QString &zeroText = QString());

    FunctionCommandPageAccess m_access;
    QScrollArea *m_scroll = nullptr;
    QVBoxLayout *m_contentLayout = nullptr;
    QString m_functionId;
};

#endif // VOCEKIT_FUNCTION_COMMAND_PAGE_H
