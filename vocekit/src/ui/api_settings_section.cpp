#include "api_settings_section.h"

#include "app_dialogs.h"
#include "attention_message.h"
#include "custom_model_dialog_support.h"
#include "history_row_frame.h"
#include "ui_style.h"
#include "windows_speech_settings_card.h"

#include "../config/app_settings_defaults.h"
#include "../config/baidu_sample_parser.h"
#include "../providers/openai_compatible_model_provider.h"
#include "../providers/windows_speech_helper_client.h"
#include "../providers/windows_speech_helper_protocol.h"

#include <QtConcurrent>
#include <QtWidgets>
#include <QPointer>
#include <QSet>

namespace {

QString apiTr8(const char *text)
{
    return QString::fromUtf8(text);
}

} // namespace

ApiSettingsSection::ApiSettingsSection(
    const Callbacks &callbacks,
    QWidget *parent
)
    : QWidget(parent),
      m_callbacks(callbacks)
{
    buildUi();
}

void ApiSettingsSection::refreshFromSettings()
{
    const ApiSettingsSnapshot current = snapshot();
    setComboCurrentData(m_speechProviderBox, current.speechProvider);
    if (m_windowsSpeechSettingsCard) {
        m_windowsSpeechSettingsCard->setLanguage(
            current.windowsSpeechLanguage
        );
    }
    updateSpeechSecretRows();
    setComboCurrentData(m_ocrProviderBox, current.ocrEngine);
    updateOcrSecretRows();

    const SecretConfig secrets = loadSecrets();
    if (m_openaiBaseUrlEdit) {
        m_openaiBaseUrlEdit->setText(secrets.openaiBaseUrl);
    }
    if (m_anthropicBaseUrlEdit) {
        m_anthropicBaseUrlEdit->setText(secrets.anthropicBaseUrl);
    }
}

ApiSettingsSnapshot ApiSettingsSection::snapshot() const
{
    return m_callbacks.snapshotProvider
        ? m_callbacks.snapshotProvider()
        : ApiSettingsSnapshot();
}

