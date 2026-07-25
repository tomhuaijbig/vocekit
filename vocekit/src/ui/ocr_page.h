#ifndef VOCEKIT_OCR_PAGE_H
#define VOCEKIT_OCR_PAGE_H

#include <QList>
#include <QString>
#include <QWidget>

#include <functional>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTextEdit;

struct OcrPageCallbacks
{
    std::function<void()> selectImages;
    std::function<void()> startRecognition;
    std::function<void()> cancelRecognition;
    std::function<void()> previousImage;
    std::function<void()> nextImage;
    std::function<void(const QString &)> aiAction;
    std::function<void(const QString &)> resultTextChanged;
};

// OCR page shell. Recognition state and AI processing stay in HubWindow callbacks.
class OcrPage : public QWidget
{
public:
    explicit OcrPage(
        int configuredEngine,
        const OcrPageCallbacks &callbacks,
        QWidget *parent = nullptr
    );

    QLineEdit *imagePathEdit() const { return m_imagePathEdit; }
    QLabel *previewLabel() const { return m_previewLabel; }
    QLabel *statusLabel() const { return m_statusLabel; }
    QComboBox *engineBox() const { return m_engineBox; }
    QTextEdit *resultEdit() const { return m_resultEdit; }
    QPushButton *selectButton() const { return m_selectButton; }
    QPushButton *startButton() const { return m_startButton; }
    QPushButton *cancelButton() const { return m_cancelButton; }
    QPushButton *previousButton() const { return m_previousButton; }
    QPushButton *nextButton() const { return m_nextButton; }
    QLabel *positionLabel() const { return m_positionLabel; }
    QList<QPushButton *> aiButtons() const { return m_aiButtons; }

private:
    OcrPageCallbacks m_callbacks;
    QLineEdit *m_imagePathEdit = nullptr;
    QLabel *m_previewLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QComboBox *m_engineBox = nullptr;
    QTextEdit *m_resultEdit = nullptr;
    QPushButton *m_selectButton = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
    QPushButton *m_previousButton = nullptr;
    QPushButton *m_nextButton = nullptr;
    QLabel *m_positionLabel = nullptr;
    QList<QPushButton *> m_aiButtons;
};

#endif // VOCEKIT_OCR_PAGE_H
