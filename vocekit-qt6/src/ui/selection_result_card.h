#ifndef VOCEKIT_SELECTION_RESULT_CARD_H
#define VOCEKIT_SELECTION_RESULT_CARD_H

#include "../input/selection_snapshot.h"

#include <QPoint>
#include <QRect>
#include <QWidget>

#include <functional>

class QEvent;
class QGridLayout;
class QLabel;
class QLineEdit;
class QPaintEvent;
class QPlainTextEdit;
class QPushButton;
class QTimer;
class QToolButton;
class QVBoxLayout;

struct SelectionResultCardState
{
    QString actionId;
    QString title;
    QString committedText;
    QString provisionalText;
    QString statusText;
    bool running = false;
    bool pinned = false;
    bool replaceEnabled = false;
    bool requiresLongTextConfirmation = false;
};

struct SelectionResultCardCallbacks
{
    std::function<void()> cancelRequested;
    std::function<void()> copyRequested;
    std::function<void()> replaceRequested;
    std::function<void()> regenerateRequested;
    std::function<void(bool)> pinChanged;
    std::function<void()> closeRequested;
    std::function<void(const QString &question)> followUpRequested;
    std::function<void()> processFullTextRequested;
};

class SelectionResultCard : public QWidget
{
    Q_OBJECT

public:
    explicit SelectionResultCard(QWidget *parent = nullptr);
    ~SelectionResultCard() override;

    void setCallbacks(const SelectionResultCardCallbacks &callbacks);
    void setState(const SelectionResultCardState &state);
    SelectionResultCardState state() const;
    void showAt(
        const QPoint &topLeft,
        const QRect &availableGeometry
    );
    void closeIfUnpinned();
    bool ownsNativeWindow(SelectedTextNativeWindowHandle window) const;

protected:
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateControls();
    void scheduleRender();
    void renderNow();
    void relayoutActionButtons();
    void clampToAvailableGeometry();
    void enableFollowUpFocus();
    void restorePassiveWindowMode();
    void invokeAction(const std::function<void()> &callback);
    void invokeTerminal(std::function<void()> &callback);

    SelectionResultCardState m_state;
    SelectionResultCardCallbacks m_callbacks;
    QRect m_availableGeometry;
    QTimer *m_renderTimer = nullptr;
    QLabel *m_title = nullptr;
    QLabel *m_status = nullptr;
    QPlainTextEdit *m_committed = nullptr;
    QPlainTextEdit *m_provisional = nullptr;
    QWidget *m_longTextConfirmation = nullptr;
    QPushButton *m_longTextProcess = nullptr;
    QPushButton *m_longTextCancel = nullptr;
    QGridLayout *m_actionLayout = nullptr;
    QPushButton *m_cancel = nullptr;
    QPushButton *m_regenerate = nullptr;
    QPushButton *m_copy = nullptr;
    QPushButton *m_replace = nullptr;
    QToolButton *m_pin = nullptr;
    QPushButton *m_close = nullptr;
    QLineEdit *m_followUpInput = nullptr;
    QPushButton *m_followUpButton = nullptr;
    bool m_followUpFocusEnabled = false;
};

#endif // VOCEKIT_SELECTION_RESULT_CARD_H