bool ApiSettingsSection::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        auto *widget = qobject_cast<QWidget *>(watched);
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (widget && mouse->button() == Qt::LeftButton && widget->property("settingDetailEnabled").toBool()) {
            if (m_callbacks.showDetail) {
                m_callbacks.showDetail(
                    widget->property("settingDetailTitle").toString(),
                    widget->property("settingDetailText").toString()
                );
            }
            event->accept();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void ApiSettingsSection::setComboCurrentData(QComboBox *box, const QString &value)
{
    if (!box) {
        return;
    }
    const int index = box->findData(value);
    if (index < 0 || index == box->currentIndex()) {
        return;
    }
    box->blockSignals(true);
    box->setCurrentIndex(index);
    box->blockSignals(false);
}

void ApiSettingsSection::attachSettingDetail(
    QWidget *card,
    const QString &title,
    const QString &detail)
{
    installSettingDetailTarget(card, title, detail);
    const QList<QLabel *> labels = card
        ? card->findChildren<QLabel *>(QString(), Qt::FindDirectChildrenOnly)
        : QList<QLabel *>();
    for (QLabel *label : labels) {
        if (label && label->textInteractionFlags() == Qt::NoTextInteraction) {
            installSettingDetailTarget(label, title, detail);
        }
    }
}

void ApiSettingsSection::installSettingDetailTarget(
    QWidget *target,
    const QString &title,
    const QString &detail)
{
    if (!target || detail.trimmed().isEmpty()) {
        return;
    }
    target->setProperty("settingDetailEnabled", true);
    target->setProperty("settingDetailTitle", title);
    target->setProperty("settingDetailText", detail);
    target->setCursor(Qt::PointingHandCursor);
    target->installEventFilter(this);
}
QString ApiSettingsSection::apiRowDetailText(const QString &title, const QString &hint) const
    {
        if (title == apiTr8("当前语音识别服务")) {
            return apiTr8("选择语音输入使用的识别服务。\n\n百度语音识别：使用百度 API Key 和 Secret Key。\n讯飞语音听写：使用讯飞 AppID、API Key 和 API Secret。\n自定义语音接口：向你填写的地址发送 JSON 音频请求。\n\n切换服务后，界面只显示当前服务需要填写的字段，但已经填写过的其它服务配置仍会保存在配置里。");
        }
        if (title == apiTr8("百度接口密钥（API Key）")) {
            return apiTr8("百度语音识别的 API Key，用来和 Secret Key 一起换取访问令牌。\n\n它必须来自百度智能云语音识别应用。复制时不要带多余空格或换行。");
        }
        if (title == apiTr8("百度安全密钥（Secret Key）")) {
            return apiTr8("百度语音识别的 Secret Key，必须和百度 API Key 属于同一个应用。\n\n如果填错，通常会出现令牌获取失败或接口认证失败。");
        }
        if (title == apiTr8("百度应用编号（AppID）")) {
            return apiTr8("百度实时语音识别需要 AppID 和 API Key。仅使用停止后的整段 REST 识别时可以留空；留空不会阻止保存，软件会自动使用整段识别。");
        }
        if (title == apiTr8("讯飞应用编号（AppID）")) {
            return apiTr8("讯飞语音听写的 AppID。讯飞 WebSocket 鉴权时需要它和 API Key、API Secret 对应。\n\n请按讯飞控制台的字段顺序填写，避免把 API Key 和 API Secret 填反。");
        }
        if (title == apiTr8("讯飞接口密钥（API Key）")) {
            return apiTr8("讯飞语音听写的 API Key，用于生成 WebSocket 鉴权签名。\n\n它必须和同一个讯飞应用的 AppID、API Secret 配套使用。");
        }
        if (title == apiTr8("讯飞安全密钥（API Secret）")) {
            return apiTr8("讯飞语音听写的 API Secret，用于生成 WebSocket 鉴权签名。\n\n如果网络提示远端关闭连接，除了检查这里的密钥，也要检查 TUN、代理分流和讯飞接口权限。");
        }
        if (title == apiTr8("自定义语音接口地址")) {
            return apiTr8("自定义语音接口会收到一个 JSON POST 请求：format=pcm、rate=16000、channel=1、len=音频字节数、speech=PCM 的 base64。\n\n接口返回 JSON 时，软件会优先读取 text、result、transcript、data.text、data.result 等字段作为识别文字。");
        }
        if (title == apiTr8("自定义语音接口密钥（API Key）")) {
            return apiTr8("可选字段。填写后软件会在请求头里发送 Authorization: Bearer 你的密钥。\n\n如果你的自定义接口不需要认证，可以留空。");
        }
        if (title == apiTr8("自定义语音模型名称")) {
            return apiTr8("可选字段。填写后会随请求 JSON 的 model 字段发送给自定义语音接口。\n\n如果接口不区分模型，可以留空。");
        }
        if (title == apiTr8("DeepSeek 密钥（API Key）")) {
            return apiTr8("使用 DeepSeek 模型时需要填写。听写整理、翻译、问答和自定义功能如果选择 deepseek 模型，会读取这里的密钥。");
        }
        if (title == apiTr8("OpenAI 密钥（GPT API Key）")) {
            return apiTr8("使用 OpenAI 模型时需要填写。只有功能自定义里选择 OpenAI 模型时才会调用它。");
        }
        if (title == apiTr8("OpenAI Base URL（可选）")) {
            const QString value = m_openaiBaseUrlEdit
                ? m_openaiBaseUrlEdit->text().trimmed()
                : QString();
            return apiTr8("当前值：")
                + (value.isEmpty() ? apiTr8("官方地址") : value)
                + apiTr8("\n\n留空时使用 OpenAI 官方地址。也可填写网关根地址、以 /v1 结尾的地址，或完整 chat/completions 接口地址。");
        }
        if (title == apiTr8("Anthropic 密钥（Claude API Key）")) {
            return apiTr8("使用 Claude 模型时需要填写。只有功能自定义里选择 Claude 模型时才会调用它。");
        }
        if (title == apiTr8("Anthropic Base URL（可选）")) {
            const QString value = m_anthropicBaseUrlEdit
                ? m_anthropicBaseUrlEdit->text().trimmed()
                : QString();
            return apiTr8("当前值：")
                + (value.isEmpty() ? apiTr8("官方地址") : value)
                + apiTr8("\n\n留空时使用 Anthropic 官方地址。也可填写网关根地址、以 /v1 结尾的地址，或完整 messages 接口地址。");
        }
        if (title == apiTr8("自定义大模型接口地址")) {
            return apiTr8("按 OpenAI 兼容接口调用。可以填写根地址，例如 https://api.example.com，软件会自动补成 /v1/chat/completions；也可以直接填写完整 chat/completions 地址。\n\n选择模型时使用“自定义大模型”才会调用这里。");
        }
        if (title == apiTr8("自定义大模型密钥（API Key）")) {
            return apiTr8("可选字段。填写后软件会在请求头里发送 Authorization: Bearer 你的密钥。\n\n如果你的自定义大模型接口不需要认证，可以留空。");
        }
        if (title == apiTr8("自定义大模型名称")) {
            return apiTr8("选择“自定义大模型”时实际发送给接口的 model 值。\n\n如果留空，软件会发送 custom-model。");
        }
        return hint.trimmed().isEmpty() ? title : hint;
    }

void ApiSettingsSection::buildUi()
    {
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(22, 22, 22, 22);
        layout->setSpacing(12);

        auto *scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);

        auto *holder = new QWidget;
        auto *content = new QVBoxLayout(holder);
        content->setContentsMargins(0, 0, 0, 0);
        content->setSpacing(14);

        const SecretConfig secrets = loadSecrets();

        m_deepseekKeyEdit = newSecretEdit(secrets.deepseekApiKey);
        m_openaiKeyEdit = newSecretEdit(secrets.openaiApiKey);
        m_openaiBaseUrlEdit = newPlainEdit(
            secrets.openaiBaseUrl,
            apiTr8("留空使用官方地址；可填网关根地址、/v1 或完整 chat/completions 地址")
        );
        m_openaiBaseUrlEdit->setObjectName(QStringLiteral("openaiBaseUrlEdit"));
        m_anthropicKeyEdit = newSecretEdit(secrets.anthropicApiKey);
        m_anthropicBaseUrlEdit = newPlainEdit(
            secrets.anthropicBaseUrl,
            apiTr8("留空使用官方地址；可填网关根地址、/v1 或完整 messages 地址")
        );
        m_anthropicBaseUrlEdit->setObjectName(QStringLiteral("anthropicBaseUrlEdit"));
        m_baiduApiKeyEdit = newSecretEdit(secrets.baiduApiKey);
        m_baiduSecretKeyEdit = newSecretEdit(secrets.baiduSecretKey);
        m_baiduAppIdEdit = newSecretEdit(secrets.baiduAppId);
        m_xfyunAppIdEdit = newSecretEdit(secrets.xfyunAppId);
        m_xfyunApiKeyEdit = newSecretEdit(secrets.xfyunApiKey);
        m_xfyunApiSecretEdit = newSecretEdit(secrets.xfyunApiSecret);
        m_customSpeechUrlEdit = newPlainEdit(secrets.customSpeechUrl, apiTr8("例如：https://example.com/asr"));
        m_customSpeechApiKeyEdit = newSecretEdit(secrets.customSpeechApiKey);
        m_customSpeechModelEdit = newPlainEdit(secrets.customSpeechModel, apiTr8("可选，例如：asr-large"));
        m_customOcrUrlEdit = newPlainEdit(secrets.customOcrUrl, apiTr8("例如：https://example.com/ocr"));
        m_customOcrApiKeyEdit = newSecretEdit(secrets.customOcrApiKey);
        m_customOcrModelEdit = newPlainEdit(secrets.customOcrModel, apiTr8("可选，例如：ocr-large"));
        m_customModelProfiles = secrets.effectiveCustomModels();

        QVector<QWidget *> voiceRows;
        voiceRows.append(speechProviderRow());
        WindowsSpeechSettingsCardCallbacks windowsCallbacks;
        windowsCallbacks.probe = [](
            const QString &language,
            const CancellationToken &cancellation
        ) {
            WindowsSpeechHelperClient client(
                windowsSpeechHelperPathForApplicationDir(
                    QCoreApplication::applicationDirPath()
                )
            );
            WindowsSpeechProbeRequest request;
            request.runId = QUuid::createUuid().toString();
            request.language = normalizeWindowsSpeechLanguage(language);
            request.cancellation = cancellation;
            const WindowsSpeechHelperResult result = client.probe(request);
            if (!result.ok) {
                return QStringList()
                    << result.errorCode
                    << QStringLiteral("requestedLanguage=") + request.language
                    << result.errorMessage;
            }
            return QStringList()
                << QStringLiteral("OK")
                << QStringLiteral("resolvedLanguage=") + result.resolvedLanguage
                << QStringLiteral("installedLanguages=")
                    + result.installedLanguages.join(QStringLiteral(","));
        };
        m_windowsSpeechSettingsCard = new WindowsSpeechSettingsCard(
            windowsCallbacks
        );
        m_windowsSpeechSettingsCard->setLanguage(
            snapshot().windowsSpeechLanguage
        );
        voiceRows.append(m_windowsSpeechSettingsCard);
        m_baiduSampleCodeImportRow = baiduSampleCodeImportRow();
        m_baiduApiKeyRow = secretInputRow(apiTr8("百度接口密钥（API Key）"), apiTr8("语音转文字时使用"), m_baiduApiKeyEdit);
        m_baiduSecretKeyRow = secretInputRow(apiTr8("百度安全密钥（Secret Key）"), apiTr8("用于获取语音识别访问令牌"), m_baiduSecretKeyEdit);
        m_baiduAppIdRow = secretInputRow(apiTr8("百度应用编号（AppID）"), apiTr8("实时识别必填；仅整段识别可留空"), m_baiduAppIdEdit);
        m_xfyunAppIdRow = secretInputRow(apiTr8("讯飞应用编号（AppID）"), apiTr8("讯飞语音听写应用编号"), m_xfyunAppIdEdit);
        m_xfyunApiKeyRow = secretInputRow(apiTr8("讯飞接口密钥（API Key）"), apiTr8("用于生成讯飞接口鉴权签名"), m_xfyunApiKeyEdit);
        m_xfyunApiSecretRow = secretInputRow(apiTr8("讯飞安全密钥（API Secret）"), apiTr8("用于生成讯飞接口鉴权签名"), m_xfyunApiSecretEdit);
        m_customSpeechUrlRow = plainInputRow(apiTr8("自定义语音接口地址"), apiTr8("接收 JSON 音频请求的接口地址"), m_customSpeechUrlEdit);
        m_customSpeechApiKeyRow = secretInputRow(apiTr8("自定义语音接口密钥（API Key）"), apiTr8("可选，会以 Bearer Token 方式发送"), m_customSpeechApiKeyEdit);
        m_customSpeechModelRow = plainInputRow(apiTr8("自定义语音模型名称"), apiTr8("可选，会随请求 JSON 的 model 字段发送"), m_customSpeechModelEdit);
        voiceRows.append(m_baiduSampleCodeImportRow);
        voiceRows.append(m_baiduApiKeyRow);
        voiceRows.append(m_baiduSecretKeyRow);
        voiceRows.append(m_baiduAppIdRow);
        voiceRows.append(m_xfyunAppIdRow);
        voiceRows.append(m_xfyunApiKeyRow);
        voiceRows.append(m_xfyunApiSecretRow);
        voiceRows.append(m_customSpeechUrlRow);
        voiceRows.append(m_customSpeechApiKeyRow);
        voiceRows.append(m_customSpeechModelRow);

        QVector<QWidget *> ocrRows;
        ocrRows.append(ocrProviderRow());
        m_customOcrUrlRow = plainInputRow(
            apiTr8("自定义云 OCR 接口地址"),
            apiTr8("接收图片 Base64 JSON 请求"),
            m_customOcrUrlEdit
        );
        m_customOcrApiKeyRow = secretInputRow(
            apiTr8("自定义云 OCR 密钥（API Key）"),
            apiTr8("可选，会以 Bearer Token 方式发送"),
            m_customOcrApiKeyEdit
        );
        m_customOcrModelRow = plainInputRow(
            apiTr8("自定义云 OCR 模型名称"),
            apiTr8("可选，会随请求 JSON 的 model 字段发送"),
            m_customOcrModelEdit
        );
        ocrRows.append(m_customOcrUrlRow);
        ocrRows.append(m_customOcrApiKeyRow);
        ocrRows.append(m_customOcrModelRow);

        QVector<QWidget *> modelRows;
        modelRows.append(secretInputRow(apiTr8("DeepSeek 密钥（API Key）"), apiTr8("选择 DeepSeek 模型时使用"), m_deepseekKeyEdit));
        modelRows.append(secretInputRow(apiTr8("OpenAI 密钥（GPT API Key）"), apiTr8("选择 GPT 模型时使用"), m_openaiKeyEdit));
        m_openaiBaseUrlRow = plainInputRow(
            apiTr8("OpenAI Base URL（可选）"),
            apiTr8("留空使用官方地址；支持网关根地址、/v1 或完整接口地址"),
            m_openaiBaseUrlEdit
        );
        modelRows.append(m_openaiBaseUrlRow);
        modelRows.append(secretInputRow(apiTr8("Anthropic 密钥（Claude API Key）"), apiTr8("选择 Claude 模型时使用"), m_anthropicKeyEdit));
        m_anthropicBaseUrlRow = plainInputRow(
            apiTr8("Anthropic Base URL（可选）"),
            apiTr8("留空使用官方地址；支持网关根地址、/v1 或完整接口地址"),
            m_anthropicBaseUrlEdit
        );
        modelRows.append(m_anthropicBaseUrlRow);
        modelRows.append(customModelConfigCard());

        connect(m_openaiBaseUrlEdit, &QLineEdit::textChanged, this, [this]() {
            attachSettingDetail(
                m_openaiBaseUrlRow,
                apiTr8("OpenAI Base URL（可选）"),
                apiRowDetailText(
                    apiTr8("OpenAI Base URL（可选）"),
                    QString()
                )
            );
        });
        connect(m_anthropicBaseUrlEdit, &QLineEdit::textChanged, this, [this]() {
            attachSettingDetail(
                m_anthropicBaseUrlRow,
                apiTr8("Anthropic Base URL（可选）"),
                apiRowDetailText(
                    apiTr8("Anthropic Base URL（可选）"),
                    QString()
                )
            );
        });

        content->addWidget(secretSection(apiTr8("语音识别接口"), apiTr8("用于听写、问答和自定义功能里的语音输入。可以选择百度、讯飞或自定义语音接口。"), voiceRows));
        content->addWidget(secretSection(apiTr8("图片识别接口"), QString(), ocrRows));
        content->addWidget(secretSection(apiTr8("大模型接口"), apiTr8("用于整理听写内容、翻译、问答和自定义功能。"), modelRows));
        updateSpeechSecretRows();
        updateOcrSecretRows();

        content->addStretch();
        scroll->setWidget(holder);
        layout->addWidget(scroll, 1);

        auto *buttons = new QHBoxLayout;
        auto *save = new QPushButton(apiTr8("保存接口配置"));
        save->setStyleSheet(buttonStyle(QStringLiteral("#111827")));
        buttons->addWidget(save);
        buttons->addStretch();
        layout->addLayout(buttons);

        connect(save, &QPushButton::clicked, this, [this]() {
            saveSecretsFromUi(true);
        });

    }

