#include "function_command_page.h"

#include "attention_message.h"
#include "command_center_shell.h"
#include "history_row_frame.h"
#include "hub_settings_state.h"
#include "shortcut_display.h"
#include "ui_style.h"

#include "../capture/screenshot_types.h"
#include "../config/app_settings_defaults.h"
#include "../domain/function_catalog.h"
#include "../providers/model_catalog.h"
#include "../result_flow_config.h"

#include <QtWidgets>

namespace
{

QString text8(const char *text) { return QString::fromUtf8(text); }

} // namespace

FunctionCommandPage::FunctionCommandPage(const FunctionCommandPageAccess &access, QWidget *parent)
    : QWidget(parent), m_access(access)
{
    auto *pageLayout = new QVBoxLayout(this);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);

    m_scroll = new QScrollArea;
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scroll->setFocusPolicy(Qt::WheelFocus);
    m_scroll->setStyleSheet(
        QStringLiteral("QScrollArea { background: #eef1f5; border: none; }"
                       "QScrollArea > QWidget > QWidget { background: #eef1f5; }"));

    auto *holder = new QWidget;
    m_contentLayout = new QVBoxLayout(holder);
    m_contentLayout->setContentsMargins(24, 20, 24, 28);
    m_contentLayout->setSpacing(14);
    m_scroll->setWidget(holder);
    pageLayout->addWidget(m_scroll);
}

QString FunctionCommandPage::functionId() const { return m_functionId; }

void FunctionCommandPage::setFunctionId(const QString &id)
{
    const QString normalized = id.trimmed();
    if (m_functionId == normalized)
    {
        refresh();
        return;
    }
    m_functionId = normalized;
    refresh();
}

QString FunctionCommandPage::functionTitle(const QString &id) const
{
    const AppSettingsData settings =
        m_access.settings ? m_access.settings->toData() : AppSettingsData();
    return functionDisplayTitle(settings, id, text8("功能"));
}

CustomFunctionDef FunctionCommandPage::customFunction(const QString &id, bool *customOut) const
{
    if (customOut)
    {
        *customOut = false;
    }
    if (!m_access.settings)
    {
        return CustomFunctionDef();
    }
    for (const CustomFunctionDef &function : m_access.settings->customFunctions())
    {
        if (function.id == id)
        {
            if (customOut)
            {
                *customOut = true;
            }
            return function;
        }
    }
    return CustomFunctionDef();
}

void FunctionCommandPage::saveSettings()
{
    if (m_access.saveSettings)
    {
        m_access.saveSettings();
    }
}

void FunctionCommandPage::clearLayout(QLayout *layout)
{
    while (QLayoutItem *item = layout->takeAt(0))
    {
        if (item->widget())
        {
            item->widget()->hide();
            item->widget()->deleteLater();
        }
        if (item->layout())
        {
            clearLayout(item->layout());
        }
        delete item;
    }
}

QComboBox *FunctionCommandPage::modelCombo(const QString &currentModel)
{
    auto *box = new QComboBox;
    box->setFixedHeight(36);
    for (const ModelOption &option : modelOptions())
    {
        box->addItem(option.title, option.id);
    }
    const int index =
        box->findData(normalizeModelId(currentModel, QStringLiteral("deepseek-v4-flash")));
    if (index >= 0)
    {
        box->setCurrentIndex(index);
    }
    box->setStyleSheet(QStringLiteral("QComboBox { background: #ffffff; border: 1px solid #d0d5dd; "
                                      "border-radius: 6px; padding: 6px 10px; }"));
    return box;
}

