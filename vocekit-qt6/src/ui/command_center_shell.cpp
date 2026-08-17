#include "command_center_shell.h"

#include "command_search_router.h"

#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {

QString uiText(const char *text)
{
    return QString::fromUtf8(text);
}

QVector<QPair<QString, QString>> toolItems()
{
    return {
        qMakePair(QStringLiteral("history"), uiText("历史")),
        qMakePair(QStringLiteral("vocabulary"), uiText("词库")),
        qMakePair(QStringLiteral("ocr"), uiText("图片识别")),
        qMakePair(QStringLiteral("prompts"), uiText("提示词")),
        qMakePair(QStringLiteral("diagnostics"), uiText("测试")),
        qMakePair(QStringLiteral("logs"), uiText("日志")),
        qMakePair(QStringLiteral("settings"), uiText("设置")),
        qMakePair(QStringLiteral("faq"), uiText("帮助"))
    };
}

} // namespace

CommandCenterShell::CommandCenterShell(
    const CommandCenterShellAccess &access,
    QWidget *pages,
    QWidget *parent
)
    : QWidget(parent)
    , m_access(access)
{
    setObjectName(QStringLiteral("central"));
    setStyleSheet(QStringLiteral(
        "QWidget#central { background: #f6f7f9; color: #111827; }"
        "QLabel { background: transparent; }"
    ));

    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(sidebar());

    auto *content = new QFrame;
    content->setObjectName(QStringLiteral("commandShell"));
    content->setStyleSheet(QStringLiteral(
        "QFrame#commandShell { background: #eef1f5; border: none; }"
    ));
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);
    contentLayout->addWidget(toolbar());
    contentLayout->addWidget(pages ? pages : new QWidget, 1);
    root->addWidget(content, 1);

    refreshFunctions();
}

QWidget *CommandCenterShell::sidebar()
{
    auto *panel = new QFrame;
    panel->setObjectName(QStringLiteral("commandSidebar"));
    panel->setFixedWidth(176);
    panel->setStyleSheet(commandCenterSidebarStyle());

    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(10, 16, 10, 14);
    layout->setSpacing(4);

    auto *brand = new QLabel(QStringLiteral("vocekit"));
    brand->setObjectName(QStringLiteral("commandBrand"));
    brand->setMinimumHeight(38);
    brand->setContentsMargins(10, 0, 0, 0);
    layout->addWidget(brand);

    auto *divider = new QFrame;
    divider->setFrameShape(QFrame::HLine);
    divider->setStyleSheet(QStringLiteral("color: #2a3547;"));
    layout->addWidget(divider);

    CommandCenterFunctionItem home;
    home.id = QStringLiteral("home");
    home.title = uiText("主页");
    auto *homeButton = functionButton(home);
    homeButton->setToolTip(uiText("返回主页"));
    layout->addWidget(homeButton);

    auto *functionScroll = new QScrollArea;
    functionScroll->setObjectName(QStringLiteral("commandFunctionScroll"));
    functionScroll->setWidgetResizable(true);
    functionScroll->setFrameShape(QFrame::NoFrame);
    functionScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    functionScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    functionScroll->setFocusPolicy(Qt::WheelFocus);

    auto *functionHolder = new QWidget;
    m_functionLayout = new QVBoxLayout(functionHolder);
    m_functionLayout->setContentsMargins(0, 0, 4, 0);
    m_functionLayout->setSpacing(4);
    m_functionLayout->setAlignment(Qt::AlignTop);
    functionScroll->setWidget(functionHolder);
    layout->addWidget(functionScroll, 1);
    return panel;
}