QLineEdit *ApiSettingsSection::newSecretEdit(const QString &value)
    {
        auto *edit = new QLineEdit(value);
        edit->setEchoMode(QLineEdit::Password);
        edit->setMinimumHeight(34);
        edit->setPlaceholderText(apiTr8("在这里填写"));
        edit->setStyleSheet(QStringLiteral(
            "QLineEdit {"
            "  background: #ffffff;"
            "  border: 1px solid #d0d5dd;"
            "  border-radius: 6px;"
            "  padding: 6px 10px;"
            "}"
        ));
        return edit;
    }

QLineEdit *ApiSettingsSection::newPlainEdit(const QString &value, const QString &placeholder)
    {
        auto *edit = new QLineEdit(value);
        edit->setMinimumHeight(34);
        edit->setPlaceholderText(placeholder);
        edit->setStyleSheet(QStringLiteral(
            "QLineEdit {"
            "  background: #ffffff;"
            "  border: 1px solid #d0d5dd;"
            "  border-radius: 6px;"
            "  padding: 6px 10px;"
            "}"
        ));
        return edit;
    }

QWidget *ApiSettingsSection::speechProviderRow()
    {
        auto *frame = new QFrame;
        frame->setObjectName(QStringLiteral("secretRow"));
        frame->setStyleSheet(QStringLiteral(
            "QFrame#secretRow {"
            "  background: #f9fafb;"
            "  border: 1px solid #eef0f4;"
            "  border-radius: 6px;"
            "}"
        ));
        auto *layout = new QHBoxLayout(frame);
        layout->setContentsMargins(16, 12, 16, 12);
        layout->setSpacing(12);

        auto *labels = new QVBoxLayout;
        auto *name = new QLabel(apiTr8("当前语音识别服务"));
        name->setFont(appFont(11, QFont::DemiBold));
        labels->addWidget(name);

        m_speechProviderBox = new QComboBox;
        for (const QString &provider : supportedSpeechProviderIds()) {
            m_speechProviderBox->addItem(
                speechProviderTitle(provider),
                provider
            );
        }
        const int index = m_speechProviderBox->findData(snapshot().speechProvider);
        m_speechProviderBox->setCurrentIndex(index >= 0 ? index : 0);
        m_speechProviderBox->setMinimumWidth(240);
        m_speechProviderBox->setFixedHeight(34);
        connect(m_speechProviderBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [this]() {
            updateSpeechSecretRows();
        });

        layout->addLayout(labels, 1);
        layout->addWidget(m_speechProviderBox);
        attachSettingDetail(frame, apiTr8("当前语音识别服务"), apiRowDetailText(apiTr8("当前语音识别服务"), QString()));
        return frame;
    }

