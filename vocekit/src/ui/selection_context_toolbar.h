#ifndef VOCEKIT_SELECTION_CONTEXT_TOOLBAR_H
#define VOCEKIT_SELECTION_CONTEXT_TOOLBAR_H

#include "../config/selection_context_action_customization.h"
#include "../input/selection_snapshot.h"

#include <QMap>
#include <QPoint>
#include <QVector>
#include <QWidget>

#include <functional>

class QEvent;
class QHBoxLayout;
class QLabel;
class QMenu;
class QPaintEvent;
class QToolButton;

struct SelectionContextToolbarCallbacks
{
    std::function<void(const QString &actionId)> actionRequested;
    std::function<void()> closeRequested;
};

struct SelectionContextMenuItem
{
    QString actionId;
    QString title;
    bool enabled = true;
};

class SelectionContextToolbar : public QWidget
{
    Q_OBJECT

public:
    explicit SelectionContextToolbar(QWidget *parent = nullptr);
    ~SelectionContextToolbar() override;

    void setCallbacks(const SelectionContextToolbarCallbacks &callbacks);
    void setActionPresentation(
        const QStringList &actionIds,
        const SelectionContextActionCustomizationMap &customizations
    );
    void setActionOrder(const QStringList &actionIds);
    void setMoreActions(const QVector<SelectionContextMenuItem> &items);
    void setBusyAction(const QString &actionId);
    void setActionEnabled(const QString &actionId, bool enabled);
    void showForSnapshot(
        const SelectionSnapshot &snapshot,
        const QRect &availableGeometry,
        bool keyboardNavigationMode = false
    );
    void hideToolbar();
    bool ownsNativeWindow(SelectedTextNativeWindowHandle window) const;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void changeEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void applyWindowMode(bool keyboardNavigationMode);
    void applyButtonMetrics();
    void rebuildActionButtons();
    void rebuildActionLayout();
    void rebuildMoreMenu();
    void updateMoreMenuEnabledState();
    void resizeForAvailableGeometry(const QRect &availableGeometry);
    void keepInsideAvailableGeometry(const QRect &availableGeometry);
    void configureForAvailableWidth(int width);
    int visibleRowWidth() const;
    QToolButton *firstVisibleActionButton() const;
    void requestAction(const QString &actionId);
    void requestClose();

    SelectionContextToolbarCallbacks m_callbacks;
    QStringList m_actionOrder;
    QMap<QString, QString> m_actionTitles;
    QVector<SelectionContextMenuItem> m_moreActions;
    QStringList m_overflowActionIds;
    QMap<QString, bool> m_actionEnabled;
    QString m_busyActionId;
    QRect m_availableGeometry;
    QHBoxLayout *m_layout = nullptr;
    QWidget *m_dragHandle = nullptr;
    QLabel *m_identity = nullptr;
    QMap<QString, QToolButton *> m_actionButtons;
    QToolButton *m_moreButton = nullptr;
    QToolButton *m_closeButton = nullptr;
    QMenu *m_moreMenu = nullptr;
    bool m_dragging = false;
    QPoint m_dragOffset;
};

#endif // VOCEKIT_SELECTION_CONTEXT_TOOLBAR_H
