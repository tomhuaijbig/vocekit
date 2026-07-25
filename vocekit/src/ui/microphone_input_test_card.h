#ifndef VOCEKIT_MICROPHONE_INPUT_TEST_CARD_H
#define VOCEKIT_MICROPHONE_INPUT_TEST_CARD_H

#include <QFrame>

#include <functional>

class AudioRecorder;
class QCheckBox;
class QLabel;
class QPushButton;

class MicrophoneInputTestCard : public QFrame
{
public:
    explicit MicrophoneInputTestCard(
        const std::function<QString()> &recordDirectoryPath,
        QWidget *parent = nullptr
    );
    ~MicrophoneInputTestCard();

private:
    void startTest();
    void finishTest(bool keepSample);
    QString sampleDirectory(bool keepSample) const;

    std::function<QString()> m_recordDirectoryPath;
    QLabel *m_result = nullptr;
    QPushButton *m_button = nullptr;
    QCheckBox *m_keepSampleBox = nullptr;
    AudioRecorder *m_recorder = nullptr;
    bool m_testing = false;
};

#endif // VOCEKIT_MICROPHONE_INPUT_TEST_CARD_H