QWidget *ApiSettingsSection::ocrProviderRow()
    {
        auto *frame = new QFrame;
        frame->setObjectName(QStringLiteral("secretRow"));
        frame->setStyleSheet(QStringLiteral(
            "QFrame#secretRow {"
            "  background: #f9fafb;"
            "  border: 1px solid #eef0f4;"
            "  border-radius: 6px;"
            "}"
        ));
        auto *layout = new QHBoxLayout(frame);
        layout->setContentsMargins(16, 12, 16, 12);
        layout->setSpacing(12);

        auto *name = new QLabel(apiTr8("当前图片识别服务"));
        name->setFont(appFont(11, QFont::DemiBold));

        m_ocrProviderBox = new QComboBox;
        m_ocrProviderBox->addItem(apiTr8("自动选择（本地）"), ocrEngineAutomatic());
        m_ocrProviderBox->addItem(QStringLiteral("RapidOCR"), ocrEngineRapid());
        m_ocrProviderBox->addItem(QStringLiteral("Windows OCR"), ocrEngineWindows());
        m_ocrProviderBox->addItem(apiTr8("自定义云 OCR"), ocrEngineCustomCloud());
        const int index = m_ocrProviderBox->findData(snapshot().ocrEngine);
        m_ocrProviderBox->setCurrentIndex(index >= 0 ? index : 0);
        m_ocrProviderBox->setMinimumWidth(240);
        m_ocrProviderBox->setFixedHeight(34);
        connect(m_ocrProviderBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [this]() {
            updateOcrSecretRows();
        });

        layout->addWidget(name, 1);
        layout->addWidget(m_ocrProviderBox);
        attachSettingDetail(
            frame,
            apiTr8("当前图片识别服务"),
            apiTr8("自动选择会优先使用 RapidOCR，失败时回退到 Windows OCR。自定义云 OCR 会把图片发送到你填写的接口。")
        );
        return frame;
    }

