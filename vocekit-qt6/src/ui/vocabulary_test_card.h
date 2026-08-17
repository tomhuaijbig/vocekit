#ifndef VOCEKIT_VOCABULARY_TEST_CARD_H
#define VOCEKIT_VOCABULARY_TEST_CARD_H

#include "../tasks/vocabulary_diagnostic_task.h"

#include <QFrame>

#include <functional>

class QLabel;
class QPushButton;

class VocabularyTestCard : public QFrame
{
public:
    typedef std::function<VocabularyDiagnosticRequest()> RequestProvider;

    explicit VocabularyTestCard(
        const RequestProvider &requestProvider,
        QWidget *parent = nullptr
    );

private:
    void runTest();

    RequestProvider m_requestProvider;
    QPushButton *m_button = nullptr;
    QLabel *m_result = nullptr;
};

#endif // VOCEKIT_VOCABULARY_TEST_CARD_H
