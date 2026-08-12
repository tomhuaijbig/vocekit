#ifndef VOCEKIT_WINDOWS_SPEECH_SETTINGS_CARD_H
#define VOCEKIT_WINDOWS_SPEECH_SETTINGS_CARD_H

#include "../tasks/cancellation_token.h"

#include <QFrame>
#include <QStringList>

#include <functional>

class DiagnosticTaskRunner;
class QComboBox;
class QLabel;
class QPushButton;

struct WindowsSpeechSettingsCardCallbacks
{
    std::function<QStringList(const QString &, const CancellationToken &)> probe;
    std::function<void()> openWindowsLanguageSettings;
};

class WindowsSpeechSettingsCard : public QFrame
{
public:
    explicit WindowsSpeechSettingsCard(
        const WindowsSpeechSettingsCardCallbacks &callbacks,
        QWidget *parent = nullptr
    );

    QString language() const;
    void setLanguage(const QString &language);
    void cancelProbe();

private:
    void runProbe();
    void openLanguageSettings();
    void publishProbeResult(const QStringList &lines);

    WindowsSpeechSettingsCardCallbacks m_callbacks;
    QComboBox *m_languageBox = nullptr;
    QPushButton *m_testButton = nullptr;
    QPushButton *m_openSettingsButton = nullptr;
    QLabel *m_resultLabel = nullptr;
    DiagnosticTaskRunner *m_runner = nullptr;
};

#endif // VOCEKIT_WINDOWS_SPEECH_SETTINGS_CARD_H