QWidget *ApiSettingsSection::baiduSampleCodeImportRow()
    {
        auto *frame = new QFrame;
        frame->setObjectName(QStringLiteral("secretRow"));
        frame->setStyleSheet(QStringLiteral(
            "QFrame#secretRow {"
            "  background: #f9fafb;"
            "  border: 1px solid #eef0f4;"
            "  border-radius: 6px;"
            "}"
        ));
        auto *layout = new QVBoxLayout(frame);
        layout->setContentsMargins(16, 12, 16, 12);
        layout->setSpacing(10);

        auto *top = new QHBoxLayout;
        auto *name = new QLabel(apiTr8("从百度示例代码导入"));
        name->setFont(appFont(11, QFont::DemiBold));
        auto *parse = new QPushButton(apiTr8("解析并填入"));
        parse->setFixedHeight(34);
        parse->setStyleSheet(buttonStyle(QStringLiteral("#111827")));
        top->addWidget(name, 1);
        top->addWidget(parse);
        layout->addLayout(top);

        auto *code = new QTextEdit;
        code->setMinimumHeight(96);
        code->setPlaceholderText(apiTr8("把百度智能云示例代码粘贴到这里，软件会提取 client_id 和 client_secret。"));
        code->setStyleSheet(QStringLiteral(
            "QTextEdit {"
            "  background: #ffffff;"
            "  border: 1px solid #d0d5dd;"
            "  border-radius: 6px;"
            "  padding: 8px 10px;"
            "}"
        ));
        layout->addWidget(code);

        connect(parse, &QPushButton::clicked, this, [this, code]() {
            QString apiKey;
            QString secretKey;
            if (!extractBaiduCredentialsFromSampleCode(code->toPlainText(), &apiKey, &secretKey)) {
                showAttentionWarning(
                    this,
                    apiTr8("解析失败"),
                    apiTr8("没有在示例代码中找到 client_id 和 client_secret。请确认粘贴的是百度智能云获取 AccessToken 的示例代码。")
                );
                return;
            }
            if (m_baiduApiKeyEdit) {
                m_baiduApiKeyEdit->setText(apiKey);
            }
            if (m_baiduSecretKeyEdit) {
                m_baiduSecretKeyEdit->setText(secretKey);
            }
            code->clear();
            showAttentionInformation(this, apiTr8("已填入"), apiTr8("已从示例代码填入百度 API Key 和 Secret Key。请点击“保存接口配置”后再使用。"));
        });

        attachSettingDetail(
            frame,
            apiTr8("从百度示例代码导入"),
            apiTr8("百度智能云的 AccessToken 示例代码里，client_id 对应百度 API Key，client_secret 对应百度 Secret Key。软件只提取这两个字段，不保存粘贴的代码。")
        );
        return frame;
    }

void ApiSettingsSection::updateSpeechSecretRows()
    {
        const QString provider = m_speechProviderBox
            ? normalizeSpeechProvider(m_speechProviderBox->currentData().toString())
            : normalizeSpeechProvider(snapshot().speechProvider);
        const bool showBaidu = provider == speechProviderBaidu();
        const bool showXfyun = provider == speechProviderXfyun();
        const bool showCustom = provider == speechProviderCustom();
        const bool showWindows = provider == speechProviderWindowsLocal();

        if (m_windowsSpeechSettingsCard) {
            m_windowsSpeechSettingsCard->setVisible(showWindows);
            m_windowsSpeechSettingsCard->setEnabled(showWindows);
        }

        const QVector<QWidget *> baiduRows = QVector<QWidget *>()
            << m_baiduSampleCodeImportRow
            << m_baiduApiKeyRow
            << m_baiduSecretKeyRow
            << m_baiduAppIdRow;
        for (QWidget *row : baiduRows) {
            if (row) {
                row->setVisible(showBaidu);
                row->setEnabled(showBaidu);
            }
        }

        const QVector<QWidget *> xfyunRows = QVector<QWidget *>()
            << m_xfyunAppIdRow
            << m_xfyunApiKeyRow
            << m_xfyunApiSecretRow;
        for (QWidget *row : xfyunRows) {
            if (row) {
                row->setVisible(showXfyun);
                row->setEnabled(showXfyun);
            }
        }

        const QVector<QWidget *> customRows = QVector<QWidget *>()
            << m_customSpeechUrlRow
            << m_customSpeechApiKeyRow
            << m_customSpeechModelRow;
        for (QWidget *row : customRows) {
            if (row) {
                row->setVisible(showCustom);
                row->setEnabled(showCustom);
            }
        }
    }

void ApiSettingsSection::updateOcrSecretRows()
    {
        const QString provider = m_ocrProviderBox
            ? normalizeOcrEngine(m_ocrProviderBox->currentData().toString())
            : normalizeOcrEngine(snapshot().ocrEngine);
        const bool showCustomCloud = provider == ocrEngineCustomCloud();

        const QVector<QWidget *> customRows = QVector<QWidget *>()
            << m_customOcrUrlRow
            << m_customOcrApiKeyRow
            << m_customOcrModelRow;
        for (QWidget *row : customRows) {
            if (row) {
                row->setVisible(showCustomCloud);
                row->setEnabled(showCustomCloud);
            }
        }
    }

QWidget *ApiSettingsSection::secretSection(const QString &title, const QString &hint, const QVector<QWidget *> &rows)
    {
        auto *frame = new QFrame;
        frame->setObjectName(QStringLiteral("apiSection"));
        frame->setStyleSheet(QStringLiteral(
            "QFrame#apiSection {"
            "  background: #ffffff;"
            "  border: 1px solid #dde2ea;"
            "  border-radius: 8px;"
            "}"
        ));

        auto *layout = new QVBoxLayout(frame);
        layout->setContentsMargins(16, 14, 16, 16);
        layout->setSpacing(12);

        auto *name = new QLabel(title);
        name->setFont(appFont(13, QFont::DemiBold));
        layout->addWidget(name);

        for (QWidget *row : rows) {
            layout->addWidget(row);
        }
        attachSettingDetail(frame, title, hint);
        return frame;
    }

QWidget *ApiSettingsSection::secretInputRow(const QString &title, const QString &hint, QLineEdit *edit)
    {
        auto *frame = new QFrame;
        frame->setObjectName(QStringLiteral("secretRow"));
        frame->setStyleSheet(QStringLiteral(
            "QFrame#secretRow {"
            "  background: #f9fafb;"
            "  border: 1px solid #eef0f4;"
            "  border-radius: 6px;"
            "}"
        ));
        auto *layout = new QHBoxLayout(frame);
        layout->setContentsMargins(16, 12, 16, 12);
        layout->setSpacing(12);

        auto *labels = new QVBoxLayout;
        auto *name = new QLabel(title);
        name->setFont(appFont(11, QFont::DemiBold));
        labels->addWidget(name);

        auto *show = new QPushButton(apiTr8("显示"));
        show->setFixedHeight(34);
        show->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
        connect(show, &QPushButton::clicked, this, [edit, show]() {
            if (edit->echoMode() == QLineEdit::Password) {
                edit->setEchoMode(QLineEdit::Normal);
                show->setText(apiTr8("隐藏"));
            } else {
                edit->setEchoMode(QLineEdit::Password);
                show->setText(apiTr8("显示"));
            }
        });

        layout->addLayout(labels, 1);
        layout->addWidget(edit, 2);
        layout->addWidget(show);
        attachSettingDetail(frame, title, apiRowDetailText(title, hint));
        return frame;
    }

