#include "function_canvas_inspector.h"

#include "../providers/model_catalog.h"
#include "function_canvas_node_item.h"
#include "ui_style.h"

#include <QAbstractItemModel>
#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>

namespace {

QVector<QPair<QString, QString>> choices(
    const char *firstId,
    const char *firstTitle,
    const char *secondId,
    const char *secondTitle,
    const char *thirdId = nullptr,
    const char *thirdTitle = nullptr,
    const char *fourthId = nullptr,
    const char *fourthTitle = nullptr)
{
    QVector<QPair<QString, QString>> result;
    result.append(qMakePair(
        QString::fromLatin1(firstId),
        QString::fromUtf8(firstTitle)
    ));
    result.append(qMakePair(
        QString::fromLatin1(secondId),
        QString::fromUtf8(secondTitle)
    ));
    if (thirdId && thirdTitle) {
        result.append(qMakePair(
            QString::fromLatin1(thirdId),
            QString::fromUtf8(thirdTitle)
        ));
    }
    if (fourthId && fourthTitle) {
        result.append(qMakePair(
            QString::fromLatin1(fourthId),
            QString::fromUtf8(fourthTitle)
        ));
    }
    return result;
}

QSpinBox *integerBox(
    int minimum,
    int maximum,
    int value,
    const QString &objectName,
    QWidget *parent)
{
    QSpinBox *box = new QSpinBox(parent);
    box->setObjectName(objectName);
    box->setRange(minimum, maximum);
    box->setValue(qBound(minimum, value, maximum));
    box->setMinimumHeight(34);
    return box;
}

QComboBox *opacityBox(
    int value,
    const QString &objectName,
    QWidget *parent)
{
    QComboBox *box = new QComboBox(parent);
    box->setObjectName(objectName);
    box->setMinimumHeight(34);
    box->addItem(QString::fromUtf8("继承全局"), -1);
    for (int opacity = 20; opacity <= 100; ++opacity) {
        box->addItem(
            QStringLiteral("%1%").arg(opacity),
            opacity
        );
    }
    const int selected = box->findData(value);
    box->setCurrentIndex(selected >= 0 ? selected : 0);
    return box;
}

QString nodeTitle(
    const FunctionFlowGraph &graph,
    const QString &nodeId)
{
    for (const FunctionFlowNode &node : graph.nodes) {
        if (node.id == nodeId) {
            return node.title.trimmed().isEmpty()
                ? functionCanvasNodeTypeDisplayName(node.type)
                : node.title;
        }
    }
    return nodeId;
}

QString popupActionDisplayName(const QString &id)
{
    if (id == QStringLiteral("regenerate")) {
        return QString::fromUtf8("重新生成");
    }
    if (id == QStringLiteral("retryModel")) {
        return QString::fromUtf8("重试模型");
    }
    if (id == QStringLiteral("followUp")) {
        return QString::fromUtf8("继续追问");
    }
    if (id == QStringLiteral("expand")) {
        return QString::fromUtf8("展开");
    }
    if (id == QStringLiteral("vocabulary")) {
        return QString::fromUtf8("加入词库");
    }
    if (id == QStringLiteral("copy")) {
        return QString::fromUtf8("复制");
    }
    if (id == QStringLiteral("write")) {
        return QString::fromUtf8("写入");
    }
    if (id == QStringLiteral("replace")) {
        return QString::fromUtf8("替换");
    }
    return id;
}

} // namespace

FunctionCanvasInspector::FunctionCanvasInspector(
    const FunctionCanvasInspectorOptions &options,
    QWidget *parent)
    : QWidget(parent),
      m_options(options)
{
    setObjectName(QStringLiteral("functionCanvasInspector"));
    setFont(appFont());
    setMinimumWidth(280);
    setMaximumWidth(360);
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(14, 14, 14, 14);
    m_layout->setSpacing(10);
    rebuild();
}

void FunctionCanvasInspector::setGraphAndSelection(
    const FunctionFlowGraph &graph,
    const QString &nodeId)
{
    m_graph = graph;
    m_selectedNodeId = nodeId.trimmed();
    bool found = false;
    for (const FunctionFlowNode &node : m_graph.nodes) {
        if (node.id == m_selectedNodeId) {
            m_node = node;
            found = true;
            break;
        }
    }
    if (!found) {
        m_selectedNodeId.clear();
        m_node = FunctionFlowNode();
    }
    rebuild();
}

