#include "vocabulary_test_card.h"

#include "../tasks/vocabulary_diagnostic_task.h"
#include "diagnostic_action_card.h"
#include "ui_style.h"

#include <QtWidgets>

namespace {

QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

} // namespace

VocabularyTestCard::VocabularyTestCard(
    const RequestProvider &requestProvider,
    QWidget *parent
)
    : QFrame(parent),
      m_requestProvider(requestProvider)
{
    setObjectName(QStringLiteral("card"));
    setProperty(
        "testSearchText",
        tr8("词库测试 词库文件 启用状态 有效词条 快捷键加入方式 示例替换 大模型注入")
    );
    setStyleSheet(cardStyle());

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);

    auto *top = new QHBoxLayout;
    auto *labels = new QVBoxLayout;
    auto *name = new QLabel(tr8("词库测试"));
    name->setFont(appFont(13, QFont::DemiBold));
    auto *hint = new QLabel(tr8("检查词库文件、启用状态、有效词条数量和示例替换效果。"));
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: #667085;"));
    labels->addWidget(name);
    labels->addWidget(hint);

    m_button = new QPushButton(tr8("开始测试"));
    m_button->setMinimumSize(96, 36);
    m_button->setStyleSheet(buttonStyle(QStringLiteral("#111827")));
    connect(m_button, &QPushButton::clicked, this, [this]() {
        runTest();
    });

    top->addLayout(labels, 1);
    top->addWidget(m_button, 0, Qt::AlignTop);
    layout->addLayout(top);

    m_result = new QLabel;
    m_result->setWordWrap(true);
    m_result->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_result->setVisible(false);
    m_result->setStyleSheet(QStringLiteral(
        "QLabel { background: #f2f4f7; color: #344054; border-radius: 6px; padding: 10px; }"
    ));
    layout->addWidget(m_result);
}

void VocabularyTestCard::runTest()
{
    if (!m_button || !m_result || !m_requestProvider) {
        return;
    }
    m_button->setEnabled(false);

    const VocabularyDiagnosticRequest request = m_requestProvider();

    showDiagnosticResult(
        m_result,
        runVocabularyDiagnosticTask(request).join(QStringLiteral("\n\n"))
    );
    m_button->setEnabled(true);
}