QWidget *ApiSettingsSection::plainInputRow(const QString &title, const QString &hint, QLineEdit *edit)
    {
        auto *frame = new QFrame;
        frame->setObjectName(QStringLiteral("secretRow"));
        frame->setStyleSheet(QStringLiteral(
            "QFrame#secretRow {"
            "  background: #f9fafb;"
            "  border: 1px solid #eef0f4;"
            "  border-radius: 6px;"
            "}"
        ));
        auto *layout = new QHBoxLayout(frame);
        layout->setContentsMargins(16, 12, 16, 12);
        layout->setSpacing(12);

        auto *labels = new QVBoxLayout;
        auto *name = new QLabel(title);
        name->setFont(appFont(11, QFont::DemiBold));
        labels->addWidget(name);

        layout->addLayout(labels, 1);
        layout->addWidget(edit, 2);
        attachSettingDetail(frame, title, apiRowDetailText(title, hint));
        return frame;
    }

QString ApiSettingsSection::customModelSummaryText() const
    {
        QStringList names;
        int configured = 0;
        for (const CustomModelProfile &profile : m_customModelProfiles) {
            if (!profile.hasEndpoint()) {
                continue;
            }
            ++configured;
            names << (profile.name.trimmed().isEmpty() ? profile.id : profile.name.trimmed());
        }
        if (configured == 0) {
            return apiTr8("未配置。点击卡片新增一个或多个自定义大模型。");
        }
        QString summary = apiTr8("已配置 ") + QString::number(configured) + apiTr8(" 个");
        if (!names.isEmpty()) {
            summary += apiTr8("：") + names.mid(0, 3).join(apiTr8("、"));
            if (names.size() > 3) {
                summary += apiTr8(" 等");
            }
        }
        return summary;
    }

void ApiSettingsSection::refreshCustomModelSummary()
    {
        if (m_customModelSummaryLabel) {
            m_customModelSummaryLabel->setText(customModelSummaryText());
        }
    }

QWidget *ApiSettingsSection::customModelConfigCard()
    {
        auto *frame = new HistoryRowFrame;
        frame->setObjectName(QStringLiteral("secretRow"));
        frame->setStyleSheet(QStringLiteral(
            "QFrame#secretRow {"
            "  background: #f9fafb;"
            "  border: 1px solid #eef0f4;"
            "  border-radius: 6px;"
            "}"
        ));
        auto *layout = new QHBoxLayout(frame);
        layout->setContentsMargins(16, 12, 16, 12);
        layout->setSpacing(12);

        auto *labels = new QVBoxLayout;
        auto *name = new QLabel(apiTr8("自定义大模型"));
        name->setFont(appFont(11, QFont::DemiBold));
        labels->addWidget(name);

        m_customModelSummaryLabel = new QLabel(customModelSummaryText());
        m_customModelSummaryLabel->setWordWrap(true);
        m_customModelSummaryLabel->setStyleSheet(QStringLiteral("color: #4b5563;"));
        labels->addWidget(m_customModelSummaryLabel);

        auto *button = new QPushButton(apiTr8("配置"));
        button->setFixedHeight(34);
        button->setStyleSheet(buttonStyle(QStringLiteral("#111827")));

        layout->addLayout(labels, 1);
        layout->addWidget(button);

        auto openDialog = [this]() {
            showCustomModelConfigDialog();
        };
        frame->setClickCallback(openDialog);
        connect(button, &QPushButton::clicked, this, openDialog);
        return frame;
    }