void FunctionCanvasInspector::clearSelection()
{
    m_selectedNodeId.clear();
    m_node = FunctionFlowNode();
    rebuild();
}

QString FunctionCanvasInspector::selectedNodeId() const
{
    return m_selectedNodeId;
}

void FunctionCanvasInspector::setEditable(bool editable)
{
    m_editable = editable;
    setEnabled(editable);
}

void FunctionCanvasInspector::rebuild()
{
    clearLayout(m_layout);
    auto *heading = new QLabel(
        m_selectedNodeId.isEmpty()
            ? QString::fromUtf8("节点设置")
            : QString::fromUtf8("节点设置 · ")
                + (m_node.title.trimmed().isEmpty()
                    ? functionCanvasNodeTypeDisplayName(m_node.type)
                    : m_node.title)
    );
    heading->setFont(appFont(14, QFont::DemiBold));
    heading->setWordWrap(true);
    m_layout->addWidget(heading);
    if (m_selectedNodeId.isEmpty()) {
        auto *empty = new QLabel(
            QString::fromUtf8("选择画布中的节点后，在这里修改设置。")
        );
        empty->setWordWrap(true);
        m_layout->addWidget(empty);
        m_layout->addStretch();
        setEnabled(m_editable);
        return;
    }

    addSection(
        QString::fromUtf8("基础设置"),
        QStringLiteral("flowInspectorSectionBasic")
    );

    auto *name = new QLineEdit(m_node.title);
    name->setObjectName(QStringLiteral("flowNodeNameEdit"));
    name->setMaxLength(80);
    connect(
        name,
        &QLineEdit::editingFinished,
        this,
        [this, name]() {
            const QString value = name->text().trimmed();
            if (m_node.title == value) {
                return;
            }
            m_node.title = value;
            emitNodeChange();
        }
    );
    m_layout->addWidget(field(QString::fromUtf8("节点名称"), name));

    auto *enabled = new QCheckBox(
        QString::fromUtf8("启用此节点")
    );
    enabled->setObjectName(QStringLiteral("flowNodeEnabled"));
    enabled->setChecked(m_node.enabled);
    connect(
        enabled,
        &QCheckBox::toggled,
        this,
        [this](bool checked) {
            m_node.enabled = checked;
            emitNodeChange();
        }
    );
    m_layout->addWidget(enabled);

    switch (m_node.type) {
    case FunctionFlowNodeType::VoiceSource:
        addSection(
            QString::fromUtf8("输入与来源"),
            QStringLiteral("flowInspectorSectionInputSource")
        );
        addVoiceFields();
        break;
    case FunctionFlowNodeType::SelectionSource:
        addSection(
            QString::fromUtf8("输入与来源"),
            QStringLiteral("flowInspectorSectionInputSource")
        );
        addSelectionFields();
        break;
    case FunctionFlowNodeType::ScreenshotSource:
        addSection(
            QString::fromUtf8("输入与来源"),
            QStringLiteral("flowInspectorSectionInputSource")
        );
        addScreenshotFields();
        break;
    case FunctionFlowNodeType::Input:
        addSection(
            QString::fromUtf8("输入与来源"),
            QStringLiteral("flowInspectorSectionInputSource")
        );
        addInputFields();
        break;
    case FunctionFlowNodeType::Model:
        addSection(
            QString::fromUtf8("处理"),
            QStringLiteral("flowInspectorSectionProcessing")
        );
        addModelFields();
        break;
    case FunctionFlowNodeType::Output:
        addSection(
            QString::fromUtf8("显示与输出"),
            QStringLiteral("flowInspectorSectionDisplayOutput")
        );
        addOutputFields();
        break;
    case FunctionFlowNodeType::ResultPopup:
        addSection(
            QString::fromUtf8("显示与输出"),
            QStringLiteral("flowInspectorSectionDisplayOutput")
        );
        addPopupFields();
        break;
    case FunctionFlowNodeType::ScreenshotPanel:
        addSection(
            QString::fromUtf8("显示与输出"),
            QStringLiteral("flowInspectorSectionDisplayOutput")
        );
        addScreenshotPanelFields();
        break;
    case FunctionFlowNodeType::AutoWrite:
        addSection(
            QString::fromUtf8("显示与输出"),
            QStringLiteral("flowInspectorSectionDisplayOutput")
        );
        addAutoWriteFields();
        break;
    }
    m_layout->addStretch();
    setEnabled(m_editable);
}

