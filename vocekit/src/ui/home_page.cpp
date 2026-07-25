#include "home_page.h"

#include "current_status_panel.h"
#include "ui_style.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QSizePolicy>
#include <QVBoxLayout>

HomePage::HomePage(const HomePageAccess &access, QWidget *parent)
    : QWidget(parent)
    , m_access(access)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(18);

    auto *title = new QLabel(QStringLiteral("vocekit"));
    title->setFont(appFont(24, QFont::DemiBold));
    title->setMinimumHeight(title->fontMetrics().height() + 8);
    layout->addWidget(title);

    auto *modeScroll = new QScrollArea;
    modeScroll->setWidgetResizable(true);
    modeScroll->setFrameShape(QFrame::NoFrame);
    modeScroll->setMinimumHeight(320);
    modeScroll->setMaximumHeight(500);
    modeScroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    modeScroll->setFocusPolicy(Qt::WheelFocus);
    modeScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    modeScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    modeScroll->setStyleSheet(QStringLiteral(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollArea > QWidget > QWidget { background: transparent; }"
    ));

    m_functionModes = new FunctionModeGrid(m_access.functionModes);
    m_functionModes->setEditCallback(m_access.editFunction);
    m_functionModes->setSettingsChangedCallback(m_access.settingsChanged);
    m_functionModes->setWarningCallback(m_access.showWarning);
    m_functionModes->refresh();
    modeScroll->setWidget(m_functionModes);
    layout->addWidget(modeScroll, 1);

    auto *summary = new QHBoxLayout;
    summary->setSpacing(14);

    m_recentHistory = new RecentHistoryPanel;
    m_recentHistory->setEntriesProvider(m_access.recentEntries);
    m_recentHistory->setTabsProvider(m_access.recentTabs);
    m_recentHistory->setListFactory(m_access.recentListFactory);
    m_recentHistory->reload();
    summary->addWidget(m_recentHistory, 2);

    const CurrentStatusSnapshot status = m_access.currentStatus
        ? m_access.currentStatus()
        : CurrentStatusSnapshot();
    m_currentStatus = new CurrentStatusPanel(status);
    summary->addWidget(m_currentStatus, 1);

    layout->addLayout(summary, 2);
}

void HomePage::refreshFunctionModes()
{
    if (m_functionModes) {
        m_functionModes->refresh();
    }
}

void HomePage::refreshRecentHistory()
{
    if (m_recentHistory) {
        m_recentHistory->reload();
    }
}

void HomePage::refreshCurrentStatus()
{
    if (m_currentStatus && m_access.currentStatus) {
        m_currentStatus->refresh(m_access.currentStatus());
    }
}
