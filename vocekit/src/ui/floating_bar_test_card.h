#ifndef VOCEKIT_FLOATING_BAR_TEST_CARD_H
#define VOCEKIT_FLOATING_BAR_TEST_CARD_H

#include <QFrame>

#include <functional>

class FloatingBar;
class QLabel;
class QComboBox;
class QTimer;

class FloatingBarTestCard : public QFrame
{
public:
    explicit FloatingBarTestCard(
        const std::function<bool()> &floatingBarEnabled,
        const std::function<int()> &dictateFloatingBarSeconds,
        FloatingBar *floatingBar,
        QWidget *parent = nullptr
    );

private:
    void runTest();
    void stopPreviewTimer();
    void startRecordingPreview();
    void startTimedPreview(const QString &title, int stage);

    std::function<bool()> m_floatingBarEnabled;
    std::function<int()> m_dictateFloatingBarSeconds;
    FloatingBar *m_floatingBar = nullptr;
    QComboBox *m_stageBox = nullptr;
    QLabel *m_result = nullptr;
    QTimer *m_previewTimer = nullptr;
    int m_previewTick = 0;
};

#endif // VOCEKIT_FLOATING_BAR_TEST_CARD_H