void FunctionCanvasInspector::clearLayout(QLayout *layout)
{
    if (!layout) {
        return;
    }
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (item->widget()) {
            delete item->widget();
        }
        if (item->layout()) {
            clearLayout(item->layout());
            delete item->layout();
        }
        delete item;
    }
}

void FunctionCanvasInspector::addSection(
    const QString &title,
    const QString &objectName)
{
    auto *heading = new QLabel(title, this);
    heading->setObjectName(objectName);
    heading->setFont(appFont(11, QFont::DemiBold));
    heading->setWordWrap(true);
    m_layout->addWidget(heading);

    auto *divider = new QFrame(this);
    divider->setObjectName(objectName + QStringLiteral("Divider"));
    divider->setFrameShape(QFrame::HLine);
    divider->setFrameShadow(QFrame::Plain);
    m_layout->addWidget(divider);
}

QWidget *FunctionCanvasInspector::field(
    const QString &label,
    QWidget *control)
{
    QWidget *container = new QWidget(this);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    auto *caption = new QLabel(label);
    caption->setWordWrap(true);
    layout->addWidget(caption);
    layout->addWidget(control);
    return container;
}

QComboBox *FunctionCanvasInspector::combo(
    const QVector<QPair<QString, QString>> &options,
    const QString &current,
    const QString &objectName)
{
    QComboBox *box = new QComboBox(this);
    box->setObjectName(objectName);
    for (const QPair<QString, QString> &option : options) {
        box->addItem(option.second, option.first);
    }
    if (!current.isEmpty() && box->findData(current) < 0) {
        box->addItem(current, current);
    }
    const int index = box->findData(current);
    if (index >= 0) {
        box->setCurrentIndex(index);
    }
    box->setMinimumHeight(34);
    return box;
}

