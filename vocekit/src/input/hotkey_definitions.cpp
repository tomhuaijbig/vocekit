#include "hotkey_definitions.h"

#include <QKeySequence>

namespace {

QString text8(const char *text)
{
    return QString::fromUtf8(text);
}

} // namespace

const QVector<HotkeyDef> &hotkeyDefs()
{
    static const QVector<HotkeyDef> defs = {
        {
            QStringLiteral("dictate"),
            text8("听写（Dictate）"),
            QStringLiteral("Ctrl+Alt+Space"),
            text8("开始或停止语音输入")
        },
        {
            QStringLiteral("translate"),
            text8("翻译（Translate）"),
            QStringLiteral("Ctrl+Alt+T"),
            text8("翻译鼠标拖选的文字")
        },
        {
            QStringLiteral("ask"),
            text8("问答（Ask）"),
            QStringLiteral("Ctrl+Alt+Q"),
            text8("基于选中文本回答语音问题")
        },
        {
            QStringLiteral("vocabulary_add"),
            text8("加入词库"),
            QStringLiteral("Ctrl+Alt+L"),
            text8("把鼠标拖选的文字加入词库")
        },
        {
            QStringLiteral("hub"),
            text8("打开主界面"),
            QStringLiteral("Ctrl+Alt+S"),
            text8("打开主界面和设置入口")
        }
    };
    return defs;
}

const QVector<HotkeyDef> &coreFunctionDefs()
{
    static const QVector<HotkeyDef> defs = []() {
        QVector<HotkeyDef> items = hotkeyDefs();
        for (int i = items.size() - 1; i >= 0; --i) {
            if (items.at(i).id == QStringLiteral("hub")
                || items.at(i).id == QStringLiteral("vocabulary_add")) {
                items.remove(i);
            }
        }
        return items;
    }();
    return defs;
}

QString defaultScreenshotShortcutForFunction(const QString &id)
{
    if (id == QStringLiteral("dictate")) {
        return QStringLiteral("Ctrl+Alt+Shift+Space");
    }
    if (id == QStringLiteral("translate")) {
        return QStringLiteral("Ctrl+Alt+Shift+T");
    }
    if (id == QStringLiteral("ask")) {
        return QStringLiteral("Ctrl+Alt+Shift+Q");
    }
    return QStringLiteral("Ctrl+Alt+Shift+O");
}

QString screenshotShortcutFromFunctionShortcut(const QString &shortcut)
{
    const QKeySequence sequence(shortcut);
    if (sequence.isEmpty()) {
        return QStringLiteral("Ctrl+Alt+Shift+O");
    }
    return QKeySequence(sequence[0] | Qt::SHIFT).toString(QKeySequence::NativeText);
}