QComboBox *FunctionCommandPage::resultTemplateCombo(const QString &currentTemplate)
{
    auto *box = new QComboBox;
    box->setFixedHeight(36);
    box->addItem(resultTemplateTitle(resultTemplateSimple()), resultTemplateSimple());
    box->addItem(resultTemplateTitle(resultTemplateDetail()), resultTemplateDetail());
    box->addItem(resultTemplateTitle(resultTemplateCompare()), resultTemplateCompare());
    box->addItem(resultTemplateTitle(resultTemplateOutputOnly()), resultTemplateOutputOnly());
    const int index = box->findData(normalizeResultTemplate(currentTemplate));
    if (index >= 0)
    {
        box->setCurrentIndex(index);
    }
    box->setStyleSheet(QStringLiteral("QComboBox { background: #ffffff; border: 1px solid #d0d5dd; "
                                      "border-radius: 6px; padding: 6px 10px; }"));
    return box;
}

QSpinBox *FunctionCommandPage::displayTimeSpinBox(int seconds, bool allowManualClose,
                                                  const QString &zeroText)
{
    auto *box = new QSpinBox;
    const bool allowZero = allowManualClose || !zeroText.trimmed().isEmpty();
    box->setRange(allowZero ? 0 : 1, allowManualClose ? 600 : 60);
    box->setSuffix(text8(" 秒"));
    if (allowManualClose)
    {
        box->setSpecialValueText(text8("手动关闭"));
    }
    else if (!zeroText.trimmed().isEmpty())
    {
        box->setSpecialValueText(zeroText.trimmed());
    }
    box->setValue(seconds);
    box->setFixedSize(130, 36);
    box->setStyleSheet(QStringLiteral("QSpinBox { background: #ffffff; border: 1px solid #d0d5dd; "
                                      "border-radius: 6px; padding: 4px 8px; }"));
    return box;
}

QWidget *FunctionCommandPage::commandField(const QString &label, QWidget *control)
{
    auto *field = new QWidget;
    auto *layout = new QVBoxLayout(field);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(5);
    auto *name = new QLabel(label);
    name->setObjectName(QStringLiteral("commandMuted"));
    layout->addWidget(name);
    control->setMinimumHeight(38);
    layout->addWidget(control);
    return field;
}

QWidget *FunctionCommandPage::commandAccordionCard(const QString &title, const QString &description,
                                                   const QString &statusText, bool checked,
                                                   bool showToggle, bool expanded, QWidget *body,
                                                   const std::function<void(bool)> &onToggle)
{
    auto *card = new QFrame;
    card->setObjectName(QStringLiteral("commandMethod"));
    card->setStyleSheet(commandCenterSectionStyle());
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *header = new HistoryRowFrame;
    header->setCursor(Qt::PointingHandCursor);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(14, 12, 14, 12);
    headerLayout->setSpacing(12);
    auto *arrow = new QLabel(expanded ? text8("⌄") : text8("›"));
    arrow->setFixedWidth(18);
    arrow->setAlignment(Qt::AlignCenter);
    arrow->setStyleSheet(QStringLiteral("color: #245fc4; font-weight: 700;"));
    headerLayout->addWidget(arrow);

    auto *copy = new QVBoxLayout;
    copy->setSpacing(3);
    auto *name = new QLabel(title);
    name->setFont(appFont(11, QFont::DemiBold));
    auto *detail = new QLabel(description);
    detail->setObjectName(QStringLiteral("commandMuted"));
    detail->setWordWrap(true);
    copy->addWidget(name);
    copy->addWidget(detail);
    headerLayout->addLayout(copy, 1);

    auto *stateLabel = new QLabel(statusText);
    stateLabel->setObjectName(checked ? QStringLiteral("commandStateOn")
                                      : QStringLiteral("commandStateOff"));
    stateLabel->setAlignment(Qt::AlignCenter);
    headerLayout->addWidget(stateLabel);

    QCheckBox *toggle = nullptr;
    if (showToggle)
    {
        toggle = new QCheckBox;
        toggle->setChecked(checked);
        toggle->setCursor(Qt::PointingHandCursor);
        headerLayout->addWidget(toggle);
        connect(toggle, &QCheckBox::toggled, card,
                [onToggle](bool enabled)
                {
                    if (onToggle)
                    {
                        onToggle(enabled);
                    }
                });
    }
    layout->addWidget(header);

    body->setVisible(expanded);
    body->setStyleSheet(QStringLiteral(
        "QWidget#commandAccordionBody { background: #f7f9fc; border-top: 1px solid #d7dee9; }"
        "QLabel { background: transparent; }"
        "QComboBox, QSpinBox, QLineEdit, QKeySequenceEdit, QTextEdit {"
        "  background: #ffffff; border: 1px solid #cbd3df; border-radius: 4px; padding: 5px 9px;"
        "}"));
    body->setObjectName(QStringLiteral("commandAccordionBody"));
    layout->addWidget(body);

    header->setClickCallback(
        [body, arrow]()
        {
            const bool open = !body->isVisible();
            body->setVisible(open);
            arrow->setText(open ? text8("⌄") : text8("›"));
        });
    return card;
}

