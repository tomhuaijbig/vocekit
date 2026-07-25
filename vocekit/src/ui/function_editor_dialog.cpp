#include "function_editor_dialog.h"

#include "app_dialogs.h"
#include "attention_message.h"
#include "hub_settings_state.h"
#include "shortcut_display.h"
#include "toggle_switch.h"
#include "ui_style.h"

#include "../capture/screenshot_types.h"
#include "../config/app_settings_defaults.h"
#include "../providers/model_catalog.h"

#include <QtWidgets>

namespace {

QString text8(const char *text)
{
    return QString::fromUtf8(text);
}

QComboBox *modelCombo(const QString &currentModel)
{
    auto *box = new QComboBox;
    box->setFixedHeight(36);
    for (const ModelOption &option : modelOptions()) {
        box->addItem(option.title, option.id);
    }
    const int modelIndex = box->findData(
        normalizeModelId(currentModel, QStringLiteral("deepseek-v4-flash"))
    );
    if (modelIndex >= 0) {
        box->setCurrentIndex(modelIndex);
    }
    box->setStyleSheet(QStringLiteral(
        "QComboBox { background: #ffffff; border: 1px solid #d0d5dd; "
        "border-radius: 6px; padding: 6px 10px; }"
    ));
    return box;
}

QComboBox *outputModeCombo(const QString &currentMode)
{
    auto *box = new QComboBox;
    box->setFixedHeight(36);
    box->addItem(outputModeTitle(outputModeAutoWrite()), outputModeAutoWrite());
    box->addItem(outputModeTitle(outputModePopup()), outputModePopup());
    box->addItem(
        outputModeTitle(outputModeScreenshotPanel()),
        outputModeScreenshotPanel()
    );
    const int modeIndex = box->findData(
        normalizeOutputMode(currentMode, outputModePopup())
    );
    if (modeIndex >= 0) {
        box->setCurrentIndex(modeIndex);
    }
    box->setStyleSheet(QStringLiteral(
        "QComboBox { background: #ffffff; border: 1px solid #d0d5dd; "
        "border-radius: 6px; padding: 6px 10px; }"
    ));
    return box;
}

QComboBox *resultTemplateCombo(const QString &currentTemplate)
{
    auto *box = new QComboBox;
    box->setFixedHeight(36);
    box->addItem(resultTemplateTitle(resultTemplateSimple()), resultTemplateSimple());
    box->addItem(resultTemplateTitle(resultTemplateDetail()), resultTemplateDetail());
    box->addItem(resultTemplateTitle(resultTemplateCompare()), resultTemplateCompare());
    box->addItem(
        resultTemplateTitle(resultTemplateOutputOnly()),
        resultTemplateOutputOnly()
    );
    const int templateIndex = box->findData(
        normalizeResultTemplate(currentTemplate)
    );
    if (templateIndex >= 0) {
        box->setCurrentIndex(templateIndex);
    }
    box->setStyleSheet(QStringLiteral(
        "QComboBox { background: #ffffff; border: 1px solid #d0d5dd; "
        "border-radius: 6px; padding: 6px 10px; }"
    ));
    return box;
}

QSpinBox *displayTimeSpinBox(
    int seconds,
    bool allowManualClose,
    const QString &zeroText = QString()
)
{
    auto *box = new QSpinBox;
    const bool allowZero = allowManualClose || !zeroText.trimmed().isEmpty();
    box->setRange(allowZero ? 0 : 1, allowManualClose ? 600 : 60);
    box->setSuffix(text8(" 秒"));
    if (allowManualClose) {
        box->setSpecialValueText(text8("手动关闭"));
    } else if (!zeroText.trimmed().isEmpty()) {
        box->setSpecialValueText(zeroText.trimmed());
    }
    box->setValue(seconds);
    box->setFixedSize(130, 36);
    box->setStyleSheet(QStringLiteral(
        "QSpinBox { background: #ffffff; border: 1px solid #d0d5dd; "
        "border-radius: 6px; padding: 4px 8px; }"
    ));
    return box;
}

QWidget *dialogSection(const QString &title, QVBoxLayout **contentOut = nullptr)
{
    auto *frame = new QFrame;
    frame->setObjectName(QStringLiteral("card"));
    frame->setStyleSheet(cardStyle());

    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(12);

    auto *name = new QLabel(title);
    name->setFont(appFont(13, QFont::DemiBold));
    layout->addWidget(name);

    auto *sectionContentLayout = new QVBoxLayout;
    sectionContentLayout->setContentsMargins(0, 0, 0, 0);
    sectionContentLayout->setSpacing(10);
    layout->addLayout(sectionContentLayout);
    if (contentOut) {
        *contentOut = sectionContentLayout;
    }
    return frame;
}

} // namespace

