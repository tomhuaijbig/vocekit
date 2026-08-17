#ifndef VOCEKIT_INTERFACE_SELF_CHECK_CARD_H
#define VOCEKIT_INTERFACE_SELF_CHECK_CARD_H

#include "../config/secret_config.h"

#include <QFrame>

#include <functional>

class QLabel;
class QComboBox;
class QPushButton;
class DiagnosticTaskRunner;

class InterfaceSelfCheckCard : public QFrame
{
public:
    explicit InterfaceSelfCheckCard(
        const std::function<bool()> &useSystemProxy,
        const std::function<QString()> &ocrEngine,
        const std::function<QString()> &windowsSpeechLanguage,
        const std::function<int()> &ocrTimeoutMs,
        const std::function<QString()> &applicationBasePath,
        const std::function<QString()> &applicationDirPath,
        const std::function<SecretConfig()> &secrets,
        QWidget *parent = nullptr
    );

    void refreshTargets();
    void cancelCheck();

private:
    void runCheck();

    std::function<bool()> m_useSystemProxy;
    std::function<QString()> m_ocrEngine;
    std::function<QString()> m_windowsSpeechLanguage;
    std::function<int()> m_ocrTimeoutMs;
    std::function<QString()> m_applicationBasePath;
    std::function<QString()> m_applicationDirPath;
    std::function<SecretConfig()> m_secrets;
    QComboBox *m_targetBox = nullptr;
    QPushButton *m_button = nullptr;
    QLabel *m_result = nullptr;
    DiagnosticTaskRunner *m_runner = nullptr;
};

#endif // VOCEKIT_INTERFACE_SELF_CHECK_CARD_H