void FunctionCanvasInspector::addVoiceFields()
{
    auto *device = new QLineEdit(QString::fromUtf8("系统默认"));
    device->setObjectName(QStringLiteral("flowVoiceDevice"));
    device->setReadOnly(true);
    m_layout->addWidget(field(
        QString::fromUtf8("麦克风设备"),
        device
    ));

    QComboBox *provider = combo(
        m_options.speechProviders,
        m_node.config.voice.speechProviderId,
        QStringLiteral("flowVoiceProvider")
    );
    connect(
        provider,
        static_cast<void (QComboBox::*)(int)>(
            &QComboBox::currentIndexChanged
        ),
        this,
        [this, provider](int) {
            m_node.config.voice.speechProviderId =
                provider->currentData().toString();
            emitNodeChange();
        }
    );
    m_layout->addWidget(field(
        QString::fromUtf8("语音服务"),
        provider
    ));

    QComboBox *trigger = combo(
        choices("toggle", "按一次开始/再按结束",
                "hold", "按住录音"),
        m_node.config.voice.recording.triggerMode,
        QStringLiteral("flowVoiceTriggerMode")
    );
    connect(
        trigger,
        static_cast<void (QComboBox::*)(int)>(
            &QComboBox::currentIndexChanged
        ),
        this,
        [this, trigger](int) {
            m_node.config.voice.recording.triggerMode =
                trigger->currentData().toString();
            emitNodeChange();
        }
    );
    m_layout->addWidget(field(
        QString::fromUtf8("录音方式"),
        trigger
    ));

    QSpinBox *sequence = integerBox(
        0, 10000,
        m_node.config.voice.acquisitionSequence,
        QStringLiteral("flowVoiceSequence"),
        this
    );
    connect(
        sequence,
        static_cast<void (QSpinBox::*)(int)>(
            &QSpinBox::valueChanged
        ),
        this,
        [this](int value) {
            m_node.config.voice.acquisitionSequence = value;
            emitNodeChange();
        }
    );
    m_layout->addWidget(field(
        QString::fromUtf8("采集顺序"),
        sequence
    ));

    QSpinBox *countdown = integerBox(
        0, 60,
        m_node.config.voice.recording.countdownSeconds,
        QStringLiteral("flowVoiceCountdown"),
        this
    );
    countdown->setSuffix(QString::fromUtf8(" 秒"));
    connect(
        countdown,
        static_cast<void (QSpinBox::*)(int)>(
            &QSpinBox::valueChanged
        ),
        this,
        [this](int value) {
            m_node.config.voice.recording.countdownSeconds = value;
            emitNodeChange();
        }
    );
    m_layout->addWidget(field(
        QString::fromUtf8("倒计时"),
        countdown
    ));

    auto *beep = new QCheckBox(QString::fromUtf8("录音提示音"));
    beep->setObjectName(QStringLiteral("flowVoiceBeep"));
    beep->setChecked(m_node.config.voice.recording.beepEnabled);
    connect(
        beep,
        &QCheckBox::toggled,
        this,
        [this](bool checked) {
            m_node.config.voice.recording.beepEnabled = checked;
            emitNodeChange();
        }
    );
    m_layout->addWidget(beep);

    auto *beepPath = new QLineEdit(
        m_node.config.voice.recording.beepPath
    );
    beepPath->setObjectName(QStringLiteral("flowVoiceBeepPath"));
    connect(
        beepPath,
        &QLineEdit::editingFinished,
        this,
        [this, beepPath]() {
            m_node.config.voice.recording.beepPath =
                beepPath->text().trimmed();
            emitNodeChange();
        }
    );
    m_layout->addWidget(field(
        QString::fromUtf8("提示音文件"),
        beepPath
    ));

    auto *longRecording = new QCheckBox(
        QString::fromUtf8("启用长录音")
    );
    longRecording->setObjectName(
        QStringLiteral("flowVoiceLongRecording")
    );
    longRecording->setChecked(
        m_node.config.voice.recording.longRecordingEnabled
    );
    connect(
        longRecording,
        &QCheckBox::toggled,
        this,
        [this](bool checked) {
            m_node.config.voice.recording.longRecordingEnabled =
                checked;
            emitNodeChange();
        }
    );
    m_layout->addWidget(longRecording);

    QSpinBox *segment = integerBox(
        20, 55,
        m_node.config.voice.recording.segmentSeconds,
        QStringLiteral("flowVoiceSegmentSeconds"),
        this
    );
    segment->setSuffix(QString::fromUtf8(" 秒"));
    connect(
        segment,
        static_cast<void (QSpinBox::*)(int)>(
            &QSpinBox::valueChanged
        ),
        this,
        [this](int value) {
            m_node.config.voice.recording.segmentSeconds = value;
            emitNodeChange();
        }
    );
    m_layout->addWidget(field(
        QString::fromUtf8("分段时长"),
        segment
    ));

    QSpinBox *maximum = integerBox(
        1, 30,
        m_node.config.voice.recording.maximumMinutes,
        QStringLiteral("flowVoiceMaximumMinutes"),
        this
    );
    maximum->setSuffix(QString::fromUtf8(" 分钟"));
    connect(
        maximum,
        static_cast<void (QSpinBox::*)(int)>(
            &QSpinBox::valueChanged
        ),
        this,
        [this](int value) {
            m_node.config.voice.recording.maximumMinutes = value;
            emitNodeChange();
        }
    );
    m_layout->addWidget(field(
        QString::fromUtf8("最长录音"),
        maximum
    ));

    QComboBox *network = combo(
        choices("inherit", "继承全局",
                "direct", "直连",
                "systemProxy", "系统代理"),
        m_node.config.voice.networkPolicy,
        QStringLiteral("flowVoiceNetwork")
    );
    connect(
        network,
        static_cast<void (QComboBox::*)(int)>(
            &QComboBox::currentIndexChanged
        ),
        this,
        [this, network](int) {
            m_node.config.voice.networkPolicy =
                network->currentData().toString();
            emitNodeChange();
        }
    );
    m_layout->addWidget(field(
        QString::fromUtf8("网络策略"),
        network
    ));
}

