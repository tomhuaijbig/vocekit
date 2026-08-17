#ifndef VOCEKIT_LOGS_PANEL_H
#define VOCEKIT_LOGS_PANEL_H

#include "log_pagination_snapshot.h"

#include <QWidget>

#include <QStringList>

class QLabel;
class QComboBox;
class QLineEdit;
class QScrollArea;
class QVBoxLayout;

class LogsPanel : public QWidget
{
public:
    explicit LogsPanel(
        const LogPaginationSnapshot &pagination,
        QWidget *parent = nullptr
    );

    void setPaginationSnapshot(const LogPaginationSnapshot &pagination);
    void reload(bool reloadFromDisk = true);

private:
    QStringList recentRuntimeLogLines(int maxLines = 0) const;
    QWidget *logLineCard(const QString &line);
    void showRuntimeLogDetail(const QString &line);
    void appendMoreLogLines(int count);
    bool logMatchesSelectedFilter(const QString &line) const;
    void clearList();

    LogPaginationSnapshot m_pagination;
    QLineEdit *m_searchEdit = nullptr;
    QComboBox *m_filterBox = nullptr;
    QVBoxLayout *m_listLayout = nullptr;
    QLabel *m_emptyLabel = nullptr;
    QLabel *m_loadHintLabel = nullptr;
    QScrollArea *m_scrollArea = nullptr;
    QStringList m_linesCache;
    QStringList m_filteredLines;
    int m_visibleCount = 0;
    bool m_appending = false;
};

#endif // VOCEKIT_LOGS_PANEL_H
