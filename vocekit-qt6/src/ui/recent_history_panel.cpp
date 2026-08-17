#include "recent_history_panel.h"

#include "tab_bar_wheel_filter.h"
#include "ui_style.h"

#include <QtWidgets>

namespace {

QString text8(const char *text)
{
    return QString::fromUtf8(text);
}

} // namespace

RecentHistoryPanel::RecentHistoryPanel(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("card"));
    setStyleSheet(cardStyle());

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(10);

    auto *title = new QLabel(text8("最近记录"));
    title->setFont(appFont(15, QFont::DemiBold));
    title->setStyleSheet(QStringLiteral(
        "background: transparent; color: #111827;"
    ));
    layout->addWidget(title);

    m_tabs = new QTabWidget;
    configureTabs(m_tabs);
    layout->addWidget(m_tabs, 1);
}

void RecentHistoryPanel::setEntriesProvider(
    const EntriesProvider &provider)
{
    m_entriesProvider = provider;
}

void RecentHistoryPanel::setTabsProvider(const TabsProvider &provider)
{
    m_tabsProvider = provider;
}

void RecentHistoryPanel::setListFactory(const ListFactory &factory)
{
    m_listFactory = factory;
}

void RecentHistoryPanel::reload()
{
    if (!m_tabs || !m_entriesProvider || !m_tabsProvider || !m_listFactory) {
        return;
    }

    const int previousIndex = qMax(0, m_tabs->currentIndex());
    m_tabs->clear();

    const QVector<HistoryEntry> entries = m_entriesProvider();
    const QVector<TabSpec> tabs = m_tabsProvider();
    for (const TabSpec &tab : tabs) {
        m_tabs->addTab(m_listFactory(tab.id, entries, 8), tab.title);
    }
    if (m_tabs->count() > 0) {
        m_tabs->setCurrentIndex(qMin(previousIndex, m_tabs->count() - 1));
    }
}

void RecentHistoryPanel::configureTabs(QTabWidget *tabs)
{
    if (!tabs || !tabs->tabBar()) {
        return;
    }
    tabs->setTabPosition(QTabWidget::North);
    tabs->setElideMode(Qt::ElideNone);
    tabs->tabBar()->setExpanding(false);
    tabs->tabBar()->setUsesScrollButtons(true);
    tabs->tabBar()->setElideMode(Qt::ElideNone);
    tabs->tabBar()->installEventFilter(new TabBarWheelFilter(tabs->tabBar()));
    tabs->setStyleSheet(QStringLiteral(
        "QTabWidget::pane { border: none; }"
        "QTabBar::tab { padding: 8px 12px; color: #667085; }"
        "QTabBar::tab:selected { color: #111827; font-weight: 600; }"
        "QTabBar QToolButton { width: 26px; background: #ffffff; "
        "border: 1px solid #d0d5dd; color: #111827; }"
        "QTabBar QToolButton:hover { background: #eef2ff; }"
    ));
}
