#ifndef APP_DIALOGS_H
#define APP_DIALOGS_H

#include <QDialog>
#include <QString>

// 应用通用弹窗：统一移除 Windows 标题栏里无实际用途的上下文帮助按钮。
class AppDialog : public QDialog
{
public:
    explicit AppDialog(QWidget *parent = nullptr);
};

// 设置或说明弹窗：拦截标题栏“？”帮助模式，改为显示软件自己的中文说明。
class HelpDialog : public AppDialog
{
public:
    HelpDialog(const QString &helpTitle, const QString &helpText, QWidget *parent = nullptr);

protected:
    bool event(QEvent *qtEvent) override;

private:
    QString m_helpTitle;
    QString m_helpText;
};

#endif // APP_DIALOGS_H
