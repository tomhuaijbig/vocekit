#ifndef CHINESE_TEXT_CONTEXT_MENU_H
#define CHINESE_TEXT_CONTEXT_MENU_H

#include <QObject>
#include <QString>

class QEvent;
class QMenu;

// 中文右键菜单：把 Qt 文本框默认英文菜单翻译成中文，同时保留 Ctrl+C 等快捷键提示。
class ChineseTextContextMenu : public QObject
{
public:
    explicit ChineseTextContextMenu(QObject *parent = nullptr);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    static QString normalizedActionText(QString text);
    static QString shortcutSuffix(const QString &text);
    static void translateActions(QMenu *menu);
};

#endif // CHINESE_TEXT_CONTEXT_MENU_H
