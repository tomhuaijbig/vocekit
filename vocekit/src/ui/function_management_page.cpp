#include "function_management_page.h"

#include "history_row_frame.h"
#include "ui_style.h"

#include <QtWidgets>

namespace {

QString text8(const char *text)
{
    return QString::fromUtf8(text);
}

} // namespace

FunctionManagementPage::FunctionManagementPage(
    const FunctionManagementPageAccess &access,
    QWidget *parent
)
    : QWidget(parent), m_access(access)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(16);

    auto *top = new QHBoxLayout;
    auto *title = new QLabel(text8("功能自定义"));
    title->setFont(appFont(24, QFont::DemiBold));

    auto *add = new QPushButton(text8("新增功能"));
    add->setFont(appFont(10, QFont::DemiBold));
    add->setFixedSize(112, 42);
    add->setStyleSheet(compactButtonStyle(QStringLiteral("#111827")));
    QObject::connect(add, &QPushButton::clicked, this, [this]() {
        if (m_access.addFunction) {
            m_access.addFunction();
        }
    });

    top->addWidget(title, 1);
    top->addWidget(add);
    layout->addLayout(top);

    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *holder = new QWidget;
    m_listLayout = new QVBoxLayout(holder);
    m_listLayout->setContentsMargins(0, 0, 0, 0);
    m_listLayout->setSpacing(12);
    scroll->setWidget(holder);
    layout->addWidget(scroll, 1);

    refresh();
}

void FunctionManagementPage::refresh()
{
    if (!m_listLayout) {
        return;
    }
    clearLayout(m_listLayout);
    const QVector<FunctionManagementItem> items = m_access.itemsProvider
        ? m_access.itemsProvider()
        : QVector<FunctionManagementItem>();
    for (const FunctionManagementItem &item : items) {
        m_listLayout->addWidget(summaryCard(item));
    }
    m_listLayout->addStretch();
}

QWidget *FunctionManagementPage::summaryCard(
    const FunctionManagementItem &item
)
{
    auto *frame = new HistoryRowFrame;
    frame->setObjectName(QStringLiteral("card"));
    frame->setStyleSheet(cardStyle());
    frame->setClickCallback([this, item]() {
        if (m_access.editFunction) {
            m_access.editFunction(item);
        }
    });

    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);

    auto *top = new QHBoxLayout;
    auto *name = new QLabel(item.title);
    name->setFont(appFont(13, QFont::DemiBold));
    name->setWordWrap(true);
    auto *edit = new QPushButton(text8("编辑"));
    edit->setFont(appFont(10, QFont::DemiBold));
    edit->setFixedSize(92, 42);
    edit->setStyleSheet(compactButtonStyle(QStringLiteral("#111827")));
    top->addWidget(name, 1);
    top->addWidget(edit, 0, Qt::AlignTop);

    QPushButton *remove = nullptr;
    if (item.custom) {
        remove = new QPushButton(text8("删除"));
        remove->setFont(appFont(10, QFont::DemiBold));
        remove->setFixedSize(92, 42);
        remove->setStyleSheet(compactButtonStyle(
            QStringLiteral("#ffffff"),
            QStringLiteral("#111827")
        ));
        top->addWidget(remove, 0, Qt::AlignTop);
    }
    layout->addLayout(top);

    auto *meta = new QLabel(item.summary);
    meta->setWordWrap(true);
    meta->setTextInteractionFlags(Qt::TextSelectableByMouse);
    meta->setStyleSheet(QStringLiteral("color: #047857; font-weight: 600;"));
    layout->addWidget(meta);

    QObject::connect(edit, &QPushButton::clicked, this, [this, item]() {
        if (m_access.editFunction) {
            m_access.editFunction(item);
        }
    });
    if (remove) {
        QObject::connect(remove, &QPushButton::clicked, this, [this, item]() {
            if (QMessageBox::question(
                    this,
                    text8("删除自定义功能"),
                    text8("确定删除“") + item.title + text8("”？")
                ) != QMessageBox::Yes) {
                return;
            }
            if (m_access.removeFunction) {
                m_access.removeFunction(item);
            }
        });
    }
    return frame;
}

void FunctionManagementPage::clearLayout(QLayout *layout)
{
    if (!layout) {
        return;
    }
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        if (QLayout *childLayout = item->layout()) {
            clearLayout(childLayout);
        }
        delete item;
    }
}
