#ifndef VOCEKIT_RECENT_HISTORY_PANEL_H
#define VOCEKIT_RECENT_HISTORY_PANEL_H

#include "../domain/history_types.h"

#include <QFrame>

#include <functional>

class QTabWidget;
class QWidget;

// Home page recent-history card. Data loading stays behind callbacks.
class RecentHistoryPanel : public QFrame
{
public:
    struct TabSpec
    {
        QString id;
        QString title;
    };

    using EntriesProvider = std::function<QVector<HistoryEntry>()>;
    using TabsProvider = std::function<QVector<TabSpec>()>;
    using ListFactory = std::function<QWidget *(
        const QString &modeId,
        const QVector<HistoryEntry> &entries,
        int maxRows
    )>;

    explicit RecentHistoryPanel(QWidget *parent = nullptr);

    void setEntriesProvider(const EntriesProvider &provider);
    void setTabsProvider(const TabsProvider &provider);
    void setListFactory(const ListFactory &factory);

    void reload();

private:
    void configureTabs(QTabWidget *tabs);

    QTabWidget *m_tabs = nullptr;
    EntriesProvider m_entriesProvider;
    TabsProvider m_tabsProvider;
    ListFactory m_listFactory;
};

#endif // VOCEKIT_RECENT_HISTORY_PANEL_H
