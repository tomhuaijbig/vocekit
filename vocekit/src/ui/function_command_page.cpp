#include "function_command_page.h"

#include "attention_message.h"
#include "command_center_shell.h"
#include "function_canvas_editor.h"
#include "floating_bar_style_selector.h"
#include "history_row_frame.h"
#include "hub_settings_state.h"
#include "reorderable_card_column.h"
#include "shortcut_display.h"
#include "ui_style.h"

#include "../capture/screenshot_types.h"
#include "../config/app_settings_defaults.h"
#include "../domain/function_catalog.h"
#include "../domain/prompt_runtime_library.h"
#include "../providers/model_catalog.h"
#include "../result_flow_config.h"

#include <QtWidgets>

namespace
{

QString text8(const char *text) { return QString::fromUtf8(text); }

QString ocrEngineTitleForCanvas(const QString &id)
{
    if (id == ocrEngineAutomatic()) {
        return text8("自动选择");
    }
    if (id == ocrEngineRapid()) {
        return text8("Rapid OCR");
    }
    if (id == ocrEngineWindows()) {
        return text8("Windows OCR");
    }
    if (id == ocrEngineCustomCloud()) {
        return text8("自定义云 OCR");
    }
    if (id == ocrEngineVision()) {
        return text8("视觉模型");
    }
    return id;
}

} // namespace

FunctionCommandPage::FunctionCommandPage(const FunctionCommandPageAccess &access, QWidget *parent)
    : QWidget(parent), m_access(access)
{
    auto *pageLayout = new QVBoxLayout(this);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);

    m_pageStack = new QStackedWidget(this);
    m_scroll = new QScrollArea(m_pageStack);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scroll->setFocusPolicy(Qt::WheelFocus);
    m_scroll->setStyleSheet(
        QStringLiteral("QScrollArea { background: #eef1f5; border: none; }"
                       "QScrollArea > QWidget > QWidget { background: #eef1f5; }"));

    auto *holder = new QWidget;
    m_settingsLayout = new QVBoxLayout(holder);
    m_settingsLayout->setContentsMargins(24, 20, 24, 28);
    m_settingsLayout->setSpacing(14);
    m_scroll->setWidget(holder);

    m_canvasHost = new QWidget(m_pageStack);
    m_canvasHost->setObjectName(QStringLiteral("functionCanvasPageHost"));
    m_canvasLayout = new QVBoxLayout(m_canvasHost);
    m_canvasLayout->setContentsMargins(24, 12, 24, 12);
    m_canvasLayout->setSpacing(8);

    m_pageStack->addWidget(m_scroll);
    m_pageStack->addWidget(m_canvasHost);
    m_pageStack->setCurrentWidget(m_scroll);
    m_contentLayout = m_settingsLayout;
    pageLayout->addWidget(m_pageStack);
}

QString FunctionCommandPage::functionId() const { return m_functionId; }

bool FunctionCommandPage::setFunctionId(const QString &id)
{
    const QString normalized = id.trimmed();
    if (m_functionId == normalized)
    {
        refresh();
        return true;
    }
    if (m_canvasEditor
        && !m_canvasEditor->setFunctionId(
            normalized,
            flowPlacementDefaults(normalized)
        )) {
        return false;
    }
    m_functionId = normalized;
    m_canvasMode = false;
    refresh();
    return true;
}

bool FunctionCommandPage::flushPendingFlowDraft()
{
    return !m_canvasEditor
        || m_canvasEditor->flushAllPendingSaves();
}

void FunctionCommandPage::discardPendingFlowDraft()
{
    if (m_canvasEditor) {
        m_canvasEditor->discardPendingSaves();
    }
}

bool FunctionCommandPage::applyFunctionFlowRuntimeEvent(
    const FunctionFlowNodeExecutionEvent &event)
{
    return m_canvasEditor
        && m_functionId == event.functionId
        && m_canvasEditor->applyRuntimeEvent(event);
}

bool FunctionCommandPage::applyFunctionFlowRunEvent(
    const FunctionFlowRunExecutionEvent &event)
{
    return m_canvasEditor
        && m_functionId == event.functionId
        && m_canvasEditor->applyRunEvent(event);
}

