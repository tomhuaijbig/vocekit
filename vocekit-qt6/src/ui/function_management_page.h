#ifndef VOCEKIT_FUNCTION_MANAGEMENT_PAGE_H
#define VOCEKIT_FUNCTION_MANAGEMENT_PAGE_H

#include "../domain/app_legacy_types.h"

#include <QString>
#include <QVector>
#include <QWidget>

#include <functional>

class QLayout;
class QVBoxLayout;

// 管理页只渲染摘要项；配置读写和编辑器调用由装配层提供。
struct FunctionManagementItem
{
    QString id;
    QString title;
    QString shortcut;
    QString summary;
    bool custom = false;
    CustomFunctionDef function;
};

struct FunctionManagementPageAccess
{
    std::function<QVector<FunctionManagementItem>()> itemsProvider;
    std::function<void()> addFunction;
    std::function<void(const FunctionManagementItem &)> editFunction;
    std::function<void(const FunctionManagementItem &)> removeFunction;
};

class FunctionManagementPage : public QWidget
{
  public:
    explicit FunctionManagementPage(
        const FunctionManagementPageAccess &access,
        QWidget *parent = nullptr
    );

    void refresh();

  private:
    QWidget *summaryCard(const FunctionManagementItem &item);
    void clearLayout(QLayout *layout);

    FunctionManagementPageAccess m_access;
    QVBoxLayout *m_listLayout = nullptr;
};

#endif // VOCEKIT_FUNCTION_MANAGEMENT_PAGE_H