QWidget *ApiSettingsSection::customModelEditorRow(
        const CustomModelProfile &profile,
        QLineEdit **nameEdit,
        QLineEdit **urlEdit,
        QLineEdit **keyEdit,
        QLineEdit **modelEdit,
        QPushButton **deleteButton
    )
    {
        auto *frame = new QFrame;
        frame->setObjectName(QStringLiteral("customModelRow"));
        frame->setStyleSheet(QStringLiteral(
            "QFrame#customModelRow { background: #ffffff; border: 1px solid #dde2ea; border-radius: 8px; }"
        ));
        auto *layout = new QVBoxLayout(frame);
        layout->setContentsMargins(14, 12, 14, 12);
        layout->setSpacing(10);

        auto addLine = [&](const QString &labelText, QLineEdit *edit) {
            auto *row = new QHBoxLayout;
            auto *label = new QLabel(labelText);
            label->setMinimumWidth(132);
            label->setFont(appFont(10, QFont::DemiBold));
            row->addWidget(label);
            row->addWidget(edit, 1);
            layout->addLayout(row);
        };

        *nameEdit = newPlainEdit(profile.name, apiTr8("显示名称，例如：公司网关 GPT"));
        *urlEdit = newPlainEdit(profile.url, apiTr8("API URL，例如：https://api.example.com 或 /v1 地址"));
        (*urlEdit)->setObjectName(QStringLiteral("customModelApiUrlEdit"));
        *keyEdit = newSecretEdit(profile.apiKey);
        *modelEdit = newPlainEdit(profile.model, apiTr8("实际模型名，例如：gpt-4o-compatible"));
        (*keyEdit)->setPlaceholderText(apiTr8("可选，不需要密钥可留空"));

        auto *top = new QHBoxLayout;
        auto *title = new QLabel(profile.name.trimmed().isEmpty() ? apiTr8("自定义大模型") : profile.name.trimmed());
        title->setFont(appFont(12, QFont::DemiBold));
        auto *testButton = new QPushButton(apiTr8("测试"));
        applyCustomModelDialogButtonSizing(
            testButton,
            QStringLiteral("#ffffff"),
            QStringLiteral("#111827")
        );
        *deleteButton = new QPushButton(apiTr8("删除"));
        applyCustomModelDialogButtonSizing(
            *deleteButton,
            QStringLiteral("#ffffff"),
            QStringLiteral("#b91c1c")
        );
        top->addWidget(title, 1);
        top->addWidget(testButton);
        top->addWidget(*deleteButton);
        layout->addLayout(top);

        addLine(apiTr8("显示名称"), *nameEdit);
        addLine(apiTr8("API URL（接口地址）"), *urlEdit);

        auto *endpointPreviewRow = new QHBoxLayout;
        auto *endpointPreviewTitle = new QLabel(apiTr8("最终请求地址"));
        endpointPreviewTitle->setMinimumWidth(132);
        endpointPreviewTitle->setFont(appFont(10, QFont::DemiBold));
        auto *endpointPreview = new QLabel;
        endpointPreview->setObjectName(QStringLiteral("customModelFinalEndpointPreview"));
        endpointPreview->setWordWrap(true);
        endpointPreview->setTextInteractionFlags(Qt::TextSelectableByMouse);
        endpointPreviewRow->addWidget(endpointPreviewTitle);
        endpointPreviewRow->addWidget(endpointPreview, 1);
        layout->addLayout(endpointPreviewRow);

        auto updateEndpointPreview = [endpointPreview](const QString &urlText) {
            const QString preview = customModelFinalEndpointPreview(urlText);
            endpointPreview->setText(preview);
            endpointPreview->setStyleSheet(
                preview == apiTr8("地址无效")
                    ? QStringLiteral("color: #b91c1c;")
                    : QStringLiteral("color: #4b5563;")
            );
        };
        updateEndpointPreview((*urlEdit)->text());
        connect(
            *urlEdit,
            &QLineEdit::textChanged,
            this,
            updateEndpointPreview
        );

        addLine(apiTr8("接口密钥"), *keyEdit);
        addLine(apiTr8("模型名称"), *modelEdit);

        QLineEdit * const nameField = *nameEdit;
        QLineEdit * const urlField = *urlEdit;
        QLineEdit * const keyField = *keyEdit;
        QLineEdit * const modelField = *modelEdit;
        const QPointer<QPushButton> testButtonGuard(testButton);
        connect(testButton, &QPushButton::clicked, this, [this, profile, testButtonGuard, nameField, urlField, keyField, modelField]() {
            CustomModelProfile testProfile = profile;
            testProfile.id = normalizeCustomModelProfileId(testProfile.id);
            testProfile.name = nameField ? nameField->text().trimmed() : QString();
            testProfile.url = urlField ? urlField->text().trimmed() : QString();
            testProfile.apiKey = keyField ? keyField->text().trimmed() : QString();
            testProfile.model = modelField ? modelField->text().trimmed() : QString();
            if (testProfile.url.isEmpty()) {
                showAttentionWarning(this, apiTr8("测试失败"), apiTr8("请先填写自定义大模型接口地址。"));
                return;
            }

            if (testButtonGuard) {
                testButtonGuard->setEnabled(false);
                testButtonGuard->setText(apiTr8("测试中"));
            }
            const bool useSystemProxy = snapshot().useSystemProxy;
            auto *watcher = new QFutureWatcher<QPair<QString, QString>>(this);
            connect(watcher, &QFutureWatcher<QPair<QString, QString>>::finished, this, [this, watcher, testButtonGuard]() {
                const QPair<QString, QString> result = watcher->result();
                if (testButtonGuard) {
                    testButtonGuard->setEnabled(true);
                    testButtonGuard->setText(apiTr8("测试"));
                }
                if (result.first.isEmpty()) {
                    showAttentionWarning(this, apiTr8("自定义大模型测试失败"), result.second);
                } else {
                    showAttentionInformation(this, apiTr8("测试通过"), result.first);
                }
                watcher->deleteLater();
            });
            watcher->setFuture(QtConcurrent::run([testProfile, useSystemProxy]() {
                SecretConfig secrets;
                secrets.customModels.append(testProfile);
                const QSharedPointer<IModelProvider> provider =
                    createOpenAiCompatibleModelProvider(
                        QStringLiteral("custom:") + testProfile.id,
                        secrets,
                        useSystemProxy
                    );
                const ProviderCheckResult result =
                    provider->checkConfiguration();
                return qMakePair(
                    result.available ? result.message : QString(),
                    result.error.message
                );
            }));
        });
        return frame;
    }

void ApiSettingsSection::showCustomModelConfigDialog()
    {
        AppDialog dialog(this);
        dialog.setWindowTitle(apiTr8("配置自定义大模型"));
        dialog.resize(760, 620);
        dialog.setStyleSheet(QStringLiteral("QDialog { background: #f6f7f9; } QLabel { color: #111827; }"));

        QVector<CustomModelProfile> profiles = m_customModelProfiles;
        auto *root = new QVBoxLayout(&dialog);
        root->setContentsMargins(22, 20, 22, 20);
        root->setSpacing(12);

        auto *top = new QHBoxLayout;
        auto *title = new QLabel(apiTr8("自定义大模型"));
        title->setFont(appFont(20, QFont::DemiBold));
        auto *add = new QPushButton(apiTr8("新增模型"));
        applyCustomModelDialogButtonSizing(add);
        top->addWidget(title, 1);
        top->addWidget(add);
        root->addLayout(top);

        auto *scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto *holder = new QWidget;
        auto *list = new QVBoxLayout(holder);
        list->setContentsMargins(0, 0, 10, 0);
        list->setSpacing(12);
        scroll->setWidget(holder);
        root->addWidget(scroll, 1);

        struct RowEditors {
            QString id;
            QLineEdit *name = nullptr;
            QLineEdit *url = nullptr;
            QLineEdit *key = nullptr;
            QLineEdit *model = nullptr;
        };
        QVector<RowEditors> rows;

        auto clearList = [&]() {
            while (QLayoutItem *item = list->takeAt(0)) {
                if (QWidget *widget = item->widget()) {
                    widget->deleteLater();
                }
                delete item;
            }
            rows.clear();
        };

        std::function<void()> rebuild = [&]() {
            clearList();
            if (profiles.isEmpty()) {
                auto *empty = new QLabel(apiTr8("还没有自定义大模型。点击右上角“新增模型”开始配置。"));
                empty->setWordWrap(true);
                empty->setAlignment(Qt::AlignCenter);
                empty->setStyleSheet(QStringLiteral("QLabel { background: #eef2ff; color: #344054; border-radius: 8px; padding: 18px; }"));
                list->addWidget(empty);
                list->addStretch();
                return;
            }
            for (int i = 0; i < profiles.size(); ++i) {
                QLineEdit *nameEdit = nullptr;
                QLineEdit *urlEdit = nullptr;
                QLineEdit *keyEdit = nullptr;
                QLineEdit *modelEdit = nullptr;
                QPushButton *deleteButton = nullptr;
                QWidget *row = customModelEditorRow(profiles.at(i), &nameEdit, &urlEdit, &keyEdit, &modelEdit, &deleteButton);
                const int rowIndex = i;
                connect(deleteButton, &QPushButton::clicked, &dialog, [&, rowIndex]() {
                    if (rowIndex >= 0 && rowIndex < profiles.size()) {
                        profiles.remove(rowIndex);
                    }
                    rebuild();
                });
                RowEditors editors;
                editors.id = profiles.at(i).id;
                editors.name = nameEdit;
                editors.url = urlEdit;
                editors.key = keyEdit;
                editors.model = modelEdit;
                rows.append(editors);
                list->addWidget(row);
            }
            list->addStretch();
        };

        connect(add, &QPushButton::clicked, &dialog, [&]() {
            CustomModelProfile profile;
            profile.id = QStringLiteral("custom_") + QString::number(QDateTime::currentMSecsSinceEpoch());
            profile.name = apiTr8("自定义大模型 ") + QString::number(profiles.size() + 1);
            profiles.append(profile);
            rebuild();
        });

        rebuild();

        auto *buttons = new QHBoxLayout;
        buttons->addStretch();
        auto *cancel = new QPushButton(apiTr8("取消"));
        applyCustomModelDialogButtonSizing(
            cancel,
            QStringLiteral("#ffffff"),
            QStringLiteral("#111827")
        );
        auto *save = new QPushButton(apiTr8("保存"));
        applyCustomModelDialogButtonSizing(save);
        buttons->addWidget(cancel);
        buttons->addWidget(save);
        root->addLayout(buttons);

        connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
        connect(save, &QPushButton::clicked, &dialog, [&]() {
            QVector<CustomModelProfile> updated;
            QSet<QString> usedIds;
            for (const RowEditors &row : rows) {
                CustomModelProfile profile;
                profile.id = normalizeCustomModelProfileId(row.id);
                while (usedIds.contains(profile.id)) {
                    profile.id += QStringLiteral("_1");
                }
                usedIds.insert(profile.id);
                profile.name = row.name ? row.name->text().trimmed() : QString();
                profile.url = row.url ? row.url->text().trimmed() : QString();
                profile.apiKey = row.key ? row.key->text().trimmed() : QString();
                profile.model = row.model ? row.model->text().trimmed() : QString();
                if (profile.name.isEmpty()) {
                    profile.name = profile.model.isEmpty() ? apiTr8("自定义大模型") : profile.model;
                }
                if (!profile.url.isEmpty()) {
                    updated.append(profile);
                }
            }
            m_customModelProfiles = updated;
            refreshCustomModelSummary();
            if (saveSecretsFromUi(false)) {
                showAttentionInformation(&dialog, apiTr8("已保存"), apiTr8("自定义大模型配置已保存。"));
                dialog.accept();
            }
        });

        dialog.exec();
    }