FunctionCanvasEditor *FunctionCommandPage::canvasEditor() const
{
    return m_canvasEditor;
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
    header->setObjectName(QStringLiteral("commandMethodHeader"));
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
                                                    const QList<QWidget *> &rows,
                                                    const QStringList &rowIds,
                                                    const std::function<void(
                                                        const QStringList &
                                                    )> &onOrderChanged)
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
    if (rowIds.size() == rows.size() && !rowIds.isEmpty())
    {
        auto *column = new ReorderableCardColumn;
        column->setOrderChangedCallback(onOrderChanged);
        for (int index = 0; index < rows.size(); ++index)
        {
            QWidget *row = rows.at(index);
            QWidget *dragSurface =
                row->findChild<QWidget *>(
                    QStringLiteral("commandMethodHeader")
                );
            column->addCard(rowIds.at(index), row, dragSurface);
        }
        layout->addWidget(column);
    }
    else
    {
        for (QWidget *row : rows)
        {
            layout->addWidget(row);
        }
    }
    return section;
}

FunctionCanvasEditor *FunctionCommandPage::ensureCanvasEditor()
{
    if (m_canvasEditor) {
        if (m_canvasEditor->functionId() != m_functionId) {
            if (!m_canvasEditor->setFunctionId(
                m_functionId,
                flowPlacementDefaults(m_functionId)
            )) {
                return nullptr;
            }
        }
        return m_canvasEditor;
    }

    FunctionCanvasEditorAccess access;
    access.flows = m_access.flows;
    for (const ModelOption &option : modelOptions()) {
        access.inspectorOptions.models.append(
            qMakePair(option.id, option.title)
        );
    }
    const PromptRuntimeSnapshot snapshot =
        m_access.prompts.snapshotProvider
            ? m_access.prompts.snapshotProvider()
            : PromptRuntimeSnapshot();
    for (const PromptTargetInfo &target :
         promptRuntimeTargets(snapshot)) {
        access.inspectorOptions.prompts.append(
            qMakePair(target.id, target.title)
        );
    }
    for (const QString &id : supportedSpeechProviderIds()) {
        access.inspectorOptions.speechProviders.append(
            qMakePair(id, speechProviderTitle(id))
        );
    }
    for (const QString &id : supportedOcrEngineIds()) {
        access.inspectorOptions.ocrEngines.append(
            qMakePair(id, ocrEngineTitleForCanvas(id))
        );
    }
    access.showWarning = [this](
        const QString &title,
        const QString &message
    ) {
        if (m_access.operationFailed) {
            OperationError error;
            error.code = QStringLiteral("flow_canvas_operation_failed");
            error.message = title;
            error.detail = message;
            m_access.operationFailed(error);
            return;
        }
        showAttentionWarning(this, title, message);
    };
    access.executionModeProvider = [this](
        const QString &functionId
    ) {
        if (!m_access.settings) {
            return FunctionExecutionMode::Classic;
        }
        const AppSettingsData data = m_access.settings->toData();
        const int index = data.functionIndex(functionId);
        return index >= 0
            ? data.functions.at(index).executionMode
            : FunctionExecutionMode::Classic;
    };
    access.showInformation = [this](
        const QString &title,
        const QString &message
    ) {
        showAttentionInformation(this, title, message);
    };
    m_canvasEditor = new FunctionCanvasEditor(access, this);
    if (!m_canvasEditor->setFunctionId(
        m_functionId,
        flowPlacementDefaults(m_functionId)
    )) {
        return nullptr;
    }
    return m_canvasEditor;
}

bool FunctionCommandPage::setCanvasMode(bool enabled)
{
    if (enabled == m_canvasMode) {
        return true;
    }
    if (!enabled && m_canvasEditor
        && !m_canvasEditor->flushAllPendingSaves()) {
        reportFlowFailure(m_canvasEditor->controller()->lastError());
        return false;
    }
    if (enabled && !ensureCanvasEditor()) {
        return false;
    }
    m_canvasMode = enabled;
    refresh();
    return true;
}