void FunctionCanvasInspector::addSelectionFields()
{
    QComboBox *mode = combo(
        choices("plain", "普通读取",
                "inheritStrong", "继承全局强力选中"),
        m_node.config.selection.inheritStrongSelection
            ? QStringLiteral("inheritStrong")
            : QStringLiteral("plain"),
        QStringLiteral("flowSelectionMode")
    );
    connect(
        mode,
        static_cast<void (QComboBox::*)(int)>(
            &QComboBox::currentIndexChanged
        ),
        this,
        [this, mode](int) {
            m_node.config.selection.inheritStrongSelection =
                mode->currentData().toString()
                == QStringLiteral("inheritStrong");
            emitNodeChange();
        }
    );
    m_layout->addWidget(field(
        QString::fromUtf8("读取方式"),
        mode
    ));
    QSpinBox *sequence = integerBox(
        0, 10000,
        m_node.config.selection.acquisitionSequence,
        QStringLiteral("flowSelectionSequence"),
        this
    );
    connect(
        sequence,
        static_cast<void (QSpinBox::*)(int)>(
            &QSpinBox::valueChanged
        ),
        this,
        [this](int value) {
            m_node.config.selection.acquisitionSequence = value;
            emitNodeChange();
        }
    );
    m_layout->addWidget(field(
        QString::fromUtf8("采集顺序"),
        sequence
    ));
}

void FunctionCanvasInspector::addScreenshotFields()
{
    QComboBox *engine = combo(
        m_options.ocrEngines,
        m_node.config.screenshot.ocrEngineId,
        QStringLiteral("flowScreenshotOcrEngine")
    );
    connect(
        engine,
        static_cast<void (QComboBox::*)(int)>(
            &QComboBox::currentIndexChanged
        ),
        this,
        [this, engine](int) {
            m_node.config.screenshot.ocrEngineId =
                engine->currentData().toString();
            emitNodeChange();
        }
    );
    m_layout->addWidget(field(
        QString::fromUtf8("文字识别引擎"),
        engine
    ));
    auto *languages = new QLabel(
        QString::fromUtf8("识别语言：简体中文 / 英文（只读）")
    );
    languages->setObjectName(
        QStringLiteral("flowScreenshotLanguages")
    );
    languages->setWordWrap(true);
    m_layout->addWidget(languages);

    QSpinBox *timeout = integerBox(
        1000, 120000,
        m_node.config.screenshot.timeoutMs,
        QStringLiteral("flowScreenshotTimeout"),
        this
    );
    timeout->setSuffix(QString::fromUtf8(" 毫秒"));
    connect(
        timeout,
        static_cast<void (QSpinBox::*)(int)>(
            &QSpinBox::valueChanged
        ),
        this,
        [this](int value) {
            m_node.config.screenshot.timeoutMs = value;
            emitNodeChange();
        }
    );
    m_layout->addWidget(field(
        QString::fromUtf8("超时"),
        timeout
    ));

    QComboBox *trigger = combo(
        choices("primary", "主快捷键",
                "separate", "独立截图快捷键",
                "launcher", "截图悬浮入口",
                "separateAndLauncher", "独立快捷键和悬浮入口"),
        m_node.config.screenshot.triggerMode,
        QStringLiteral("flowScreenshotTrigger")
    );
    connect(
        trigger,
        static_cast<void (QComboBox::*)(int)>(
            &QComboBox::currentIndexChanged
        ),
        this,
        [this, trigger](int) {
            m_node.config.screenshot.triggerMode =
                trigger->currentData().toString();
            emitNodeChange();
        }
    );
    m_layout->addWidget(field(
        QString::fromUtf8("截图入口"),
        trigger
    ));

    auto *shortcut = new QLineEdit(
        m_node.config.screenshot.separateShortcut
    );
    shortcut->setObjectName(
        QStringLiteral("flowScreenshotShortcut")
    );
    connect(
        shortcut,
        &QLineEdit::editingFinished,
        this,
        [this, shortcut]() {
            m_node.config.screenshot.separateShortcut =
                shortcut->text().trimmed();
            emitNodeChange();
        }
    );
    m_layout->addWidget(field(
        QString::fromUtf8("独立截图快捷键"),
        shortcut
    ));

    QSpinBox *sequence = integerBox(
        0, 10000,
        m_node.config.screenshot.acquisitionSequence,
        QStringLiteral("flowScreenshotSequence"),
        this
    );
    connect(
        sequence,
        static_cast<void (QSpinBox::*)(int)>(
            &QSpinBox::valueChanged
        ),
        this,
        [this](int value) {
            m_node.config.screenshot.acquisitionSequence = value;
            emitNodeChange();
        }
    );
    m_layout->addWidget(field(
        QString::fromUtf8("采集顺序"),
        sequence
    ));

    QComboBox *network = combo(
        choices("inherit", "继承全局",
                "direct", "直连",
                "systemProxy", "系统代理"),
        m_node.config.screenshot.networkPolicy,
        QStringLiteral("flowScreenshotNetwork")
    );
    connect(
        network,
        static_cast<void (QComboBox::*)(int)>(
            &QComboBox::currentIndexChanged
        ),
        this,
        [this, network](int) {
            m_node.config.screenshot.networkPolicy =
                network->currentData().toString();
            emitNodeChange();
        }
    );
    m_layout->addWidget(field(
        QString::fromUtf8("网络策略"),
        network
    ));
}

