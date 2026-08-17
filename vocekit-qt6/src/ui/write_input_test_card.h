#ifndef VOCEKIT_WRITE_INPUT_TEST_CARD_H
#define VOCEKIT_WRITE_INPUT_TEST_CARD_H

#include <QFrame>

class QTextEdit;

class WriteInputTestCard : public QFrame
{
public:
    explicit WriteInputTestCard(QWidget *parent = nullptr);

private:
    void resetText();
    void insertAtCursor();
    void replaceSelection();

    QTextEdit *m_edit = nullptr;
};

#endif // VOCEKIT_WRITE_INPUT_TEST_CARD_H
