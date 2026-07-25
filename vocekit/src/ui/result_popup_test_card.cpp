#include "result_popup_test_card.h"

#include <QString>

QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

#include "ui_style.h"

#include <QtWidgets>

ResultPopupTestCard::ResultPopupTestCard(
    const PreviewCallback &previewCallback,
    QWidget *parent
)
    : QFrame(parent),
      m_previewCallback(previewCallback)
{
    setObjectName(QStringLiteral("card"));
    setProperty(
        "testSearchText",
        tr8("结果小框测试 弹窗 复制 写入 替换 关闭 按钮 模板")
    );
    setStyleSheet(cardStyle());

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);

    auto *labels = new QVBoxLayout;
    auto *title = new QLabel(tr8("结果小框测试"));
    title->setFont(appFont(13, QFont::DemiBold));
    auto *hint = new QLabel(tr8("弹出一次真实结果小框，检查按钮、布局和关闭效果。"));
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: #667085;"));
    labels->addWidget(title);
    labels->addWidget(hint);

    auto *button = new QPushButton(tr8("开始测试"));
    button->setMinimumSize(96, 36);
    button->setStyleSheet(buttonStyle(QStringLiteral("#111827")));
    connect(button, &QPushButton::clicked, this, [this]() {
        showPreview();
    });

    layout->addLayout(labels, 1);
    layout->addWidget(button, 0, Qt::AlignTop);
}

void ResultPopupTestCard::showPreview()
{
    if (m_previewCallback) {
        m_previewCallback(this);
    }
}
