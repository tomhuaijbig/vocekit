#include "selection_input_test_card.h"

#include "attention_message.h"
#include "diagnostic_action_card.h"
#include "floating_bar.h"
#include "ui_style.h"
#include "../input/selected_text_reader.h"
#include "../tasks/selected_text_diagnostic_task.h"

#include <QtWidgets>

namespace {

QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

} // namespace

SelectionInputTestCard::SelectionInputTestCard(
    const std::function<bool()> &floatingBarEnabled,
    FloatingBar *floatingBar,
    QWidget *hostWindow,
    QWidget *parent
)
    : QFrame(parent),
      m_floatingBarEnabled(floatingBarEnabled),
      m_floatingBar(floatingBar),
      m_hostWindow(hostWindow)
{
    setObjectName(QStringLiteral("card"));
    setProperty(
        "testSearchText",
        tr8("选中文字测试 普通读取 强力选中 强力读取 鼠标拖选")
    );
    setStyleSheet(cardStyle());

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);

    auto *top = new QHBoxLayout;
    auto *name = new QLabel(tr8("选中文字测试"));
    name->setFont(appFont(13, QFont::DemiBold));

    m_normalButton = new QPushButton(tr8("普通读取"));
    m_normalButton->setMinimumSize(96, 36);
    m_normalButton->setStyleSheet(buttonStyle(QStringLiteral("#111827")));

    m_strongButton = new QPushButton(tr8("强力读取"));
    m_strongButton->setMinimumSize(96, 36);
    m_strongButton->setStyleSheet(
        buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827"))
    );

    connect(m_normalButton, &QPushButton::clicked, this, [this]() {
        start(false);
    });
    connect(m_strongButton, &QPushButton::clicked, this, [this]() {
        start(true);
    });

    top->addWidget(name, 1);
    top->addWidget(m_normalButton);
    top->addWidget(m_strongButton);
    layout->addLayout(top);

    m_result = new QLabel;
    m_result->setWordWrap(true);
    m_result->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_result->setVisible(false);
    m_result->setStyleSheet(QStringLiteral(
        "QLabel { background: #f2f4f7; color: #344054; border-radius: 6px; padding: 10px; }"
    ));
    layout->addWidget(m_result);

    m_timer = new QTimer(this);
    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, [this]() {
        --m_seconds;
        if (m_seconds > 0) {
            if (m_floatingBar) {
                m_floatingBar->setStatus(
                    tr8("选中文字测试"),
                    QString::number(m_seconds) + tr8(" 秒后读取")
                );
            }
            return;
        }
        finish();
    });
}

void SelectionInputTestCard::start(bool strong)
{
    if (m_timer && m_timer->isActive()) {
        return;
    }
    if (m_floatingBarEnabled && !m_floatingBarEnabled()) {
        showAttentionInformation(
            m_hostWindow ? m_hostWindow.data() : this,
            tr8("无法开始测试"),
            tr8("选中文字测试需要用浮动条显示倒计时。请先在“设置 -> 常用设置”中启用浮动条。")
        );
        return;
    }

    m_strong = strong;
    m_seconds = 5;
    setButtonsEnabled(false);
    showDiagnosticResult(
        m_result,
        tr8("请在 5 秒内切换到目标窗口，用鼠标左键拖动选中文字。")
    );

    if (m_floatingBar) {
        m_floatingBar->setStatus(tr8("选中文字测试"), tr8("5 秒后读取"));
    }
    if (m_hostWindow) {
        m_hostWindow->hide();
    }
    m_timer->start();
}

void SelectionInputTestCard::finish()
{
    if (m_timer) {
        m_timer->stop();
    }

    const QString selected = SelectedTextReader::read(m_strong, nullptr).trimmed();
    if (m_hostWindow) {
        m_hostWindow->showNormal();
        m_hostWindow->raise();
        m_hostWindow->activateWindow();
    }
    if (m_floatingBar) {
        m_floatingBar->hide();
    }
    setButtonsEnabled(true);

    SelectedTextDiagnosticRequest request;
    request.selectedText = selected;
    request.strongMode = m_strong;
    showDiagnosticResult(m_result, runSelectedTextDiagnosticTask(request).displayText);
}

void SelectionInputTestCard::setButtonsEnabled(bool enabled)
{
    if (m_normalButton) {
        m_normalButton->setEnabled(enabled);
    }
    if (m_strongButton) {
        m_strongButton->setEnabled(enabled);
    }
}