void FunctionCanvasInspector::addInputFields()
{
    QComboBox *role = combo(
        choices("source", "原文",
                "instruction", "指令",
                "screenshot", "截图文字",
                "system", "系统消息"),
        m_node.config.input.role,
        QStringLiteral("flowInputRole")
    );
    role->setEditable(true);
    role->lineEdit()->setMaxLength(40);
    const auto storeRole = [this, role]() {
        const int index = role->currentIndex();
        const bool selectedKnownItem =
            index >= 0
            && role->currentText() == role->itemText(index);
        const QString value = selectedKnownItem
            ? role->itemData(index).toString()
            : role->currentText().trimmed();
        if (m_node.config.input.role == value) {
            return;
        }
        m_node.config.input.role = value;
        emitNodeChange();
    };
    connect(
        role,
        static_cast<void (QComboBox::*)(int)>(
            &QComboBox::currentIndexChanged
        ),
        this,
        [storeRole](int) {
            storeRole();
        }
    );
    connect(
        role->lineEdit(),
        &QLineEdit::editingFinished,
        this,
        storeRole
    );
    m_layout->addWidget(field(
        QString::fromUtf8("内容角色"),
        role
    ));
    QSpinBox *sequence = integerBox(
        0, 10000,
        m_node.config.input.sequence,
        QStringLiteral("flowInputSequence"),
        this
    );
    connect(
        sequence,
        static_cast<void (QSpinBox::*)(int)>(
            &QSpinBox::valueChanged
        ),
        this,
        [this](int value) {
            m_node.config.input.sequence = value;
            emitNodeChange();
        }
    );
    m_layout->addWidget(field(
        QString::fromUtf8("模型消息顺序"),
        sequence
    ));
    auto *required = new QCheckBox(
        QString::fromUtf8("此输入为必需")
    );
    required->setObjectName(QStringLiteral("flowInputRequired"));
    required->setChecked(m_node.config.input.required);
    connect(
        required,
        &QCheckBox::toggled,
        this,
        [this](bool checked) {
            m_node.config.input.required = checked;
            emitNodeChange();
        }
    );
    m_layout->addWidget(required);
}

void FunctionCanvasInspector::addModelFields()
{
    const QString displayedModelId = normalizeModelId(
        m_node.config.model.modelId,
        m_node.config.model.modelId
    );
    QComboBox *model = combo(
        m_options.models,
        displayedModelId,
        QStringLiteral("flowModelId")
    );
    connect(
        model,
        static_cast<void (QComboBox::*)(int)>(
            &QComboBox::currentIndexChanged
        ),
        this,
        [this, model](int) {
            m_node.config.model.modelId =
                model->currentData().toString();
            emitNodeChange();
        }
    );
    m_layout->addWidget(field(QString::fromUtf8("模型"), model));

    QComboBox *prompt = combo(
        m_options.prompts,
        m_node.config.model.promptId,
        QStringLiteral("flowPromptId")
    );
    connect(
        prompt,
        static_cast<void (QComboBox::*)(int)>(
            &QComboBox::currentIndexChanged
        ),
        this,
        [this, prompt](int) {
            m_node.config.model.promptId =
                prompt->currentData().toString();
            emitNodeChange();
        }
    );
    m_layout->addWidget(field(
        QString::fromUtf8("提示词"),
        prompt
    ));

    auto *stream = new QCheckBox(QString::fromUtf8("流式输出"));
    stream->setObjectName(QStringLiteral("flowModelStream"));
    stream->setChecked(m_node.config.model.stream);
    connect(
        stream,
        &QCheckBox::toggled,
        this,
        [this](bool checked) {
            m_node.config.model.stream = checked;
            emitNodeChange();
        }
    );
    m_layout->addWidget(stream);

    auto *waitMode = new QLineEdit(
        QString::fromUtf8("等待全部输入")
    );
    waitMode->setObjectName(QStringLiteral("flowModelWaitMode"));
    waitMode->setReadOnly(true);
    m_layout->addWidget(field(
        QString::fromUtf8("触发方式"),
        waitMode
    ));

    QComboBox *network = combo(
        choices("inherit", "继承全局",
                "direct", "直连",
                "systemProxy", "系统代理"),
        m_node.config.model.networkPolicy,
        QStringLiteral("flowModelNetwork")
    );
    connect(
        network,
        static_cast<void (QComboBox::*)(int)>(
            &QComboBox::currentIndexChanged
        ),
        this,
        [this, network](int) {
            m_node.config.model.networkPolicy =
                network->currentData().toString();
            emitNodeChange();
        }
    );
    m_layout->addWidget(field(
        QString::fromUtf8("网络策略"),
        network
    ));
}