QWidget *CommandCenterShell::toolbar()
{
    auto *bar = new QFrame;
    bar->setFixedHeight(58);
    bar->setStyleSheet(QStringLiteral(
        "QFrame { background: #101827; border: none; }"
    ));
    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(24, 0, 18, 0);
    layout->setSpacing(6);

    m_searchEdit = new QLineEdit;
    m_searchEdit->setMinimumHeight(34);
    m_searchEdit->setMaximumWidth(330);
    m_searchEdit->setPlaceholderText(uiText("搜索功能、设置或历史记录"));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setStyleSheet(commandCenterSearchStyle());
    connect(m_searchEdit, &QLineEdit::returnPressed, this, [this]() {
        const QString keyword = m_searchEdit->text().trimmed();
        if (keyword.isEmpty()) {
            return;
        }

        QVector<CommandSearchEntry> functions;
        const QVector<CommandCenterFunctionItem> items = m_access.functionsProvider
            ? m_access.functionsProvider()
            : QVector<CommandCenterFunctionItem>();
        for (const CommandCenterFunctionItem &item : items) {
            CommandSearchEntry entry;
            entry.id = item.id;
            entry.title = item.title;
            functions.append(entry);
        }

        const CommandSearchResult result = CommandSearchRouter::resolve(
            keyword,
            functions,
            CommandSearchRouter::defaultPages()
        );
        if (result.type == CommandSearchTargetType::Function && m_access.openFunction) {
            m_access.openFunction(result.id);
        } else if (result.type == CommandSearchTargetType::Page && m_access.openTool) {
            m_access.openTool(result.id);
        } else if (m_access.searchMissed) {
            m_access.searchMissed(keyword);
        }
    });
    layout->addWidget(m_searchEdit, 1);
    layout->addStretch();

    const QVector<QPair<QString, QString>> tools = toolItems();
    for (const auto &tool : tools) {
        layout->addWidget(toolButton(tool.first, tool.second));
    }
    return bar;
}

QPushButton *CommandCenterShell::functionButton(const CommandCenterFunctionItem &item)
{
    auto *button = new QPushButton(item.title);
    button->setFixedHeight(38);
    button->setCursor(Qt::PointingHandCursor);
    button->setProperty("functionId", item.id);
    button->setToolTip(item.shortcut);
    connect(button, &QPushButton::clicked, this, [this, item]() {
        if (m_access.openFunction) {
            m_access.openFunction(item.id);
        }
    });
    m_functionButtons.insert(item.id, button);
    return button;
}

QPushButton *CommandCenterShell::addFunctionButton()
{
    auto *button = new QPushButton(uiText("新增功能"));
    button->setObjectName(QStringLiteral("commandAddFunctionButton"));
    button->setFixedHeight(44);
    button->setCursor(Qt::PointingHandCursor);
    button->setStyleSheet(commandCenterFunctionButtonStyle(false, true));
    connect(button, &QPushButton::clicked, this, [this]() {
        if (m_access.addFunction) {
            m_access.addFunction();
        }
    });
    return button;
}

QPushButton *CommandCenterShell::toolButton(const QString &id, const QString &title)
{
    auto *button = new QPushButton(title);
    button->setMinimumHeight(50);
    button->setMinimumWidth(qMax(54, button->fontMetrics().horizontalAdvance(title) + 20));
    button->setCursor(Qt::PointingHandCursor);
    button->setProperty("toolId", id);
    connect(button, &QPushButton::clicked, this, [this, id]() {
        if (m_access.openTool) {
            m_access.openTool(id);
        }
    });
    m_toolButtons.insert(id, button);
    return button;
}

void CommandCenterShell::clearFunctionLayout()
{
    if (!m_functionLayout) {
        return;
    }
    while (QLayoutItem *item = m_functionLayout->takeAt(0)) {
        if (item->widget()) {
            item->widget()->hide();
            item->widget()->deleteLater();
        }
        delete item;
    }
}

void CommandCenterShell::refreshFunctions()
{
    if (!m_functionLayout) {
        return;
    }
    for (auto it = m_functionButtons.begin(); it != m_functionButtons.end();) {
        if (it.key() == QStringLiteral("home")) {
            ++it;
        } else {
            it = m_functionButtons.erase(it);
        }
    }
    clearFunctionLayout();

    const QVector<CommandCenterFunctionItem> functions = m_access.functionsProvider
        ? m_access.functionsProvider()
        : QVector<CommandCenterFunctionItem>();
    for (const CommandCenterFunctionItem &item : functions) {
        m_functionLayout->addWidget(functionButton(item));
    }
    m_functionLayout->addWidget(addFunctionButton());
    m_functionLayout->addStretch();
    refreshButtonStyles();
}

void CommandCenterShell::setActivePage(const QString &pageId, const QString &functionId)
{
    m_activePageId = pageId;
    m_activeFunctionId = functionId;
    refreshButtonStyles();
}

QString CommandCenterShell::activePageId() const
{
    return m_activePageId;
}

QString CommandCenterShell::activeFunctionId() const
{
    return m_activeFunctionId;
}