QWidget *FunctionCommandPage::commandControlSection(const QString &title, const QString &hint,
                                                    const QList<QWidget *> &rows)
{
    auto *section = new QFrame;
    section->setObjectName(QStringLiteral("commandSection"));
    section->setStyleSheet(commandCenterSectionStyle());
    auto *layout = new QVBoxLayout(section);
    layout->setContentsMargins(11, 0, 11, 11);
    layout->setSpacing(9);

    auto *header = new QWidget;
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(5, 13, 5, 4);
    auto *name = new QLabel(title);
    name->setFont(appFont(14, QFont::DemiBold));
    auto *description = new QLabel(hint);
    description->setObjectName(QStringLiteral("commandMuted"));
    description->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    headerLayout->addWidget(name);
    headerLayout->addStretch();
    headerLayout->addWidget(description);
    layout->addWidget(header);
    for (QWidget *row : rows)
    {
        layout->addWidget(row);
    }
    return section;
}

void FunctionCommandPage::refresh()
{
    if (!m_access.settings || !m_contentLayout || m_functionId.trimmed().isEmpty())
    {
        return;
    }
    clearLayout(m_contentLayout);
    const QString id = m_functionId;
    const QString title = functionTitle(id);
    bool custom = false;
    const CustomFunctionDef customFunctionData = customFunction(id, &custom);
    const QString shortcut = custom ? customFunctionData.shortcut : m_access.settings->hotkey(id);

    auto *header = new QWidget;
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 2);
    headerLayout->setSpacing(12);
    auto *name = new QLabel(title);
    name->setFont(appFont(22, QFont::DemiBold));
    auto *shortcutBadge = new QLabel(displayShortcut(shortcut));
    shortcutBadge->setObjectName(QStringLiteral("commandStateOn"));
    shortcutBadge->setAlignment(Qt::AlignCenter);
    shortcutBadge->setFont(appFont(10, QFont::DemiBold));
    headerLayout->addWidget(name);
    headerLayout->addWidget(shortcutBadge);
    headerLayout->addStretch();
    if (custom)
    {
        auto *manage = new QPushButton(text8("管理功能"));
        manage->setMinimumSize(92, 38);
        manage->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
        connect(manage, &QPushButton::clicked, header,
                [this, id, title, customFunctionData]()
                {
                    if (m_access.manageCustomFunction)
                    {
                        m_access.manageCustomFunction(id, title, customFunctionData);
                    }
                    refresh();
                });
        headerLayout->addWidget(manage);
    }
    m_contentLayout->addWidget(header);

    const auto ensureInput = [this, id](const QString &changed, bool enabled)
    {
        bool selection = m_access.settings->useSelectionFor(id);
        bool voice = m_access.settings->useVoiceFor(id);
        bool screenshot = m_access.settings->useScreenshotFor(id);
        if (changed == QStringLiteral("selection"))
            selection = enabled;
        if (changed == QStringLiteral("voice"))
            voice = enabled;
        if (changed == QStringLiteral("screenshot"))
            screenshot = enabled;
        if (!selection && !voice && !screenshot)
        {
            showAttentionInformation(this, text8("需要输入方式"),
                                     text8("至少需要启用选中文字、语音输入或截图输入中的一种。"));
            refresh();
            return false;
        }
        return true;
    };

    auto *voiceBody = new QWidget;
    auto *voiceLayout = new QVBoxLayout(voiceBody);
    voiceLayout->setContentsMargins(18, 14, 18, 16);
    voiceLayout->setSpacing(12);
    auto *voiceTitle = new QLabel(text8("语音输入设置"));
    voiceTitle->setFont(appFont(11, QFont::DemiBold));
    voiceLayout->addWidget(voiceTitle);
    auto *voiceGrid = new QGridLayout;
    voiceGrid->setHorizontalSpacing(14);
    voiceGrid->setVerticalSpacing(12);

    auto *deviceBox = new QComboBox;
    deviceBox->addItem(text8("系统默认麦克风"));
    deviceBox->setEnabled(false);

    auto *triggerBox = new QComboBox;
    triggerBox->addItem(text8("再次按快捷键结束"), QStringLiteral("toggle"));
    triggerBox->addItem(text8("按住快捷键说话"), QStringLiteral("hold"));
    triggerBox->setCurrentIndex(
        qMax(0, triggerBox->findData(m_access.settings->recordingTriggerModeFor(id))));

    auto *speechBox = new QComboBox;
    speechBox->addItem(speechProviderTitle(speechProviderBaidu()), speechProviderBaidu());
    speechBox->addItem(speechProviderTitle(speechProviderXfyun()), speechProviderXfyun());
    speechBox->addItem(speechProviderTitle(speechProviderCustom()), speechProviderCustom());
    speechBox->setCurrentIndex(qMax(0, speechBox->findData(m_access.settings->speechProvider())));

    auto *countdownBox =
        displayTimeSpinBox(m_access.settings->countdownSecondsFor(id), false, text8("不倒计时"));
    countdownBox->setFixedWidth(QWIDGETSIZE_MAX);

    auto *maxRecordingBox = new QSpinBox;
    maxRecordingBox->setRange(1, 30);
    maxRecordingBox->setSuffix(text8(" 分钟"));
    maxRecordingBox->setValue(m_access.settings->maxRecordingMinutesFor(id));

    auto *recordPath = new QLineEdit(m_access.settings->recordDirectoryPath());
    recordPath->setReadOnly(true);
    recordPath->setToolTip(m_access.settings->recordDirectoryPath());
    recordPath->setCursorPosition(0);

    voiceGrid->addWidget(commandField(text8("录音设备"), deviceBox), 0, 0);
    voiceGrid->addWidget(commandField(text8("录音方式"), triggerBox), 0, 1);
    voiceGrid->addWidget(commandField(text8("语音识别服务"), speechBox), 0, 2);
    voiceGrid->addWidget(commandField(text8("开始前倒计时"), countdownBox), 1, 0);
    voiceGrid->addWidget(commandField(text8("长录音最长时间"), maxRecordingBox), 1, 1);
    voiceGrid->addWidget(commandField(text8("录音保存位置"), recordPath), 1, 2);
    voiceGrid->setColumnStretch(0, 1);
    voiceGrid->setColumnStretch(1, 1);
    voiceGrid->setColumnStretch(2, 1);
    voiceLayout->addLayout(voiceGrid);

    auto *assistLabel = new QLabel(text8("录音辅助"));
    assistLabel->setObjectName(QStringLiteral("commandMuted"));
    voiceLayout->addWidget(assistLabel);
    auto *assist = new QHBoxLayout;
    assist->setSpacing(16);
    auto *beep = new QCheckBox(text8("提示音"));
    beep->setChecked(m_access.settings->recordingBeepEnabledFor(id));
    auto *waveform = new QCheckBox(text8("显示波形"));
    waveform->setChecked(m_access.settings->floatingBarEnabled());
    auto *hold = new QCheckBox(text8("按住说话"));
    hold->setChecked(m_access.settings->recordingTriggerModeFor(id) == QStringLiteral("hold"));
    auto *longRecording = new QCheckBox(text8("长录音自动分段"));
    longRecording->setChecked(m_access.settings->longRecordingEnabledFor(id));
    assist->addWidget(beep);
    assist->addWidget(waveform);
    assist->addWidget(hold);
    assist->addWidget(longRecording);
    assist->addStretch();
    voiceLayout->addLayout(assist);

    connect(triggerBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            voiceBody,
            [this, id, triggerBox](int)
            {
                m_access.settings->setRecordingTriggerModeFor(id,
                                                              triggerBox->currentData().toString());
                saveSettings();
            });
    connect(speechBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            voiceBody,
            [this, speechBox](int)
            {
                m_access.settings->setSpeechProvider(speechBox->currentData().toString());
                saveSettings();
            });
    connect(countdownBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), voiceBody,
            [this, id](int value)
            {
                m_access.settings->setCountdownSecondsFor(id, value);
                saveSettings();
            });
    connect(maxRecordingBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            voiceBody,
            [this, id](int value)
            {
                m_access.settings->setMaxRecordingMinutesFor(id, value);
                saveSettings();
            });
    connect(beep, &QCheckBox::toggled, voiceBody,
            [this, id](bool enabled)
            {
                m_access.settings->setRecordingBeepEnabledFor(id, enabled);
                saveSettings();
            });
    connect(waveform, &QCheckBox::toggled, voiceBody,
            [this](bool enabled)
            {
                m_access.settings->setFloatingBarEnabled(enabled);
                saveSettings();
            });
    connect(hold, &QCheckBox::toggled, voiceBody,
            [this, id](bool enabled)
            {
                m_access.settings->setRecordingTriggerModeFor(
                    id, enabled ? QStringLiteral("hold") : QStringLiteral("toggle"));
                saveSettings();
            });
    connect(longRecording, &QCheckBox::toggled, voiceBody,
            [this, id](bool enabled)
            {
                m_access.settings->setLongRecordingEnabledFor(id, enabled);
                saveSettings();
            });

    auto *selectionBody = new QWidget;
    auto *selectionLayout = new QVBoxLayout(selectionBody);
    selectionLayout->setContentsMargins(18, 14, 18, 16);
    selectionLayout->setSpacing(12);
    auto *selectionTitle = new QLabel(text8("选中文字设置"));
    selectionTitle->setFont(appFont(11, QFont::DemiBold));
    selectionLayout->addWidget(selectionTitle);
    auto *selectionMode = new QComboBox;
    selectionMode->addItem(text8("标准读取"), false);
    selectionMode->addItem(text8("强力选中"), true);
    selectionMode->setCurrentIndex(m_access.settings->strongSelectionEnabled() ? 1 : 0);
    selectionLayout->addWidget(commandField(text8("读取策略"), selectionMode));
    auto *selectionNotice = new QLabel(
        text8("只读取鼠标拖动选中的文字，不主动模拟复制。部分受限软件需要启用强力选中。"));
    selectionNotice->setWordWrap(true);
    selectionNotice->setObjectName(QStringLiteral("commandMuted"));
    selectionLayout->addWidget(selectionNotice);
    connect(selectionMode, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            selectionBody,
            [this, selectionMode](int)
            {
                m_access.settings->setStrongSelectionEnabled(selectionMode->currentData().toBool());
                saveSettings();
            });

    auto *screenshotBody = new QWidget;
    auto *screenshotLayout = new QGridLayout(screenshotBody);
    screenshotLayout->setContentsMargins(18, 14, 18, 16);
    screenshotLayout->setHorizontalSpacing(14);
    screenshotLayout->setVerticalSpacing(12);
    auto *screenshotTrigger = new QComboBox;
    screenshotTrigger->addItem(text8("原功能快捷键"), screenshotTriggerPrimary());
    screenshotTrigger->addItem(text8("独立截图快捷键"), screenshotTriggerSeparate());
    screenshotTrigger->addItem(text8("截图悬浮入口"), screenshotTriggerLauncher());
    screenshotTrigger->addItem(text8("独立快捷键和悬浮入口"),
                               screenshotTriggerSeparateAndLauncher());
    screenshotTrigger->setCurrentIndex(
        qMax(0, screenshotTrigger->findData(m_access.settings->screenshotTriggerModeFor(id))));
    auto *screenshotShortcut =
        new QKeySequenceEdit(QKeySequence(m_access.settings->screenshotShortcutFor(id)));
    auto *ocrEngineBox = new QComboBox;
    ocrEngineBox->addItem(text8("自动选择"), ocrEngineAutomatic());
    ocrEngineBox->addItem(text8("RapidOCR"), ocrEngineRapid());
    ocrEngineBox->addItem(text8("Windows 系统识别"), ocrEngineWindows());
    ocrEngineBox->addItem(text8("自定义云端识别"), ocrEngineCustomCloud());
    ocrEngineBox->addItem(text8("AI 识图"), ocrEngineVision());
    ocrEngineBox->setCurrentIndex(qMax(0, ocrEngineBox->findData(m_access.settings->ocrEngine())));
    screenshotLayout->addWidget(commandField(text8("截图触发方式"), screenshotTrigger), 0, 0);
    screenshotLayout->addWidget(commandField(text8("独立截图快捷键"), screenshotShortcut), 0, 1);
    screenshotLayout->addWidget(commandField(text8("图片识别方式"), ocrEngineBox), 0, 2);
    screenshotLayout->setColumnStretch(0, 1);
    screenshotLayout->setColumnStretch(1, 1);
    screenshotLayout->setColumnStretch(2, 1);
    connect(screenshotTrigger,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), screenshotBody,
            [this, id, screenshotTrigger](int)
            {
                m_access.settings->setScreenshotTriggerModeFor(
                    id, screenshotTrigger->currentData().toString());
                saveSettings();
            });
    connect(
        screenshotShortcut, &QKeySequenceEdit::editingFinished, screenshotBody,
        [this, id, screenshotShortcut]()
        {
            const QString value =
                screenshotShortcut->keySequence().toString(QKeySequence::PortableText).trimmed();
            if (!value.isEmpty())
            {
                m_access.settings->setScreenshotShortcutFor(id, value);
                saveSettings();
            }
        });
    connect(ocrEngineBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            screenshotBody,
            [this, ocrEngineBox](int)
            {
                m_access.settings->setOcrEngine(ocrEngineBox->currentData().toString());
                saveSettings();
            });

    QList<QWidget *> inputRows;
    inputRows << commandAccordionCard(
        text8("语音输入"), text8("使用麦克风录音，并通过当前语音识别服务转换为文字。"),
        m_access.settings->useVoiceFor(id) ? text8("已启用") : text8("已关闭"),
        m_access.settings->useVoiceFor(id), true, false, voiceBody,
        [this, id, ensureInput](bool enabled)
        {
            if (!ensureInput(QStringLiteral("voice"), enabled))
                return;
            m_access.settings->setUseVoiceFor(id, enabled);
            saveSettings();
        });
    inputRows << commandAccordionCard(
        text8("读取选中文字"), text8("读取鼠标拖动选中的文字，作为原文、上下文或处理对象。"),
        m_access.settings->useSelectionFor(id) ? text8("已启用") : text8("已关闭"),
        m_access.settings->useSelectionFor(id), true, false, selectionBody,
        [this, id, ensureInput](bool enabled)
        {
            if (!ensureInput(QStringLiteral("selection"), enabled))
                return;
            m_access.settings->setUseSelectionFor(id, enabled);
            saveSettings();
        });
    inputRows << commandAccordionCard(
        text8("截图识别"), text8("框选屏幕区域，通过本地文字识别或所选图片接口读取内容。"),
        m_access.settings->useScreenshotFor(id) ? text8("已启用") : text8("已关闭"),
        m_access.settings->useScreenshotFor(id), true, false, screenshotBody,
        [this, id, ensureInput](bool enabled)
        {
            if (!ensureInput(QStringLiteral("screenshot"), enabled))
                return;
            m_access.settings->setUseScreenshotFor(id, enabled);
            saveSettings();
        });
    m_contentLayout->addWidget(
        commandControlSection(text8("输入控制"), text8("可以同时启用多种输入方式"), inputRows));

    auto *aiBody = new QWidget;
    auto *aiLayout = new QVBoxLayout(aiBody);
    aiLayout->setContentsMargins(18, 14, 18, 16);
    aiLayout->setSpacing(12);
    auto *aiGrid = new QGridLayout;
    aiGrid->setHorizontalSpacing(14);
    auto *modelBox = modelCombo(m_access.settings->modelFor(id));
    auto *promptBox = new QComboBox;
    const QVector<PromptTargetInfo> promptItems = sharedPromptTargets(m_access.prompts);
    for (const PromptTargetInfo &target : promptItems)
    {
        promptBox->addItem(target.title, target.id);
    }
    promptBox->setCurrentIndex(qMax(0, promptBox->findData(m_access.settings->promptIdFor(id))));
    aiGrid->addWidget(commandField(text8("大模型"), modelBox), 0, 0);
    aiGrid->addWidget(commandField(text8("提示词"), promptBox), 0, 1);
    aiGrid->setColumnStretch(0, 1);
    aiGrid->setColumnStretch(1, 1);
    aiLayout->addLayout(aiGrid);
    auto *promptEditor = new QTextEdit;
    promptEditor->setMinimumHeight(150);
    promptEditor->setPlainText(sharedPromptText(
        m_access.prompts,
        sharedPromptTargetForId(m_access.prompts, promptBox->currentData().toString())));
    promptEditor->setDisabled(m_access.settings->promptLocked());
    aiLayout->addWidget(promptEditor);
    auto *promptTools = new QHBoxLayout;
    promptTools->addStretch();
    auto *savePrompt = new QPushButton(text8("保存提示词"));
    savePrompt->setMinimumHeight(36);
    savePrompt->setEnabled(!m_access.settings->promptLocked());
    savePrompt->setStyleSheet(buttonStyle(QStringLiteral("#101827")));
    promptTools->addWidget(savePrompt);
    aiLayout->addLayout(promptTools);
    connect(modelBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            aiBody,
            [this, id, modelBox](int)
            {
                m_access.settings->setModelFor(id, modelBox->currentData().toString());
                saveSettings();
            });
    connect(
        promptBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), aiBody,
        [this, id, promptBox, promptEditor](int)
        {
            m_access.settings->setPromptIdFor(id, promptBox->currentData().toString());
            promptEditor->setPlainText(sharedPromptText(
                m_access.prompts,
                sharedPromptTargetForId(m_access.prompts, promptBox->currentData().toString())));
            saveSettings();
        });
    connect(
        savePrompt, &QPushButton::clicked, aiBody,
        [this, promptBox, promptEditor]()
        {
            QString error;
            if (!saveSharedPromptText(
                    m_access.prompts,
                    sharedPromptTargetForId(m_access.prompts, promptBox->currentData().toString()),
                    promptEditor->toPlainText(), &error))
            {
                showAttentionWarning(this, text8("保存失败"), error);
                return;
            }
            saveSettings();
        });

    auto outputBody = [this, id](const QString &mode)
    {
        auto *body = new QWidget;
        auto *layout = new QGridLayout(body);
        layout->setContentsMargins(18, 14, 18, 16);
        layout->setHorizontalSpacing(14);
        layout->setVerticalSpacing(12);
        auto *templateBox = resultTemplateCombo(m_access.settings->resultTemplateFor(id));
        auto *floatingTime = displayTimeSpinBox(m_access.settings->floatingBarSecondsFor(id), false,
                                                text8("不显示"));
        floatingTime->setFixedWidth(QWIDGETSIZE_MAX);
        auto *popupTime = displayTimeSpinBox(m_access.settings->resultPopupSecondsFor(id), true);
        popupTime->setFixedWidth(QWIDGETSIZE_MAX);
        layout->addWidget(commandField(text8("结果模板"), templateBox), 0, 0);
        layout->addWidget(commandField(text8("浮动条显示时间"), floatingTime), 0, 1);
        layout->addWidget(commandField(text8("结果窗口显示时间"), popupTime), 0, 2);
        layout->setColumnStretch(0, 1);
        layout->setColumnStretch(1, 1);
        layout->setColumnStretch(2, 1);
        connect(templateBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                body,
                [this, id, templateBox](int)
                {
                    m_access.settings->setResultTemplateFor(id,
                                                            templateBox->currentData().toString());
                    saveSettings();
                });
        connect(floatingTime, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), body,
                [this, id](int value)
                {
                    m_access.settings->setFloatingBarSecondsFor(id, value);
                    saveSettings();
                });
        connect(popupTime, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), body,
                [this, id](int value)
                {
                    m_access.settings->setResultPopupSecondsFor(id, value);
                    saveSettings();
                });
        Q_UNUSED(mode);
        return body;
    };

    const QString currentOutput = m_access.settings->outputModeFor(id);
    QList<QWidget *> outputRows;
    outputRows << commandAccordionCard(text8("AI 处理"),
                                       text8("选择当前功能使用的大模型和提示词。"),
                                       modelTitle(m_access.settings->modelFor(id)), true, false,
                                       false, aiBody, std::function<void(bool)>());
    outputRows << commandAccordionCard(
        text8("自动写入"), text8("处理完成后，直接把结果写入当前输入位置。"),
        currentOutput == outputModeAutoWrite() ? text8("当前默认") : text8("可选择"),
        currentOutput == outputModeAutoWrite(), true, false, outputBody(outputModeAutoWrite()),
        [this, id, currentOutput](bool enabled)
        {
            if (!enabled && currentOutput == outputModeAutoWrite())
            {
                refresh();
                return;
            }
            if (enabled)
            {
                m_access.settings->setOutputModeFor(id, outputModeAutoWrite());
                saveSettings();
            }
        });
    outputRows << commandAccordionCard(
        text8("结果小框"), text8("先显示可编辑结果，再决定复制、写入、替换或继续追问。"),
        currentOutput == outputModePopup() ? text8("当前默认") : text8("可选择"),
        currentOutput == outputModePopup(), true, false, outputBody(outputModePopup()),
        [this, id, currentOutput](bool enabled)
        {
            if (!enabled && currentOutput == outputModePopup())
            {
                refresh();
                return;
            }
            if (enabled)
            {
                m_access.settings->setOutputModeFor(id, outputModePopup());
                saveSettings();
            }
        });
    outputRows << commandAccordionCard(
        text8("截图对照窗口"), text8("以截图原图、识别结果或翻译对照方式展示处理结果。"),
        currentOutput == outputModeScreenshotPanel() ? text8("当前默认") : text8("可选择"),
        currentOutput == outputModeScreenshotPanel(), true, false,
        outputBody(outputModeScreenshotPanel()),
        [this, id, currentOutput](bool enabled)
        {
            if (!enabled && currentOutput == outputModeScreenshotPanel())
            {
                refresh();
                return;
            }
            if (enabled)
            {
                m_access.settings->setOutputModeFor(id, outputModeScreenshotPanel());
                saveSettings();
            }
        });
    m_contentLayout->addWidget(
        commandControlSection(text8("输出控制"), text8("选择默认展现方式"), outputRows));
    m_contentLayout->addStretch();
    if (m_scroll)
    {
        m_scroll->verticalScrollBar()->setValue(0);
        m_scroll->horizontalScrollBar()->setValue(0);
    }
}