void FunctionCanvasInspector::addOutputFields()
{
    QComboBox *empty = combo(
        choices("fail", "空结果视为失败",
                "skipActions", "空结果跳过动作"),
        m_node.config.output.emptyResultPolicy,
        QStringLiteral("flowOutputEmptyPolicy")
    );
    connect(
        empty,
        static_cast<void (QComboBox::*)(int)>(
            &QComboBox::currentIndexChanged
        ),
        this,
        [this, empty](int) {
            m_node.config.output.emptyResultPolicy =
                empty->currentData().toString();
            emitNodeChange();
        }
    );
    m_layout->addWidget(field(
        QString::fromUtf8("空结果策略"),
        empty
    ));

    QVector<FunctionFlowEdge> edges;
    for (const FunctionFlowEdge &edge : m_graph.edges) {
        if (edge.fromNodeId == m_node.id
            && edge.fromPortId == QStringLiteral("action_out")) {
            edges.append(edge);
        }
    }
    std::stable_sort(
        edges.begin(),
        edges.end(),
        [](const FunctionFlowEdge &left,
           const FunctionFlowEdge &right) {
            if (left.order != right.order) {
                return left.order < right.order;
            }
            return left.id < right.id;
        }
    );
    auto *list = new QListWidget;
    list->setObjectName(QStringLiteral("flowOutputActionOrder"));
    list->setDragDropMode(QAbstractItemView::InternalMove);
    list->setDefaultDropAction(Qt::MoveAction);
    for (const FunctionFlowEdge &edge : edges) {
        auto *item = new QListWidgetItem(
            nodeTitle(m_graph, edge.toNodeId),
            list
        );
        item->setData(Qt::UserRole, edge.id);
    }
    connect(
        list->model(),
        &QAbstractItemModel::rowsMoved,
        this,
        [this, list]() {
            QStringList ids;
            for (int row = 0; row < list->count(); ++row) {
                ids.append(
                    list->item(row)->data(Qt::UserRole).toString()
                );
            }
            Q_EMIT outputActionOrderChanged(m_node.id, ids);
        }
    );
    m_layout->addWidget(field(
        QString::fromUtf8("已连接结果动作的执行顺序"),
        list
    ));
}

