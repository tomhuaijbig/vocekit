#ifndef VOCEKIT_NETWORK_DIAGNOSTICS_CARD_H
#define VOCEKIT_NETWORK_DIAGNOSTICS_CARD_H

#include "../config/secret_config.h"

#include <QFrame>

#include <functional>

class QLabel;
class QPushButton;
class DiagnosticTaskRunner;

class NetworkDiagnosticsCard : public QFrame
{
public:
    explicit NetworkDiagnosticsCard(
        const std::function<bool()> &useSystemProxy,
        const std::function<SecretConfig()> &secrets,
        QWidget *parent = nullptr
    );

    void cancelCheck();

private:
    void runTest();

    std::function<bool()> m_useSystemProxy;
    std::function<SecretConfig()> m_secrets;
    QPushButton *m_button = nullptr;
    QLabel *m_result = nullptr;
    DiagnosticTaskRunner *m_runner = nullptr;
};

#endif // VOCEKIT_NETWORK_DIAGNOSTICS_CARD_H