void CommandCenterShell::refreshButtonStyles()
{
    for (auto it = m_toolButtons.constBegin(); it != m_toolButtons.constEnd(); ++it) {
        it.value()->setStyleSheet(commandCenterToolButtonStyle(it.key() == m_activePageId));
    }
    for (auto it = m_functionButtons.constBegin(); it != m_functionButtons.constEnd(); ++it) {
        const bool active = m_activePageId == QStringLiteral("home")
            ? it.key() == QStringLiteral("home")
            : m_activePageId == QStringLiteral("function")
                && it.key() == m_activeFunctionId;
        it.value()->setStyleSheet(commandCenterFunctionButtonStyle(active));
    }
}

QString commandCenterSidebarStyle()
{
    return QStringLiteral(
        "QFrame#commandSidebar { background: #101827; border: none; }"
        "QLabel#commandBrand { color: #ffffff; font-size: 20px; font-weight: 700; }"
        "QLabel#commandSectionLabel { color: #77869d; font-size: 9pt; padding: 8px 10px 2px; }"
        "QScrollArea { background: transparent; border: none; }"
        "QScrollArea > QWidget > QWidget { background: transparent; }"
        "QScrollBar:vertical { background: transparent; width: 6px; margin: 0; }"
        "QScrollBar::handle:vertical { background: #44516a; min-height: 28px; border-radius: 3px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }"
    );
}

QString commandCenterFunctionButtonStyle(bool active, bool addButton)
{
    const QString background = active ? QStringLiteral("#26344b") : QStringLiteral("transparent");
    const QString color = active ? QStringLiteral("#ffffff") : QStringLiteral("#c5cede");
    const QString border = addButton
        ? QStringLiteral("1px dashed #536078")
        : QStringLiteral("1px solid transparent");
    return QStringLiteral(
        "QPushButton {"
        "  text-align: left;"
        "  background: %1;"
        "  color: %2;"
        "  border: %3;"
        "  border-radius: 4px;"
        "  padding: 0 10px;"
        "  font-weight: %4;"
        "}"
        "QPushButton:hover { background: #1d293c; color: #ffffff; }"
        "QPushButton:pressed { background: #2c3b54; }"
        "QPushButton:focus { outline: none; }"
    ).arg(
        background,
        color,
        border,
        active ? QStringLiteral("600") : QStringLiteral("400")
    );
}

QString commandCenterToolButtonStyle(bool active)
{
    return QStringLiteral(
        "QPushButton {"
        "  background: %1;"
        "  color: %2;"
        "  border: none;"
        "  border-bottom: 2px solid %3;"
        "  padding: 0 10px;"
        "  font-weight: %4;"
        "}"
        "QPushButton:hover { background: #223048; color: #ffffff; }"
        "QPushButton:pressed { background: #2b3a55; }"
        "QPushButton:focus { outline: none; }"
    ).arg(
        active ? QStringLiteral("#263854") : QStringLiteral("transparent"),
        active ? QStringLiteral("#ffffff") : QStringLiteral("#c8d1df"),
        active ? QStringLiteral("#245fc4") : QStringLiteral("transparent"),
        active ? QStringLiteral("600") : QStringLiteral("400")
    );
}

QString commandCenterSearchStyle()
{
    return QStringLiteral(
        "QLineEdit {"
        "  background: transparent;"
        "  color: #d5ddea;"
        "  border: 1px solid transparent;"
        "  border-radius: 4px;"
        "  padding: 0 10px;"
        "  selection-background-color: #245fc4;"
        "}"
        "QLineEdit:hover { background: #172238; }"
        "QLineEdit:focus { background: #172238; border-color: #52637e; outline: none; }"
    );
}

QString commandCenterSectionStyle()
{
    return QStringLiteral(
        "QFrame#commandSection { background: #ffffff; border: 1px solid #d7dee9; border-radius: 4px; }"
        "QFrame#commandMethod { background: #ffffff; border: 1px solid #d7dee9; border-radius: 4px; }"
        "QFrame#commandMethod:hover { border-color: #91afe8; background: #fbfdff; }"
        "QLabel#commandMuted { color: #687386; }"
        "QLabel#commandStateOn { color: #174793; background: #edf3ff; border: 1px solid #b8cbee;"
        "  border-radius: 4px; padding: 3px 7px; font-weight: 600; }"
        "QLabel#commandStateOff { color: #687386; background: #f7f9fc; border: 1px solid #d7dee9;"
        "  border-radius: 4px; padding: 3px 7px; }"
    );
}
