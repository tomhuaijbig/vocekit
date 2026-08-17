#ifndef VOCEKIT_RESULT_POPUP_TEST_CARD_H
#define VOCEKIT_RESULT_POPUP_TEST_CARD_H

#include <QFrame>

#include <functional>

class ResultPopupTestCard : public QFrame
{
public:
    typedef std::function<void(QWidget *)> PreviewCallback;

    explicit ResultPopupTestCard(
        const PreviewCallback &previewCallback,
        QWidget *parent = nullptr
    );

private:
    void showPreview();

    PreviewCallback m_previewCallback;
};

#endif // VOCEKIT_RESULT_POPUP_TEST_CARD_H