bool FunctionCommandPage::changeExecutionMode(
    FunctionExecutionMode mode)
{
    if (m_access.settings) {
        const AppSettingsData latest =
            m_access.settings->toData();
        const int index =
            latest.functionIndex(m_functionId);
        if (index >= 0
            && latest.functions.at(index).executionMode == mode) {
            return true;
        }
    }

    OperationError error;
    const bool changed =
        m_access.flows.setExecutionMode
        && m_access.flows.setExecutionMode(
            m_functionId,
            mode,
            &error
        );
    if (!changed) {
        if (error.isEmpty()) {
            error.code =
                QStringLiteral("flow_mode_change_failed");
            error.message =
                text8("执行模式切换失败，请重试。");
        }
        reportFlowFailure(error);
        refresh();
        return false;
    }
    m_access.settings->reloadFunctionFlowState(m_functionId);
    refresh();
    return true;
}

void FunctionCommandPage::reportFlowFailure(
    const OperationError &error)
{
    if (m_access.operationFailed) {
        m_access.operationFailed(error);
        return;
    }
    QString message = error.message.trimmed();
    if (message.isEmpty()) {
        message = error.code.trimmed().isEmpty()
            ? text8("流程未能保存，请重试。")
            : error.code;
    }
    showAttentionWarning(
        this,
        text8("流程保存失败"),
        message
    );
}

FunctionFlowPlacementDefaults
FunctionCommandPage::flowPlacementDefaults(
    const QString &functionId) const
{
    FunctionFlowPlacementDefaults defaults;
    if (!m_access.settings) {
        return defaults;
    }
    const AppSettingsData data = m_access.settings->toData();
    const int index = data.functionIndex(functionId);
    if (index >= 0) {
        defaults.function = data.functions.at(index);
    } else {
        defaults.function.id = functionId;
        defaults.function.modelId =
            m_access.settings->modelFor(functionId);
        defaults.function.promptId =
            m_access.settings->promptIdFor(functionId);
    }
    defaults.speechProviderId =
        m_access.settings->speechProvider();
    defaults.ocrEngineId = m_access.settings->ocrEngine();
    defaults.resultPopupOpacity =
        data.resultPopupOpacity;
    return defaults;
}

void FunctionCommandPage::refreshCanvasState()
{
    if (!m_access.settings || !m_canvasEditor
        || m_canvasEditor->functionId() != m_functionId) {
        return;
    }
    const AppSettingsData data = m_access.settings->toData();
    const int functionIndex = data.functionIndex(m_functionId);
    if (functionIndex < 0) {
        return;
    }
    m_canvasEditor->observeRemoteState(
        data.functions.at(functionIndex).flow
    );
}

