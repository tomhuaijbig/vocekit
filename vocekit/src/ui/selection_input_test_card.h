#ifndef VOCEKIT_SELECTION_INPUT_TEST_CARD_H
#define VOCEKIT_SELECTION_INPUT_TEST_CARD_H

#include <QFrame>
#include <QPointer>

#include <functional>

class FloatingBar;
class QLabel;
class QPushButton;
class QTimer;

class SelectionInputTestCard : public QFrame
{
public:
    explicit SelectionInputTestCard(
        const std::function<bool()> &floatingBarEnabled,
        FloatingBar *floatingBar,
        QWidget *hostWindow,
        QWidget *parent = nullptr
    );

private:
    void start(bool strong);
    void finish();
    void setButtonsEnabled(bool enabled);

    std::function<bool()> m_floatingBarEnabled;
    FloatingBar *m_floatingBar = nullptr;
    QPointer<QWidget> m_hostWindow;
    QLabel *m_result = nullptr;
    QPushButton *m_normalButton = nullptr;
    QPushButton *m_strongButton = nullptr;
    QTimer *m_timer = nullptr;
    int m_seconds = 0;
    bool m_strong = false;
};

#endif // VOCEKIT_SELECTION_INPUT_TEST_CARD_H
