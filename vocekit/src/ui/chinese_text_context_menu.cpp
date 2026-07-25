#include "chinese_text_context_menu.h"

#include <QtWidgets>

ChineseTextContextMenu::ChineseTextContextMenu(QObject *parent)
    : QObject(parent)
{
}

bool ChineseTextContextMenu::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() != QEvent::ContextMenu) {
        return QObject::eventFilter(watched, event);
    }

    QWidget *widget = qobject_cast<QWidget *>(watched);
    if (!widget) {
        return QObject::eventFilter(watched, event);
    }

    QMenu *menu = nullptr;
    QWidget *current = widget;
    while (current && !menu) {
        if (auto *lineEdit = qobject_cast<QLineEdit *>(current)) {
            menu = lineEdit->createStandardContextMenu();
        } else if (auto *textEdit = qobject_cast<QTextEdit *>(current)) {
            menu = textEdit->createStandardContextMenu();
        } else if (auto *plainTextEdit = qobject_cast<QPlainTextEdit *>(current)) {
            menu = plainTextEdit->createStandardContextMenu();
        } else {
            current = current->parentWidget();
        }
    }
    if (!menu) {
        return QObject::eventFilter(watched, event);
    }

    translateActions(menu);
    auto *contextEvent = static_cast<QContextMenuEvent *>(event);
    menu->exec(contextEvent->globalPos());
    menu->deleteLater();
    return true;
}

QString ChineseTextContextMenu::normalizedActionText(QString text)
{
    text = text.section(QLatin1Char('\t'), 0, 0);
    text.remove(QLatin1Char('&'));
    return text.trimmed().toLower();
}

QString ChineseTextContextMenu::shortcutSuffix(const QString &text)
{
    const int separator = text.indexOf(QLatin1Char('\t'));
    return separator >= 0 ? text.mid(separator) : QString();
}

void ChineseTextContextMenu::translateActions(QMenu *menu)
{
    const QList<QAction *> actions = menu->actions();
    for (QAction *action : actions) {
        const QString text = normalizedActionText(action->text());
        const QString shortcut = shortcutSuffix(action->text());
        if (text == QStringLiteral("undo")) {
            action->setText(QString::fromUtf8("撤销") + shortcut);
        } else if (text == QStringLiteral("redo")) {
            action->setText(QString::fromUtf8("恢复") + shortcut);
        } else if (text == QStringLiteral("cut")) {
            action->setText(QString::fromUtf8("剪切") + shortcut);
        } else if (text == QStringLiteral("copy")) {
            action->setText(QString::fromUtf8("复制") + shortcut);
        } else if (text == QStringLiteral("paste")) {
            action->setText(QString::fromUtf8("粘贴") + shortcut);
        } else if (text == QStringLiteral("delete")) {
            action->setText(QString::fromUtf8("删除") + shortcut);
        } else if (text == QStringLiteral("select all")) {
            action->setText(QString::fromUtf8("全选") + shortcut);
        }
    }
}
