#ifndef VOCEKIT_CURRENT_STATUS_PANEL_H
#define VOCEKIT_CURRENT_STATUS_PANEL_H

#include "current_status_snapshot.h"

#include <QFrame>
#include <QMap>
#include <QString>

class QLabel;
class QVBoxLayout;
class QWidget;

// Home page current-status panel. It owns status row creation and refresh logic.
class CurrentStatusPanel : public QFrame
{
public:
    explicit CurrentStatusPanel(
        const CurrentStatusSnapshot &snapshot,
        QWidget *parent = nullptr
    );

    void refresh(const CurrentStatusSnapshot &snapshot);

private:
    QWidget *statusRow(
        const QString &id,
        const QString &name,
        const QString &value
    );
    void setValue(const QString &id, const QString &value);
    void setRowVisible(const QString &id, bool visible);
    void addConditionalRows(
        QVBoxLayout *items,
        const CurrentStatusSnapshot &snapshot
    );

    QMap<QString, QWidget *> m_statusRows;
    QMap<QString, QLabel *> m_statusValueLabels;
};

#endif // VOCEKIT_CURRENT_STATUS_PANEL_H
