#include "network_diagnostics_card.h"

#include "../tasks/diagnostic_task_runner.h"
#include "../tasks/network_diagnostics_task.h"
#include "diagnostic_action_card.h"
#include "ui_style.h"

#include <QtWidgets>

namespace {

QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

} // namespace

NetworkDiagnosticsCard::NetworkDiagnosticsCard(
    const std::function<bool()> &useSystemProxy,
    const std::function<SecretConfig()> &secrets,
    QWidget *parent
)
    : QFrame(parent),
      m_useSystemProxy(useSystemProxy),
      m_secrets(secrets)
{
    setObjectName(QStringLiteral("card"));
    setProperty(
        "testSearchText",
        tr8("网络诊断 系统代理 环境代理 DNS TUN 透明代理 DeepSeek 百度 讯飞 OpenAI Claude 自定义接口")
    );
    setStyleSheet(cardStyle());

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);

    auto *top = new QHBoxLayout;
    auto *labels = new QVBoxLayout;
    auto *name = new QLabel(tr8("网络诊断"));
    name->setFont(appFont(13, QFont::DemiBold));
    auto *hint = new QLabel(tr8("检查网络模式、系统代理、环境代理、DNS 解析和核心接口连通性。"));
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: #667085;"));
    labels->addWidget(name);
    labels->addWidget(hint);

    m_button = new QPushButton(tr8("开始测试"));
    m_button->setMinimumSize(96, 36);
    m_button->setStyleSheet(buttonStyle(QStringLiteral("#111827")));
    connect(m_button, &QPushButton::clicked, this, [this]() {
        runTest();
    });

    top->addLayout(labels, 1);
    top->addWidget(m_button, 0, Qt::AlignTop);
    layout->addLayout(top);

    m_result = new QLabel;
    m_result->setWordWrap(true);
    m_result->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_result->setVisible(false);
    m_result->setStyleSheet(QStringLiteral(
        "QLabel { background: #f2f4f7; color: #344054; border-radius: 6px; padding: 10px; }"
    ));
    layout->addWidget(m_result);

    m_runner = new DiagnosticTaskRunner(this);
    m_runner->finishedCallback = [this](const QStringList &lines) {
        if (m_result) {
            showDiagnosticResult(
                m_result,
                lines.join(QStringLiteral("\n\n"))
            );
        }
        if (m_button) {
            m_button->setEnabled(true);
        }
    };
}

void NetworkDiagnosticsCard::runTest()
{
    if (!m_button || !m_result) {
        return;
    }
    m_button->setEnabled(false);
    showDiagnosticResult(m_result, tr8("正在诊断网络，请稍等..."));

    NetworkDiagnosticsRequest request;
    request.useSystemProxy = m_useSystemProxy ? m_useSystemProxy() : false;
    request.secrets = m_secrets ? m_secrets() : SecretConfig();

    if (!m_runner) {
        m_button->setEnabled(true);
        return;
    }
    m_runner->start([request](const CancellationToken &cancellation) mutable {
        request.cancellation = cancellation;
        return runNetworkDiagnosticsTask(request);
    });
}

void NetworkDiagnosticsCard::cancelCheck()
{
    if (!m_runner || !m_runner->isBusy()) {
        return;
    }
    m_runner->cancel();
    if (m_button) {
        m_button->setEnabled(true);
    }
    if (m_result) {
        showDiagnosticResult(m_result, tr8("测试已取消。"));
    }
}
