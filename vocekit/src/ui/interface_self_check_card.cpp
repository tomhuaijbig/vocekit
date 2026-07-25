#include "interface_self_check_card.h"

#include "diagnostic_action_card.h"
#include "ui_style.h"
#include "../tasks/diagnostic_task_runner.h"
#include "../tasks/interface_self_check_task.h"

#include <QtWidgets>

namespace {

QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

} // namespace

InterfaceSelfCheckCard::InterfaceSelfCheckCard(
    const std::function<bool()> &useSystemProxy,
    const std::function<QString()> &ocrEngine,
    const std::function<int()> &ocrTimeoutMs,
    const std::function<QString()> &applicationBasePath,
    const std::function<SecretConfig()> &secrets,
    QWidget *parent
)
    : QFrame(parent),
      m_useSystemProxy(useSystemProxy),
      m_ocrEngine(ocrEngine),
      m_ocrTimeoutMs(ocrTimeoutMs),
      m_applicationBasePath(applicationBasePath),
      m_secrets(secrets)
{
    setObjectName(QStringLiteral("card"));
    setProperty(
        "testSearchText",
        tr8("接口自检 百度 讯飞 自定义语音 图片识别 OCR DeepSeek OpenAI Claude 自定义大模型 单项测试")
    );
    setStyleSheet(cardStyle());

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);

    auto *top = new QHBoxLayout;
    auto *name = new QLabel(tr8("接口自检"));
    name->setFont(appFont(13, QFont::DemiBold));

    m_targetBox = new QComboBox;
    m_targetBox->setMinimumSize(190, 36);
    m_targetBox->setStyleSheet(QStringLiteral(
        "QComboBox { background: #ffffff; border: 1px solid #d0d5dd; border-radius: 6px; padding: 6px 10px; }"
    ));

    m_button = new QPushButton(tr8("开始测试"));
    m_button->setFixedHeight(36);
    m_button->setStyleSheet(buttonStyle(QStringLiteral("#111827")));
    connect(m_button, &QPushButton::clicked, this, [this]() {
        runCheck();
    });

    top->addWidget(name, 1);
    top->addWidget(m_targetBox);
    top->addWidget(m_button);
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

    refreshTargets();
}

void InterfaceSelfCheckCard::refreshTargets()
{
    if (!m_targetBox) {
        return;
    }
    const QString previous = m_targetBox->currentData().toString();
    m_targetBox->clear();
    m_targetBox->addItem(tr8("全部已配置接口"), QStringLiteral("all"));
    m_targetBox->addItem(tr8("百度语音识别"), QStringLiteral("baidu"));
    m_targetBox->addItem(tr8("讯飞语音听写"), QStringLiteral("xfyun"));
    m_targetBox->addItem(tr8("自定义语音接口"), QStringLiteral("custom_speech"));
    m_targetBox->addItem(tr8("当前图片识别接口"), QStringLiteral("ocr"));
    m_targetBox->addItem(QStringLiteral("DeepSeek"), QStringLiteral("deepseek"));
    m_targetBox->addItem(QStringLiteral("OpenAI"), QStringLiteral("openai"));
    m_targetBox->addItem(QStringLiteral("Claude"), QStringLiteral("claude"));

    const SecretConfig secrets = m_secrets ? m_secrets() : SecretConfig();
    const QVector<CustomModelProfile> profiles = secrets.effectiveCustomModels();
    for (const CustomModelProfile &profile : profiles) {
        m_targetBox->addItem(
            profile.name.trimmed().isEmpty() ? tr8("自定义大模型") : profile.name.trimmed(),
            QStringLiteral("custom:") + profile.id
        );
    }
    const int previousIndex = m_targetBox->findData(previous);
    m_targetBox->setCurrentIndex(previousIndex >= 0 ? previousIndex : 0);
}

void InterfaceSelfCheckCard::runCheck()
{
    if (!m_result || !m_button) {
        return;
    }
    refreshTargets();
    m_button->setEnabled(false);
    showDiagnosticResult(m_result, tr8("正在自检接口，请稍等..."));

    InterfaceSelfCheckRequest request;
    request.useSystemProxy = m_useSystemProxy ? m_useSystemProxy() : false;
    request.target = m_targetBox
        ? m_targetBox->currentData().toString()
        : QStringLiteral("all");
    request.ocrEngine = m_ocrEngine ? m_ocrEngine() : QString();
    request.ocrTimeoutMs = m_ocrTimeoutMs ? m_ocrTimeoutMs() : 45000;
    request.applicationBasePath = m_applicationBasePath ? m_applicationBasePath() : QString();
    request.secrets = m_secrets ? m_secrets() : SecretConfig();

    if (!m_runner) {
        m_button->setEnabled(true);
        return;
    }
    m_runner->start([request](const CancellationToken &cancellation) mutable {
        request.cancellation = cancellation;
        return runInterfaceSelfCheckTask(request);
    });
}

void InterfaceSelfCheckCard::cancelCheck()
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