void FunctionCanvasInspector::addPopupFields()
{
    QComboBox *templ = combo(
        choices("simple", "简洁",
                "detail", "详细",
                "compare", "对照",
                "outputOnly", "仅输出"),
        m_node.config.popup.resultTemplate,
        QStringLiteral("flowPopupTemplate")
    );
    connect(
        templ,
        static_cast<void (QComboBox::*)(int)>(
            &QComboBox::currentIndexChanged
        ),
        this,
        [this, templ](int) {
            m_node.config.popup.resultTemplate =
                templ->currentData().toString();
            emitNodeChange();
        }
    );
    m_layout->addWidget(field(
        QString::fromUtf8("模板"),
        templ
    ));

    QSpinBox *seconds = integerBox(
        0, 600,
        m_node.config.popup.displaySeconds,
        QStringLiteral("flowPopupSeconds"),
        this
    );
    seconds->setSpecialValueText(QString::fromUtf8("手动关闭"));
    seconds->setSuffix(QString::fromUtf8(" 秒"));
    connect(
        seconds,
        static_cast<void (QSpinBox::*)(int)>(
            &QSpinBox::valueChanged
        ),
        this,
        [this](int value) {
            m_node.config.popup.displaySeconds = value;
            emitNodeChange();
        }
    );
    m_layout->addWidget(field(
        QString::fromUtf8("显示时间"),
        seconds
    ));

    QComboBox *opacity = opacityBox(
        m_node.config.popup.opacity,
        QStringLiteral("flowPopupOpacity"),
        this
    );
    connect(
        opacity,
        static_cast<void (QComboBox::*)(int)>(
            &QComboBox::currentIndexChanged
        ),
        this,
        [this, opacity](int) {
            m_node.config.popup.opacity =
                opacity->currentData().toInt();
            emitNodeChange();
        }
    );
    m_layout->addWidget(field(
        QString::fromUtf8("透明度"),
        opacity
    ));

    auto *actions = new QListWidget;
    actions->setObjectName(QStringLiteral("flowPopupActions"));
    actions->setDragDropMode(QAbstractItemView::InternalMove);
    actions->setDefaultDropAction(Qt::MoveAction);
    for (const QString &id : m_node.config.popup.resultActions) {
        auto *item = new QListWidgetItem(
            popupActionDisplayName(id),
            actions
        );
        item->setData(Qt::UserRole, id);
    }
    connect(
        actions->model(),
        &QAbstractItemModel::rowsMoved,
        this,
        [this, actions]() {
            QStringList ids;
            for (int row = 0; row < actions->count(); ++row) {
                ids.append(
                    actions->item(row)
                        ->data(Qt::UserRole).toString()
                );
            }
            m_node.config.popup.resultActions = ids;
            emitNodeChange();
        }
    );
    m_layout->addWidget(field(
        QString::fromUtf8("允许的按钮顺序"),
        actions
    ));
}

void FunctionCanvasInspector::addScreenshotPanelFields()
{
    QSpinBox *seconds = integerBox(
        0, 600,
        m_node.config.screenshotPanel.displaySeconds,
        QStringLiteral("flowPanelSeconds"),
        this
    );
    seconds->setSpecialValueText(QString::fromUtf8("手动关闭"));
    seconds->setSuffix(QString::fromUtf8(" 秒"));
    connect(
        seconds,
        static_cast<void (QSpinBox::*)(int)>(
            &QSpinBox::valueChanged
        ),
        this,
        [this](int value) {
            m_node.config.screenshotPanel.displaySeconds = value;
            emitNodeChange();
        }
    );
    m_layout->addWidget(field(
        QString::fromUtf8("显示时间"),
        seconds
    ));
    QComboBox *opacity = opacityBox(
        m_node.config.screenshotPanel.opacity,
        QStringLiteral("flowPanelOpacity"),
        this
    );
    connect(
        opacity,
        static_cast<void (QComboBox::*)(int)>(
            &QComboBox::currentIndexChanged
        ),
        this,
        [this, opacity](int) {
            m_node.config.screenshotPanel.opacity =
                opacity->currentData().toInt();
            emitNodeChange();
        }
    );
    m_layout->addWidget(field(
        QString::fromUtf8("透明度"),
        opacity
    ));
}

void FunctionCanvasInspector::addAutoWriteFields()
{
    QComboBox *mode = combo(
        choices("insert", "插入",
                "replace", "替换"),
        m_node.config.autoWrite.writeMode,
        QStringLiteral("flowAutoWriteMode")
    );
    connect(
        mode,
        static_cast<void (QComboBox::*)(int)>(
            &QComboBox::currentIndexChanged
        ),
        this,
        [this, mode](int) {
            m_node.config.autoWrite.writeMode =
                mode->currentData().toString();
            emitNodeChange();
        }
    );
    m_layout->addWidget(field(
        QString::fromUtf8("写入方式"),
        mode
    ));
    auto *fallback = new QCheckBox(
        QString::fromUtf8("失败时转结果小框")
    );
    fallback->setObjectName(
        QStringLiteral("flowAutoWriteFallback")
    );
    fallback->setChecked(
        m_node.config.autoWrite.fallbackToPopup
    );
    connect(
        fallback,
        &QCheckBox::toggled,
        this,
        [this](bool checked) {
            m_node.config.autoWrite.fallbackToPopup = checked;
            emitNodeChange();
        }
    );
    m_layout->addWidget(fallback);
}

void FunctionCanvasInspector::emitNodeChange()
{
    Q_EMIT nodeChanged(m_node);
}