void FunctionCommandPage::refresh()
{
    if (!m_access.settings || !m_settingsLayout || !m_canvasLayout)
    {
        return;
    }
    if (m_canvasEditor) {
        m_settingsLayout->removeWidget(m_canvasEditor);
        m_canvasLayout->removeWidget(m_canvasEditor);
        m_canvasEditor->setParent(this);
        m_canvasEditor->hide();
    }
    clearLayout(m_settingsLayout);
    clearLayout(m_canvasLayout);
    m_contentLayout = m_canvasMode
        ? m_canvasLayout
        : m_settingsLayout;
    if (m_canvasMode) {
        m_pageStack->setCurrentWidget(m_canvasHost);
    } else {
        m_pageStack->setCurrentWidget(m_scroll);
    }
    if (m_functionId.trimmed().isEmpty()) {
        return;
    }
    const QString id = m_functionId;
    const QString title = functionTitle(id);
    bool custom = false;
    const CustomFunctionDef customFunctionData = customFunction(id, &custom);
    const QString shortcut = custom ? customFunctionData.shortcut : m_access.settings->hotkey(id);
    const AppSettingsData currentData =
        m_access.settings->toData();
    const int currentFunctionIndex =
        currentData.functionIndex(id);
    const FunctionExecutionMode executionMode =
        currentFunctionIndex >= 0
            ? currentData.functions.at(currentFunctionIndex)
                  .executionMode
            : FunctionExecutionMode::Classic;

    auto *header = new QWidget;
    auto *headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(6);
    auto *titleLayout = new QHBoxLayout;
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(12);
    auto *actionLayout = new QHBoxLayout;
    actionLayout->setContentsMargins(0, 0, 0, 0);
    actionLayout->setSpacing(12);
    const int titleWrapWidth = 480;
    const int shortcutWrapWidth = 140;
    auto *name = new QLabel(title);
    name->setObjectName(
        QStringLiteral("functionCommandTitleLabel")
    );
    name->setFont(appFont(22, QFont::DemiBold));
    name->setWordWrap(true);
    name->setMinimumWidth(titleWrapWidth);
    name->setFixedHeight(
        name->fontMetrics().boundingRect(
            QRect(0, 0, titleWrapWidth, 10000),
            Qt::TextWordWrap,
            title
        ).height()
    );
    QSizePolicy nameSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Fixed
    );
    nameSizePolicy.setHeightForWidth(false);
    name->setSizePolicy(nameSizePolicy);
    auto *shortcutBadge = new QLabel(displayShortcut(shortcut));
    shortcutBadge->setObjectName(
        QStringLiteral("functionCommandShortcutLabel")
    );
    shortcutBadge->setAlignment(Qt::AlignCenter);
    shortcutBadge->setFont(appFont(10, QFont::DemiBold));
    shortcutBadge->setWordWrap(true);
    shortcutBadge->setMinimumWidth(shortcutWrapWidth);
    shortcutBadge->setMaximumWidth(240);
    shortcutBadge->setFixedHeight(
        shortcutBadge->fontMetrics().boundingRect(
            QRect(0, 0, shortcutWrapWidth, 10000),
            Qt::TextWordWrap,
            shortcutBadge->text()
        ).height() + 10
    );
    QSizePolicy shortcutSizePolicy(
        QSizePolicy::Preferred,
        QSizePolicy::Fixed
    );
    shortcutSizePolicy.setHeightForWidth(false);
    shortcutBadge->setSizePolicy(shortcutSizePolicy);
    shortcutBadge->setStyleSheet(QStringLiteral(
        "QLabel#functionCommandShortcutLabel {"
        " color:#174793; background:#edf3ff;"
        " border:1px solid #b8cbee; border-radius:4px;"
        " padding:3px 7px; font-weight:600; }"
    ));
    titleLayout->addWidget(name);
    titleLayout->addWidget(shortcutBadge);
    headerLayout->addLayout(titleLayout);

    auto *modeSelector = new QWidget(header);
    modeSelector->setObjectName(
        QStringLiteral("functionExecutionModeSelector")
    );
    auto *modeLayout = new QHBoxLayout(modeSelector);
    modeLayout->setContentsMargins(8, 2, 8, 2);
    modeLayout->setSpacing(4);
    auto *modeLabel = new QLabel(text8("当前执行"), modeSelector);
    modeLabel->setMinimumWidth(64);
    modeLabel->setAlignment(Qt::AlignCenter);
    auto *classicMode = new QPushButton(
        text8("普通模式"),
        modeSelector
    );
    classicMode->setObjectName(
        QStringLiteral("functionClassicModeButton")
    );
    auto *canvasMode = new QPushButton(
        text8("画布模式"),
        modeSelector
    );
    canvasMode->setObjectName(
        QStringLiteral("functionCanvasModeButton")
    );
    const QList<QPushButton *> modeButtons =
        QList<QPushButton *>() << classicMode << canvasMode;
    for (QPushButton *button : modeButtons) {
        button->setCheckable(true);
        button->setMinimumSize(96, 34);
        button->setCursor(Qt::PointingHandCursor);
    }
    auto *modeGroup = new QButtonGroup(modeSelector);
    modeGroup->setExclusive(true);
    modeGroup->addButton(classicMode);
    modeGroup->addButton(canvasMode);
    classicMode->setChecked(
        executionMode == FunctionExecutionMode::Classic
    );
    canvasMode->setChecked(
        executionMode == FunctionExecutionMode::Canvas
    );
    modeSelector->setStyleSheet(QStringLiteral(
        "QWidget#functionExecutionModeSelector {"
        " background:#e2e8f0; border-radius:8px; }"
        "QPushButton { background:transparent; color:#475569;"
        " border:1px solid transparent; border-radius:6px;"
        " padding:4px 10px; }"
        "QPushButton:checked { background:#2563eb; color:#ffffff;"
        " border-color:#2563eb; }"
    ));
    connect(
        classicMode,
        &QPushButton::clicked,
        header,
        [this]() {
            changeExecutionMode(FunctionExecutionMode::Classic);
        }
    );
    connect(
        canvasMode,
        &QPushButton::clicked,
        header,
        [this]() {
            changeExecutionMode(FunctionExecutionMode::Canvas);
        }
    );
    modeLayout->addWidget(modeLabel);
    modeLayout->addWidget(classicMode);
    modeLayout->addWidget(canvasMode);
    actionLayout->addStretch();
    actionLayout->addWidget(modeSelector);

    auto *canvas = new QPushButton(
        m_canvasMode ? text8("返回设置") : text8("编辑画布")
    );
    canvas->setObjectName(QStringLiteral("functionCanvasButton"));
    canvas->setCheckable(true);
    canvas->setChecked(m_canvasMode);
    canvas->setMinimumSize(100, 38);
    canvas->setCursor(Qt::PointingHandCursor);
    canvas->setToolTip(m_canvasMode ? text8("返回当前功能设置")
                                    : text8("打开当前功能的流程画布"));
    canvas->setStyleSheet(
        buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827"))
        + QStringLiteral(
            "QPushButton#functionCanvasButton:checked {"
            " background: #2563eb; color: #ffffff; border-color: #2563eb;"
            "}"
        )
    );
    connect(canvas, &QPushButton::toggled, header,
            [this, canvas](bool enabled)
            {
                if (!setCanvasMode(enabled)) {
                    QSignalBlocker blocker(canvas);
                    canvas->setChecked(m_canvasMode);
                }
    });
    actionLayout->addWidget(canvas);
    headerLayout->addLayout(actionLayout);
    const int titleRowHeight =
        qMax(name->maximumHeight(), shortcutBadge->maximumHeight());
    const int actionRowHeight =
        qMax(modeSelector->sizeHint().height(), canvas->minimumHeight());
    header->setSizePolicy(
        QSizePolicy::Preferred,
        QSizePolicy::Fixed
    );
    header->setFixedHeight(
        headerLayout->contentsMargins().top()
        + titleRowHeight
        + headerLayout->spacing()
        + actionRowHeight
        + headerLayout->contentsMargins().bottom()
    );
    m_contentLayout->addWidget(header);

    if (m_canvasMode)
    {
        FunctionCanvasEditor *editor = ensureCanvasEditor();
        if (!editor) {
            m_canvasMode = false;
            refresh();
            return;
        }
        const AppSettingsData data = m_access.settings->toData();
        const int functionIndex = data.functionIndex(id);
        if (functionIndex >= 0) {
            editor->observeRemoteState(
                data.functions.at(functionIndex).flow
            );
        }
        editor->show();
        m_contentLayout->addWidget(editor, 1);
        return;
    }

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

    QHash<QString, QWidget *> inputCards;
    inputCards.insert(
        functionInputVoiceId(),
        commandAccordionCard(
        text8("语音输入"), text8("使用麦克风录音，并通过当前语音识别服务转换为文字。"),
        m_access.settings->useVoiceFor(id) ? text8("已启用") : text8("已关闭"),
        m_access.settings->useVoiceFor(id), true, false, voiceBody,
        [this, id, ensureInput](bool enabled)
        {
            if (!ensureInput(QStringLiteral("voice"), enabled))
                return;
            m_access.settings->setUseVoiceFor(id, enabled);
            saveSettings();
        })
    );
    inputCards.insert(
        functionInputSelectionId(),
        commandAccordionCard(
        text8("读取选中文字"), text8("读取鼠标拖动选中的文字，作为原文、上下文或处理对象。"),
        m_access.settings->useSelectionFor(id) ? text8("已启用") : text8("已关闭"),
        m_access.settings->useSelectionFor(id), true, false, selectionBody,
        [this, id, ensureInput](bool enabled)
        {
            if (!ensureInput(QStringLiteral("selection"), enabled))
                return;
            m_access.settings->setUseSelectionFor(id, enabled);
            saveSettings();
        })
    );
    inputCards.insert(
        functionInputScreenshotId(),
        commandAccordionCard(
        text8("截图识别"), text8("框选屏幕区域，通过本地文字识别或所选图片接口读取内容。"),
        m_access.settings->useScreenshotFor(id) ? text8("已启用") : text8("已关闭"),
        m_access.settings->useScreenshotFor(id), true, false, screenshotBody,
        [this, id, ensureInput](bool enabled)
        {
            if (!ensureInput(QStringLiteral("screenshot"), enabled))
                return;
            m_access.settings->setUseScreenshotFor(id, enabled);
            saveSettings();
        })
    );
    const QStringList inputOrder =
        m_access.settings->inputOrderFor(id);
    QList<QWidget *> inputRows;
    for (const QString &inputId : inputOrder)
    {
        inputRows.append(inputCards.value(inputId));
    }
    m_contentLayout->addWidget(
        commandControlSection(
            text8("输入控制"),
            text8("可以同时启用多种输入方式"),
            inputRows,
            inputOrder,
            [this, id](const QStringList &order)
            {
                m_access.settings->setInputOrderFor(id, order);
                saveSettings();
            }
        )
    );

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

    if (custom) {
        auto *styleCard = new QFrame;
        styleCard->setObjectName(
            QStringLiteral("functionFloatingBarStyleCard")
        );
        styleCard->setStyleSheet(commandCenterSectionStyle());
        auto *styleLayout = new QVBoxLayout(styleCard);
        styleLayout->setContentsMargins(18, 14, 18, 16);
        styleLayout->setSpacing(10);
        auto *styleTitle = new QLabel(text8("漂浮窗样式"));
        styleTitle->setFont(appFont(11, QFont::DemiBold));
        auto *styleHint = new QLabel(
            text8("仅覆盖当前自定义功能；跟随全局时使用语音录音设置中的样式。")
        );
        styleHint->setWordWrap(true);
        styleHint->setObjectName(QStringLiteral("commandMuted"));
        FloatingBarStyleSelector::Options options;
        options.allowInherit = true;
        auto *styleSelector = new FloatingBarStyleSelector(
            options,
            styleCard
        );
        styleSelector->setObjectName(
            QStringLiteral("functionFloatingBarStyleSelector")
        );
        styleSelector->setCurrentStyle(
            m_access.settings->floatingBarStyleOverrideFor(id)
        );
        styleSelector->setStyleChangedCallback(
            [this, id](const QString &style) {
                m_access.settings->setFloatingBarStyleOverrideFor(
                    id,
                    style
                );
                saveSettings();
            }
        );
        styleLayout->addWidget(styleTitle);
        styleLayout->addWidget(styleHint);
        styleLayout->addWidget(styleSelector);
        m_contentLayout->addWidget(styleCard);
    }

    QHash<QString, QWidget *> outputCards;
    outputCards.insert(
        functionOutputAiId(),
        commandAccordionCard(text8("AI 处理"),
                             text8("选择当前功能使用的大模型和提示词。"),
                             modelTitle(m_access.settings->modelFor(id)), true, false,
                             false, aiBody, std::function<void(bool)>())
    );
    outputCards.insert(
        functionOutputAutoWriteId(),
        commandAccordionCard(
        text8("自动写入"), text8("处理完成后，直接把结果写入当前输入位置。"),
        currentOutput == outputModeAutoWrite() ? text8("当前默认") : text8("可选择"),
        currentOutput == outputModeAutoWrite(), true, false,
        outputBody(outputModeAutoWrite()),
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
        })
    );
    outputCards.insert(
        functionOutputPopupId(),
        commandAccordionCard(
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
        })
    );
    outputCards.insert(
        functionOutputScreenshotPanelId(),
        commandAccordionCard(
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
        })
    );
    const QStringList outputOrder =
        m_access.settings->outputOrderFor(id);
    QList<QWidget *> outputRows;
    for (const QString &outputId : outputOrder)
    {
        outputRows.append(outputCards.value(outputId));
    }
    m_contentLayout->addWidget(
        commandControlSection(
            text8("输出控制"),
            text8("选择默认展现方式"),
            outputRows,
            outputOrder,
            [this, id](const QStringList &order)
            {
                m_access.settings->setOutputOrderFor(id, order);
                saveSettings();
            }
        )
    );
    m_contentLayout->addStretch();
    if (m_scroll)
    {
        m_scroll->verticalScrollBar()->setValue(0);
        m_scroll->horizontalScrollBar()->setValue(0);
    }
}