bool runFunctionEditorDialog(
    const FunctionEditorDialogRequest &request,
    const FunctionEditorDialogAccess &access,
    QWidget *parent
)
{
    HubSettingsState *settings = access.settings;
    if (!settings) {
        return false;
    }

    const QString id = request.id;
    const QString title = request.title;
    const bool custom = request.custom;
    const CustomFunctionDef function = request.function;
    const PromptSettingsAccess promptAccess = access.prompts;

    AppDialog dialog(parent);
    dialog.setWindowTitle(custom ? text8("编辑自定义功能") : text8("编辑内置功能"));
    dialog.setMinimumSize(820, 680);
    dialog.setFont(appFont());
    dialog.setStyleSheet(QStringLiteral(
        "QDialog { background: #f6f7f9; }"
        "QLabel { color: #111827; }"
    ));

    auto *root = new QVBoxLayout(&dialog);
    root->setContentsMargins(22, 20, 22, 18);
    root->setSpacing(14);

    auto *header = new QHBoxLayout;
    auto *titleBox = new QVBoxLayout;
    auto *dialogTitle = new QLabel(title);
    dialogTitle->setFont(appFont(20, QFont::DemiBold));
    auto *summary = new QLabel(request.summaryText);
    summary->setWordWrap(true);
    summary->setStyleSheet(QStringLiteral("color: #047857; font-weight: 600;"));
    titleBox->addWidget(dialogTitle);
    titleBox->addWidget(summary);

    auto *type = new QLabel(custom ? text8("自定义") : text8("内置"));
    type->setAlignment(Qt::AlignCenter);
    type->setMinimumSize(72, 32);
    type->setStyleSheet(QStringLiteral(
        "QLabel { background: #eef2ff; color: #1d4ed8; border-radius: 6px; "
        "font-weight: 600; }"
    ));
    header->addLayout(titleBox, 1);
    header->addWidget(type, 0, Qt::AlignTop);
    root->addLayout(header);

    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QStringLiteral(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollArea > QWidget > QWidget { background: transparent; }"
    ));
    auto *holder = new QWidget;
    auto *contentLayout = new QVBoxLayout(holder);
    contentLayout->setContentsMargins(0, 0, 10, 0);
    contentLayout->setSpacing(12);

    QLineEdit *nameEdit = nullptr;
    QVBoxLayout *basicContent = nullptr;
    auto *basicSection = dialogSection(text8("基础信息"), &basicContent);
    auto *basicForm = new QGridLayout;
    basicForm->setHorizontalSpacing(12);
    basicForm->setVerticalSpacing(10);
    int basicRow = 0;
    if (custom) {
        nameEdit = new QLineEdit(function.name);
        nameEdit->setMinimumHeight(36);
        basicForm->addWidget(new QLabel(text8("名称")), basicRow, 0);
        basicForm->addWidget(nameEdit, basicRow, 1);
        ++basicRow;
    } else {
        auto *fixedName = new QLabel(title);
        fixedName->setMinimumHeight(36);
        fixedName->setStyleSheet(QStringLiteral(
            "QLabel { background: #f9fafb; border: 1px solid #eef0f4; "
            "border-radius: 6px; padding: 7px 10px; }"
        ));
        basicForm->addWidget(new QLabel(text8("名称")), basicRow, 0);
        basicForm->addWidget(fixedName, basicRow, 1);
        ++basicRow;
    }

    auto *shortcutEdit = new QKeySequenceEdit(
        QKeySequence(custom ? function.shortcut : settings->hotkey(id))
    );
    shortcutEdit->setMinimumHeight(36);
    basicForm->addWidget(new QLabel(text8("快捷键")), basicRow, 0);
    basicForm->addWidget(shortcutEdit, basicRow, 1);
    basicForm->setColumnStretch(1, 1);
    basicContent->addLayout(basicForm);
    contentLayout->addWidget(basicSection);

    QVBoxLayout *modeContent = nullptr;
    auto *modeSection = dialogSection(text8("模型和输入"), &modeContent);
    auto *modeForm = new QGridLayout;
    modeForm->setHorizontalSpacing(12);
    modeForm->setVerticalSpacing(10);
    auto *modelBox = modelCombo(settings->modelFor(id));
    auto *outputBox = outputModeCombo(settings->outputModeFor(id));
    auto *templateBox = resultTemplateCombo(settings->resultTemplateFor(id));
    auto *useSelection = new QCheckBox(text8("读取鼠标选中的文字"));
    useSelection->setChecked(settings->useSelectionFor(id));
    useSelection->setFont(appFont(10, QFont::DemiBold));
    auto *useVoice = new QCheckBox(text8("使用语音输入"));
    useVoice->setChecked(settings->useVoiceFor(id));
    useVoice->setFont(appFont(10, QFont::DemiBold));
    auto *useScreenshot = new QCheckBox(text8("使用截图输入"));
    useScreenshot->setChecked(settings->useScreenshotFor(id));
    useScreenshot->setFont(appFont(10, QFont::DemiBold));
    auto *inputBox = new QWidget;
    auto *inputLayout = new QHBoxLayout(inputBox);
    inputLayout->setContentsMargins(0, 0, 0, 0);
    inputLayout->setSpacing(14);
    inputLayout->addWidget(labeledSwitch(useSelection, text8("读取鼠标选中的文字")));
    inputLayout->addWidget(labeledSwitch(useVoice, text8("使用语音输入")));
    inputLayout->addWidget(labeledSwitch(useScreenshot, text8("使用截图输入")));
    inputLayout->addStretch();

    auto *screenshotOptions = new QWidget;
    auto *screenshotLayout = new QGridLayout(screenshotOptions);
    screenshotLayout->setContentsMargins(0, 0, 0, 0);
    screenshotLayout->setHorizontalSpacing(12);
    screenshotLayout->setVerticalSpacing(8);
    auto *screenshotTriggerBox = new QComboBox;
    screenshotTriggerBox->setMinimumHeight(36);
    screenshotTriggerBox->addItem(text8("原功能快捷键"), screenshotTriggerPrimary());
    screenshotTriggerBox->addItem(text8("独立截图快捷键"), screenshotTriggerSeparate());
    screenshotTriggerBox->addItem(text8("截图悬浮入口"), screenshotTriggerLauncher());
    screenshotTriggerBox->addItem(
        text8("独立快捷键和悬浮入口"),
        screenshotTriggerSeparateAndLauncher()
    );
    const int screenshotTriggerIndex = screenshotTriggerBox->findData(
        settings->screenshotTriggerModeFor(id)
    );
    screenshotTriggerBox->setCurrentIndex(
        screenshotTriggerIndex >= 0 ? screenshotTriggerIndex : 1
    );
    auto *screenshotShortcutEdit = new QKeySequenceEdit(
        QKeySequence(settings->screenshotShortcutFor(id))
    );
    screenshotShortcutEdit->setMinimumHeight(36);
    auto *screenshotShortcutLabel = new QLabel(text8("截图快捷键"));
    screenshotLayout->addWidget(new QLabel(text8("截图触发")), 0, 0);
    screenshotLayout->addWidget(screenshotTriggerBox, 0, 1);
    screenshotLayout->addWidget(screenshotShortcutLabel, 0, 2);
    screenshotLayout->addWidget(screenshotShortcutEdit, 0, 3);
    screenshotLayout->setColumnStretch(1, 1);
    screenshotLayout->setColumnStretch(3, 1);
    const auto refreshScreenshotOptions = [
        useScreenshot,
        screenshotOptions,
        screenshotTriggerBox,
        screenshotShortcutLabel,
        screenshotShortcutEdit
    ]() {
        const bool enabled = useScreenshot->isChecked();
        screenshotOptions->setVisible(enabled);
        const bool usesSeparate = screenshotTriggerUsesSeparate(
            screenshotTriggerBox->currentData().toString()
        );
        screenshotShortcutLabel->setVisible(enabled && usesSeparate);
        screenshotShortcutEdit->setVisible(enabled && usesSeparate);
    };
    QObject::connect(
        useScreenshot,
        &QCheckBox::toggled,
        &dialog,
        [refreshScreenshotOptions]() { refreshScreenshotOptions(); }
    );
    QObject::connect(
        screenshotTriggerBox,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        &dialog,
        [refreshScreenshotOptions](int) { refreshScreenshotOptions(); }
    );
    refreshScreenshotOptions();
    modeForm->addWidget(new QLabel(text8("模型")), 0, 0);
    modeForm->addWidget(modelBox, 0, 1);
    modeForm->addWidget(new QLabel(text8("展现方式")), 0, 2);
    modeForm->addWidget(outputBox, 0, 3);
    modeForm->addWidget(new QLabel(text8("输入方式")), 1, 0);
    modeForm->addWidget(inputBox, 1, 1, 1, 3);
    modeForm->addWidget(screenshotOptions, 2, 0, 1, 4);
    modeForm->addWidget(new QLabel(text8("结果模板")), 3, 0);
    modeForm->addWidget(templateBox, 3, 1, 1, 3);
    modeForm->setColumnStretch(1, 1);
    modeForm->setColumnStretch(3, 1);
    modeContent->addLayout(modeForm);
    contentLayout->addWidget(modeSection);

    QVBoxLayout *recordingContent = nullptr;
    auto *recordingSection = dialogSection(text8("录音方式"), &recordingContent);
    auto *recordingForm = new QGridLayout;
    recordingForm->setHorizontalSpacing(12);
    recordingForm->setVerticalSpacing(10);
    auto *triggerModeBox = new QComboBox;
    triggerModeBox->setMinimumHeight(36);
    triggerModeBox->addItem(text8("切换"), QStringLiteral("toggle"));
    triggerModeBox->addItem(text8("按住说话"), QStringLiteral("hold"));
    const int triggerModeIndex = triggerModeBox->findData(
        settings->recordingTriggerModeFor(id)
    );
    if (triggerModeIndex >= 0) {
        triggerModeBox->setCurrentIndex(triggerModeIndex);
    }
    auto *longRecordingEnabled = new QCheckBox;
    longRecordingEnabled->setChecked(settings->longRecordingEnabledFor(id));
    auto *segmentSeconds = new QSpinBox;
    segmentSeconds->setRange(20, 55);
    segmentSeconds->setSuffix(text8(" 秒"));
    segmentSeconds->setValue(settings->segmentSecondsFor(id));
    segmentSeconds->setMinimumHeight(36);
    auto *maxRecordingMinutes = new QSpinBox;
    maxRecordingMinutes->setRange(1, 30);
    maxRecordingMinutes->setSuffix(text8(" 分钟"));
    maxRecordingMinutes->setValue(settings->maxRecordingMinutesFor(id));
    maxRecordingMinutes->setMinimumHeight(36);
    auto *longRecordingSwitch = labeledSwitch(
        longRecordingEnabled,
        text8("允许长录音")
    );
    recordingForm->addWidget(new QLabel(text8("录音方式")), 0, 0);
    recordingForm->addWidget(triggerModeBox, 0, 1);
    recordingForm->addWidget(new QLabel(text8("长录音")), 0, 2);
    recordingForm->addWidget(longRecordingSwitch, 0, 3);
    recordingForm->addWidget(new QLabel(text8("每段时长")), 1, 0);
    recordingForm->addWidget(segmentSeconds, 1, 1);
    recordingForm->addWidget(new QLabel(text8("最长录音")), 1, 2);
    recordingForm->addWidget(maxRecordingMinutes, 1, 3);
    recordingForm->setColumnStretch(1, 1);
    recordingForm->setColumnStretch(3, 1);
    recordingContent->addLayout(recordingForm);
    const auto refreshRecordingControls = [
        longRecordingEnabled,
        segmentSeconds,
        maxRecordingMinutes
    ](bool) {
        const bool enabled = longRecordingEnabled->isChecked();
        segmentSeconds->setEnabled(enabled);
        maxRecordingMinutes->setEnabled(enabled);
    };
    refreshRecordingControls(longRecordingEnabled->isChecked());
    QObject::connect(
        longRecordingEnabled,
        &QCheckBox::toggled,
        &dialog,
        refreshRecordingControls
    );
    recordingSection->setVisible(useVoice->isChecked());
    QObject::connect(
        useVoice,
        &QCheckBox::toggled,
        recordingSection,
        &QWidget::setVisible
    );
    contentLayout->addWidget(recordingSection);

    QVBoxLayout *timeContent = nullptr;
    auto *timeSection = dialogSection(text8("显示时间"), &timeContent);
    auto *timeForm = new QGridLayout;
    timeForm->setHorizontalSpacing(12);
    timeForm->setVerticalSpacing(10);
    auto *floatingTime = displayTimeSpinBox(
        settings->floatingBarSecondsFor(id),
        false,
        text8("不调用")
    );
    auto *popupTime = displayTimeSpinBox(
        settings->resultPopupSecondsFor(id),
        true
    );
    auto *countdownTime = displayTimeSpinBox(
        settings->countdownSecondsFor(id),
        false,
        text8("不调用")
    );
    auto *beepEnabled = new QCheckBox;
    beepEnabled->setChecked(settings->recordingBeepEnabledFor(id));
    beepEnabled->setFixedWidth(52);
    auto *beepPathEdit = new QLineEdit(settings->recordingBeepPathFor(id));
    beepPathEdit->setReadOnly(true);
    beepPathEdit->setMinimumHeight(36);
    beepPathEdit->setPlaceholderText(text8("系统提示音"));
    beepPathEdit->setStyleSheet(QStringLiteral(
        "QLineEdit { background: #ffffff; border: 1px solid #d0d5dd; "
        "border-radius: 6px; padding: 6px 10px; }"
    ));
    auto *chooseBeep = new QPushButton(text8("选择声音"));
    chooseBeep->setFixedHeight(36);
    chooseBeep->setStyleSheet(
        buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827"))
    );
    auto *clearBeep = new QPushButton(text8("清除"));
    clearBeep->setFixedHeight(36);
    clearBeep->setStyleSheet(
        buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827"))
    );
    auto *beepBox = new QWidget;
    auto *beepLayout = new QHBoxLayout(beepBox);
    beepLayout->setContentsMargins(0, 0, 0, 0);
    beepLayout->setSpacing(8);
    beepLayout->addWidget(beepEnabled);
    beepLayout->addWidget(beepPathEdit, 1);
    beepLayout->addWidget(chooseBeep);
    beepLayout->addWidget(clearBeep);
    const auto refreshBeepControls = [
        beepEnabled,
        beepPathEdit,
        chooseBeep,
        clearBeep
    ](bool) {
        const bool enabled = beepEnabled->isChecked();
        beepPathEdit->setEnabled(enabled);
        chooseBeep->setEnabled(enabled);
        clearBeep->setEnabled(enabled);
    };
    refreshBeepControls(beepEnabled->isChecked());
    QObject::connect(beepEnabled, &QCheckBox::toggled, &dialog, refreshBeepControls);
    QObject::connect(chooseBeep, &QPushButton::clicked, &dialog, [beepPathEdit, &dialog]() {
        const QString path = QFileDialog::getOpenFileName(
            &dialog,
            text8("选择录音提示音"),
            QString(),
            text8("声音文件 (*.wav);;所有文件 (*.*)")
        );
        if (!path.trimmed().isEmpty()) {
            beepPathEdit->setText(QDir::cleanPath(path));
        }
    });
    QObject::connect(clearBeep, &QPushButton::clicked, &dialog, [beepPathEdit]() {
        beepPathEdit->clear();
    });
    timeForm->addWidget(new QLabel(text8("浮动条")), 0, 0);
    timeForm->addWidget(floatingTime, 0, 1);
    timeForm->addWidget(new QLabel(text8("结果小框")), 0, 2);
    timeForm->addWidget(popupTime, 0, 3);
    timeForm->addWidget(new QLabel(text8("录音倒计时")), 1, 0);
    timeForm->addWidget(countdownTime, 1, 1);
    timeForm->addWidget(new QLabel(text8("录音提示音")), 1, 2);
    timeForm->addWidget(beepBox, 1, 3);
    timeForm->setColumnStretch(1, 1);
    timeForm->setColumnStretch(3, 1);
    timeContent->addLayout(timeForm);
    contentLayout->addWidget(timeSection);

    QVBoxLayout *promptContent = nullptr;
    auto *promptSection = dialogSection(text8("提示词"), &promptContent);
    auto *promptTools = new QHBoxLayout;
    promptTools->setContentsMargins(0, 0, 0, 0);
    promptTools->setSpacing(10);
    auto *promptBox = new QComboBox;
    promptBox->setMinimumHeight(36);
    promptBox->setMinimumWidth(320);
    const QVector<PromptTargetInfo> targets = sharedPromptTargets(promptAccess);
    for (const PromptTargetInfo &target : targets) {
        promptBox->addItem(target.title, target.id);
    }
    const int promptIndex = promptBox->findData(settings->promptIdFor(id));
    if (promptIndex >= 0) {
        promptBox->setCurrentIndex(promptIndex);
    }
    promptBox->setStyleSheet(QStringLiteral(
        "QComboBox { background: #ffffff; border: 1px solid #d0d5dd; "
        "border-radius: 6px; padding: 6px 10px; color: #111827; }"
        "QComboBox QAbstractItemView { background: #ffffff; "
        "border: 1px solid #9ca3af; selection-background-color: #2563eb; "
        "selection-color: #ffffff; outline: 0; }"
    ));
    promptTools->addWidget(promptBox);
    promptTools->addStretch();
    promptContent->addLayout(promptTools);

    auto *promptEditor = new QTextEdit;
    promptEditor->setMinimumHeight(220);
    promptEditor->setPlainText(sharedPromptText(
        promptAccess,
        sharedPromptTargetForId(promptAccess, promptBox->currentData().toString())
    ));
    promptEditor->setDisabled(settings->promptLocked());
    promptEditor->setStyleSheet(QStringLiteral(
        "QTextEdit { background: #ffffff; border: 1px solid #dde2ea; "
        "border-radius: 8px; padding: 10px; }"
    ));
    promptContent->addWidget(promptEditor);
    QObject::connect(
        promptBox,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        &dialog,
        [promptAccess, promptBox, promptEditor](int) {
            promptEditor->setPlainText(sharedPromptText(
                promptAccess,
                sharedPromptTargetForId(
                    promptAccess,
                    promptBox->currentData().toString()
                )
            ));
        }
    );
    contentLayout->addWidget(promptSection);

    contentLayout->addStretch();
    scroll->setWidget(holder);
    root->addWidget(scroll, 1);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    auto *cancel = new QPushButton(text8("取消"));
    cancel->setFixedHeight(38);
    cancel->setStyleSheet(
        buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827"))
    );
    auto *save = new QPushButton(text8("保存"));
    save->setFixedHeight(38);
    save->setStyleSheet(buttonStyle(QStringLiteral("#111827")));
    buttons->addWidget(cancel);
    buttons->addWidget(save);
    root->addLayout(buttons);

    QObject::connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(
        save,
        &QPushButton::clicked,
        &dialog,
        [&dialog, access, promptAccess, settings, id, title, custom, function,
         nameEdit, shortcutEdit, modelBox, outputBox, templateBox,
         useSelection, useVoice, useScreenshot, screenshotTriggerBox,
         screenshotShortcutEdit, triggerModeBox, longRecordingEnabled,
         segmentSeconds, maxRecordingMinutes, floatingTime, popupTime,
         countdownTime, beepEnabled, beepPathEdit, promptBox, promptEditor]() {
            const QString shortcut = shortcutEdit
                ->keySequence()
                .toString(QKeySequence::PortableText)
                .trimmed();
            const QString screenshotShortcut = screenshotShortcutEdit
                ->keySequence()
                .toString(QKeySequence::PortableText)
                .trimmed();
            const QString screenshotTrigger = screenshotTriggerBox
                ->currentData()
                .toString();
            const QString name = custom && nameEdit
                ? nameEdit->text().trimmed()
                : title;
            const QString selectedPromptId = promptBox->currentData().toString();
            if (name.isEmpty()) {
                showAttentionWarning(
                    &dialog,
                    text8("名称不能为空"),
                    text8("请填写功能名称。")
                );
                return;
            }
            if (shortcut.isEmpty()) {
                showAttentionWarning(
                    &dialog,
                    text8("快捷键不能为空"),
                    text8("请设置快捷键。")
                );
                return;
            }
            QString otherTitle;
            if (settings->conflictsWithOther(id, shortcut, &otherTitle)) {
                showAttentionWarning(
                    &dialog,
                    text8("快捷键冲突"),
                    text8("无法把快捷键“") + displayShortcut(shortcut)
                        + text8("”分配给“") + name
                        + text8("”，因为它已经被“") + otherTitle
                        + text8("”使用。请修改其中一个快捷键。")
                );
                return;
            }
            if (!useSelection->isChecked()
                && !useVoice->isChecked()
                && !useScreenshot->isChecked()) {
                showAttentionInformation(
                    &dialog,
                    text8("需要输入方式"),
                    text8("至少需要启用选中文字、语音输入或截图输入中的一种。")
                );
                return;
            }
            if (useScreenshot->isChecked()
                && screenshotTriggerUsesSeparate(screenshotTrigger)
                && screenshotShortcut.isEmpty()) {
                showAttentionWarning(
                    &dialog,
                    text8("截图快捷键不能为空"),
                    text8("当前截图触发方式需要设置独立截图快捷键。")
                );
                return;
            }
            if (useScreenshot->isChecked()
                && screenshotTriggerUsesSeparate(screenshotTrigger)
                && screenshotShortcut.compare(shortcut, Qt::CaseInsensitive) == 0) {
                showAttentionWarning(
                    &dialog,
                    text8("快捷键冲突"),
                    text8("功能快捷键和独立截图快捷键不能相同。")
                );
                return;
            }
            if (useScreenshot->isChecked()
                && screenshotTriggerUsesSeparate(screenshotTrigger)
                && settings->conflictsWithOther(
                    screenshotHotkeyLogicalId(id),
                    screenshotShortcut,
                    &otherTitle)) {
                showAttentionWarning(
                    &dialog,
                    text8("快捷键冲突"),
                    text8("无法把截图快捷键“")
                        + displayShortcut(screenshotShortcut)
                        + text8("”分配给“") + name
                        + text8("截图”，因为它已经被“")
                        + otherTitle
                        + text8("”使用。请修改其中一个快捷键。")
                );
                return;
            }

            if (custom) {
                CustomFunctionDef updated = function;
                updated.name = name;
                updated.shortcut = shortcut;
                updated.model = modelBox->currentData().toString();
                updated.outputMode = outputBox->currentData().toString();
                updated.resultTemplate = templateBox->currentData().toString();
                updated.useSelection = useSelection->isChecked();
                updated.useVoice = useVoice->isChecked();
                updated.useScreenshot = useScreenshot->isChecked();
                updated.screenshotTriggerMode = screenshotTrigger;
                updated.screenshotShortcut = screenshotShortcut;
                updated.floatingBarSeconds = floatingTime->value();
                updated.resultPopupSeconds = popupTime->value();
                updated.countdownSeconds = countdownTime->value();
                updated.recordingBeepEnabled = beepEnabled->isChecked();
                updated.recordingBeepPath = beepPathEdit->text().trimmed();
                updated.recordingTriggerMode = triggerModeBox->currentData().toString();
                updated.longRecordingEnabled = longRecordingEnabled->isChecked();
                updated.segmentSeconds = segmentSeconds->value();
                updated.maxRecordingMinutes = maxRecordingMinutes->value();
                updated.promptId = selectedPromptId;
                if (!settings->promptLocked() && selectedPromptId == id) {
                    updated.prompt = promptEditor->toPlainText();
                }
                settings->updateCustomFunction(updated);
            } else {
                settings->setHotkey(id, shortcut);
                settings->setModelFor(id, modelBox->currentData().toString());
                settings->setOutputModeFor(id, outputBox->currentData().toString());
                settings->setResultTemplateFor(id, templateBox->currentData().toString());
                settings->setUseSelectionFor(id, useSelection->isChecked());
                settings->setUseVoiceFor(id, useVoice->isChecked());
                settings->setUseScreenshotFor(id, useScreenshot->isChecked());
                settings->setScreenshotTriggerModeFor(id, screenshotTrigger);
                settings->setScreenshotShortcutFor(id, screenshotShortcut);
                settings->setFloatingBarSecondsFor(id, floatingTime->value());
                settings->setResultPopupSecondsFor(id, popupTime->value());
                settings->setCountdownSecondsFor(id, countdownTime->value());
                settings->setRecordingBeepEnabledFor(id, beepEnabled->isChecked());
                settings->setRecordingBeepPathFor(id, beepPathEdit->text().trimmed());
                settings->setRecordingTriggerModeFor(
                    id,
                    triggerModeBox->currentData().toString()
                );
                settings->setLongRecordingEnabledFor(
                    id,
                    longRecordingEnabled->isChecked()
                );
                settings->setSegmentSecondsFor(id, segmentSeconds->value());
                settings->setMaxRecordingMinutesFor(
                    id,
                    maxRecordingMinutes->value()
                );
                settings->setPromptIdFor(id, selectedPromptId);
            }

            if (!settings->promptLocked()
                && !(custom && selectedPromptId == id)) {
                QString error;
                if (!saveSharedPromptText(
                    promptAccess,
                    sharedPromptTargetForId(promptAccess, selectedPromptId),
                    promptEditor->toPlainText(),
                    &error)) {
                    showAttentionWarning(
                        &dialog,
                        text8("保存失败"),
                        error.isEmpty() ? text8("无法保存提示词。") : error
                    );
                    return;
                }
            }

            if (access.saveSettings) {
                access.saveSettings();
            }
            showAttentionInformation(
                &dialog,
                text8("已保存"),
                text8("功能配置已保存。")
            );
            dialog.accept();
        }
    );

    return dialog.exec() == QDialog::Accepted;
}