bool ApiSettingsSection::saveSecretsFromUi(bool showConfirmation)
    {
        SecretConfig secrets;
        secrets.deepseekApiKey = m_deepseekKeyEdit ? m_deepseekKeyEdit->text().trimmed() : QString();
        secrets.openaiApiKey = m_openaiKeyEdit ? m_openaiKeyEdit->text().trimmed() : QString();
        secrets.openaiBaseUrl = m_openaiBaseUrlEdit ? m_openaiBaseUrlEdit->text().trimmed() : QString();
        secrets.anthropicApiKey = m_anthropicKeyEdit ? m_anthropicKeyEdit->text().trimmed() : QString();
        secrets.anthropicBaseUrl = m_anthropicBaseUrlEdit ? m_anthropicBaseUrlEdit->text().trimmed() : QString();
        secrets.baiduApiKey = m_baiduApiKeyEdit ? m_baiduApiKeyEdit->text().trimmed() : QString();
        secrets.baiduSecretKey = m_baiduSecretKeyEdit ? m_baiduSecretKeyEdit->text().trimmed() : QString();
        secrets.baiduAppId = m_baiduAppIdEdit ? m_baiduAppIdEdit->text().trimmed() : QString();
        secrets.xfyunAppId = m_xfyunAppIdEdit ? m_xfyunAppIdEdit->text().trimmed() : QString();
        secrets.xfyunApiKey = m_xfyunApiKeyEdit ? m_xfyunApiKeyEdit->text().trimmed() : QString();
        secrets.xfyunApiSecret = m_xfyunApiSecretEdit ? m_xfyunApiSecretEdit->text().trimmed() : QString();
        secrets.customSpeechUrl = m_customSpeechUrlEdit ? m_customSpeechUrlEdit->text().trimmed() : QString();
        secrets.customSpeechApiKey = m_customSpeechApiKeyEdit ? m_customSpeechApiKeyEdit->text().trimmed() : QString();
        secrets.customSpeechModel = m_customSpeechModelEdit ? m_customSpeechModelEdit->text().trimmed() : QString();
        secrets.customOcrUrl = m_customOcrUrlEdit ? m_customOcrUrlEdit->text().trimmed() : QString();
        secrets.customOcrApiKey = m_customOcrApiKeyEdit ? m_customOcrApiKeyEdit->text().trimmed() : QString();
        secrets.customOcrModel = m_customOcrModelEdit ? m_customOcrModelEdit->text().trimmed() : QString();
        secrets.customModels = m_customModelProfiles;
        if (!m_customModelProfiles.isEmpty()) {
            const CustomModelProfile first = m_customModelProfiles.constFirst();
            secrets.customModelUrl = first.url;
            secrets.customModelApiKey = first.apiKey;
            secrets.customModelName = first.model;
        }

        if (!saveSecrets(secrets)) {
            showAttentionWarning(this, apiTr8("保存失败"), apiTr8("无法写入 config/secrets.json。"));
            return false;
        }
        const QString speechProvider = m_speechProviderBox
            ? m_speechProviderBox->currentData().toString()
            : snapshot().speechProvider;
        const QString ocrEngine = m_ocrProviderBox
            ? m_ocrProviderBox->currentData().toString()
            : snapshot().ocrEngine;
        const QString windowsSpeechLanguage = m_windowsSpeechSettingsCard
            ? m_windowsSpeechSettingsCard->language()
            : normalizeWindowsSpeechLanguage(
                snapshot().windowsSpeechLanguage
            );
        if (m_callbacks.saveRuntimeSettings
            && !m_callbacks.saveRuntimeSettings(
                speechProvider,
                ocrEngine,
                windowsSpeechLanguage
            )) {
            showAttentionWarning(this, apiTr8("保存失败"), apiTr8("接口密钥已保存，但无法写入语音识别服务选择。"));
            return false;
        }
        if (m_callbacks.onChanged) {
            m_callbacks.onChanged();
        }
        if (showConfirmation) {
            showAttentionInformation(this, apiTr8("已保存"), apiTr8("接口配置和语音识别服务已保存，并会在下次调用时生效。"));
        }
        return true;
    }
