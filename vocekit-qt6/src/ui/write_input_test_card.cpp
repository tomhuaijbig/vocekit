#include "write_input_test_card.h"

#include "ui_style.h"
#include "../output/clipboard_writer.h"

#include <QtWidgets>

namespace {

QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

ClipboardWindowHandle topLevelWindowHandle(QWidget *widget)
{
#ifdef Q_OS_WIN
    QWidget *topLevel = widget ? widget->window() : nullptr;
    return reinterpret_cast<ClipboardWindowHandle>(
        topLevel ? topLevel->winId() : WId(0)
    );
#else
    Q_UNUSED(widget);
    return nullptr;
#endif
}

} // namespace

WriteInputTestCard::WriteInputTestCard(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("card"));
    setProperty(
        "testSearchText",
        tr8("写入测试 写入光标 替换选中 内置文本框 剪贴板")
    );
    setStyleSheet(cardStyle());

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);

    auto *top = new QHBoxLayout;
    auto *name = new QLabel(tr8("写入测试"));
    name->setFont(appFont(13, QFont::DemiBold));

    auto *insertButton = new QPushButton(tr8("写入光标"));
    insertButton->setMinimumSize(96, 36);
    insertButton->setStyleSheet(buttonStyle(QStringLiteral("#111827")));

    auto *replaceButton = new QPushButton(tr8("替换选中"));
    replaceButton->setMinimumSize(96, 36);
    replaceButton->setStyleSheet(
        buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827"))
    );

    auto *resetButton = new QPushButton(tr8("恢复内容"));
    resetButton->setMinimumSize(96, 36);
    resetButton->setStyleSheet(
        buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827"))
    );

    top->addWidget(name, 1);
    top->addWidget(insertButton);
    top->addWidget(replaceButton);
    top->addWidget(resetButton);
    layout->addLayout(top);

    m_edit = new QTextEdit;
    m_edit->setAcceptRichText(false);
    m_edit->setMinimumHeight(92);
    m_edit->setStyleSheet(QStringLiteral(
        "QTextEdit { background: #ffffff; border: 1px solid #d0d5dd;"
        " border-radius: 7px; padding: 10px; }"
    ));
    layout->addWidget(m_edit);

    connect(insertButton, &QPushButton::clicked, this, [this]() {
        insertAtCursor();
    });
    connect(replaceButton, &QPushButton::clicked, this, [this]() {
        replaceSelection();
    });
    connect(resetButton, &QPushButton::clicked, this, [this]() {
        resetText();
    });

    resetText();
}

void WriteInputTestCard::resetText()
{
    if (!m_edit) {
        return;
    }
    m_edit->setPlainText(
        tr8("这是软件内部的写入测试区域。把光标放在任意位置，或选中一段文字后开始测试。")
    );
    QTextCursor cursor = m_edit->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_edit->setTextCursor(cursor);
}

void WriteInputTestCard::insertAtCursor()
{
    if (!m_edit) {
        return;
    }
    QTextCursor cursor = m_edit->textCursor();
    cursor.clearSelection();
    m_edit->setTextCursor(cursor);
    m_edit->setFocus();
    QApplication::processEvents();

#ifdef Q_OS_WIN
    ClipboardWriter::pasteTextToWindow(
        tr8("【写入测试通过】"),
        topLevelWindowHandle(this),
        true,
        false
    );
#else
    m_edit->insertPlainText(tr8("【写入测试通过】"));
#endif
}

void WriteInputTestCard::replaceSelection()
{
    if (!m_edit) {
        return;
    }
    QTextCursor cursor = m_edit->textCursor();
    if (!cursor.hasSelection()) {
        cursor.select(QTextCursor::WordUnderCursor);
        m_edit->setTextCursor(cursor);
    }
    m_edit->setFocus();
    QApplication::processEvents();

#ifdef Q_OS_WIN
    ClipboardWriter::pasteTextToWindow(
        tr8("【替换测试通过】"),
        topLevelWindowHandle(this),
        true,
        true
    );
#else
    m_edit->insertPlainText(tr8("【替换测试通过】"));
#endif
}
