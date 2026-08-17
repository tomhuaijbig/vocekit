#include "windows_speech_settings_card.h"

#include "ui_style.h"
#include "../config/app_settings_defaults.h"
#include "../tasks/diagnostic_task_runner.h"

#include <QComboBox>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

namespace {

QString wsTr8(const char *text)
{
    return QString::fromUtf8(text);
}

bool containsMissingRecognizer(const QStringList &lines)
{
    return lines.value(0).trimmed()
        == QStringLiteral("RECOGNIZER_MISSING");
}

} // namespace

WindowsSpeechSettingsCard::WindowsSpeechSettingsCard(
    const WindowsSpeechSettingsCardCallbacks &callbacks,
    QWidget *parent
)
    : QFrame(parent),
      m_callbacks(callbacks)
{
    setObjectName(QStringLiteral("windowsSpeechSettingsCard"));
    setStyleSheet(QStringLiteral(
        "QFrame#windowsSpeechSettingsCard {"
        " background: #f9fafb; border: 1px solid #eef0f4; border-radius: 6px;"
        "}"
    ));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);

    auto *title = new QLabel(wsTr8("Windows 本地语音识别"));
    title->setObjectName(QStringLiteral("windowsSpeechCardTitle"));
    title->setFont(appFont(11, QFont::DemiBold));
    title->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    layout->addWidget(title);

    auto *hint = new QLabel(wsTr8(
        "使用 Windows 已安装的语音语言，本地识别，不需要接口密钥。"
    ));
    hint->setObjectName(QStringLiteral("windowsSpeechCardHint"));
    hint->setWordWrap(false);
    hint->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    hint->setStyleSheet(QStringLiteral("QLabel { color: #667085; }"));
    layout->addWidget(hint);

    auto *controls = new QHBoxLayout;
    controls->setSpacing(10);
    m_languageBox = new QComboBox;
    m_languageBox->setObjectName(QStringLiteral("windowsSpeechLanguageBox"));
    for (const QString &id : supportedWindowsSpeechLanguages()) {
        m_languageBox->addItem(windowsSpeechLanguageTitle(id), id);
    }
    m_languageBox->setMinimumHeight(40);
    m_languageBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    m_languageBox->setStyleSheet(QStringLiteral(
        "QComboBox { background: #ffffff; border: 1px solid #d0d5dd;"
        " border-radius: 6px; padding: 7px 10px; }"
    ));

    m_testButton = new QPushButton(wsTr8("测试"));
    m_testButton->setObjectName(QStringLiteral("windowsSpeechTestButton"));
    m_testButton->setMinimumHeight(40);
    m_testButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    m_testButton->setStyleSheet(buttonStyle(QStringLiteral("#111827")));
    connect(m_testButton, &QPushButton::clicked, this, [this]() {
        runProbe();
    });

    m_openSettingsButton = new QPushButton(wsTr8("打开语言设置"));
    m_openSettingsButton->setObjectName(
        QStringLiteral("windowsSpeechOpenSettingsButton")
    );
    m_openSettingsButton->setMinimumHeight(40);
    m_openSettingsButton->setSizePolicy(
        QSizePolicy::Minimum,
        QSizePolicy::Minimum
    );
    m_openSettingsButton->setEnabled(false);
    m_openSettingsButton->setStyleSheet(
        buttonStyle(QStringLiteral("#475467"))
    );
    connect(m_openSettingsButton, &QPushButton::clicked, this, [this]() {
        openLanguageSettings();
    });

    controls->addWidget(m_languageBox, 1);
    controls->addWidget(m_testButton);
    controls->addWidget(m_openSettingsButton);
    layout->addLayout(controls);

    m_resultLabel = new QLabel;
    m_resultLabel->setObjectName(QStringLiteral("windowsSpeechProbeResult"));
    m_resultLabel->setWordWrap(true);
    m_resultLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_resultLabel->setVisible(false);
    m_resultLabel->setStyleSheet(QStringLiteral(
        "QLabel { background: #ffffff; color: #344054; border-radius: 6px;"
        " padding: 10px; }"
    ));
    layout->addWidget(m_resultLabel);

    m_runner = new DiagnosticTaskRunner(this);
    m_runner->finishedCallback = [this](const QStringList &lines) {
        publishProbeResult(lines);
    };
}

QString WindowsSpeechSettingsCard::language() const
{
    return normalizeWindowsSpeechLanguage(
        m_languageBox ? m_languageBox->currentData().toString() : QString()
    );
}

void WindowsSpeechSettingsCard::setLanguage(const QString &language)
{
    if (!m_languageBox) {
        return;
    }
    const QString normalized = normalizeWindowsSpeechLanguage(language);
    const int index = m_languageBox->findData(normalized);
    m_languageBox->setCurrentIndex(index >= 0 ? index : 0);
}

void WindowsSpeechSettingsCard::runProbe()
{
    if (!m_runner || !m_testButton || !m_resultLabel) {
        return;
    }
    const QString requestedLanguage = language();
    m_testButton->setEnabled(false);
    if (m_openSettingsButton) {
        m_openSettingsButton->setEnabled(false);
    }
    m_resultLabel->setText(
        wsTr8("正在测试 Windows 本地语音识别，请稍候……")
    );
    m_resultLabel->setVisible(true);
    const WindowsSpeechSettingsCardCallbacks callbacks = m_callbacks;
    m_runner->start([callbacks, requestedLanguage](
        const CancellationToken &cancellation
    ) {
        if (!callbacks.probe) {
            return QStringList()
                << QStringLiteral("PROGRAM_MISSING")
                << QStringLiteral("requestedLanguage=") + requestedLanguage;
        }
        return callbacks.probe(requestedLanguage, cancellation);
    });
}

void WindowsSpeechSettingsCard::publishProbeResult(const QStringList &lines)
{
    if (m_resultLabel) {
        m_resultLabel->setText(
            lines.isEmpty()
                ? wsTr8("测试未返回结果。")
                : lines.join(QStringLiteral("\n"))
        );
        m_resultLabel->setVisible(true);
    }
    if (m_testButton) {
        m_testButton->setEnabled(true);
    }
    if (m_openSettingsButton) {
        m_openSettingsButton->setEnabled(containsMissingRecognizer(lines));
    }
}

void WindowsSpeechSettingsCard::openLanguageSettings()
{
    if (m_callbacks.openWindowsLanguageSettings) {
        m_callbacks.openWindowsLanguageSettings();
        return;
    }
    QDesktopServices::openUrl(
        QUrl(QStringLiteral("ms-settings:regionlanguage"))
    );
}

void WindowsSpeechSettingsCard::cancelProbe()
{
    if (m_runner) {
        m_runner->cancel();
    }
    if (m_testButton) {
        m_testButton->setEnabled(true);
    }
}
