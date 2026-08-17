#ifndef VOCEKIT_COMMAND_CENTER_SHELL_H
#define VOCEKIT_COMMAND_CENTER_SHELL_H

#include <QMap>
#include <QString>
#include <QVector>
#include <QWidget>

#include <functional>

class QLineEdit;
class QPushButton;
class QVBoxLayout;

struct CommandCenterFunctionItem
{
    QString id;
    QString title;
    QString shortcut;
};

// 主窗口只提供导航数据和动作，导航控件及选中状态由外壳统一管理。
struct CommandCenterShellAccess
{
    std::function<QVector<CommandCenterFunctionItem>()> functionsProvider;
    std::function<void(const QString &)> openFunction;
    std::function<void(const QString &)> openTool;
    std::function<void()> addFunction;
    std::function<void(const QString &)> searchMissed;
};

class CommandCenterShell : public QWidget
{
public:
    explicit CommandCenterShell(
        const CommandCenterShellAccess &access,
        QWidget *pages,
        QWidget *parent = nullptr
    );

    void refreshFunctions();
    void setActivePage(const QString &pageId, const QString &functionId = QString());
    QString activePageId() const;
    QString activeFunctionId() const;

private:
    QWidget *sidebar();
    QWidget *toolbar();
    QPushButton *functionButton(const CommandCenterFunctionItem &item);
    QPushButton *addFunctionButton();
    QPushButton *toolButton(const QString &id, const QString &title);
    void clearFunctionLayout();
    void refreshButtonStyles();

    CommandCenterShellAccess m_access;
    QVBoxLayout *m_functionLayout = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QMap<QString, QPushButton *> m_functionButtons;
    QMap<QString, QPushButton *> m_toolButtons;
    QString m_activePageId;
    QString m_activeFunctionId;
};

QString commandCenterSidebarStyle();
QString commandCenterFunctionButtonStyle(bool active, bool addButton = false);
QString commandCenterToolButtonStyle(bool active);
QString commandCenterSearchStyle();
QString commandCenterSectionStyle();

#endif
