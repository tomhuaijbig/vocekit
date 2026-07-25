#include "basic_settings_section.h"

#include "attention_message.h"
#include "ui_style.h"

#include "../config/app_settings_defaults.h"
#include "../platform/windows_autostart.h"

#include <QtWidgets>

namespace {

QString bssTr8(const char *text)
{
    return QString::fromUtf8(text);
}

} // namespace

BasicSettingsSection::BasicSettingsSection(
    Kind kind,
    const Callbacks &callbacks,
    QWidget *parent
)
    : QWidget(parent),
      m_kind(kind),
      m_callbacks(callbacks)
{
    buildRows();
}

void BasicSettingsSection::refreshFromSettings()
{
    const BasicSettingsSnapshot current = snapshot();
    if (m_autoStartBox && m_autoStartBox->isChecked() != current.autoStartEnabled) {
        m_autoStartBox->blockSignals(true);
        m_autoStartBox->setChecked(current.autoStartEnabled);
        m_autoStartBox->blockSignals(false);
    }
    if (m_strongSelectionBox && m_strongSelectionBox->isChecked() != current.strongSelectionEnabled) {
        m_strongSelectionBox->blockSignals(true);
        m_strongSelectionBox->setChecked(current.strongSelectionEnabled);
        m_strongSelectionBox->blockSignals(false);
    }
}

bool BasicSettingsSection::eventFilter(QObject *watched, QEvent *event)
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

void BasicSettingsSection::buildRows()
{
    auto *pageLayout = new QVBoxLayout(this);
    pageLayout->setContentsMargins(0, 0, 0, 0);

    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto *holder = new QWidget;
    auto *layout = new QVBoxLayout(holder);
    layout->setContentsMargins(22, 22, 22, 22);
    layout->setSpacing(14);

    if (m_kind == General) {
        addGeneralRows(layout);
    } else if (m_kind == Vocabulary) {
        addVocabularyRows(layout);
    } else if (m_kind == Voice) {
        addVoiceRows(layout);
    } else if (m_kind == Network) {
        addNetworkRows(layout);
    }

    layout->addStretch();
    scroll->setWidget(holder);
    pageLayout->addWidget(scroll);
}

void BasicSettingsSection::addGeneralRows(QVBoxLayout *layout)
{
    const BasicSettingsSnapshot current = snapshot();
    layout->addWidget(toggleRow(
        bssTr8("托盘常驻"),
        bssTr8("开启时关闭窗口只隐藏到托盘；关闭时关闭窗口会退出程序。"),
        current.trayResident,
        [this](bool enabled) {
            BasicSettingsSnapshot next = snapshot();
            next.trayResident = enabled;
            applyAndRefresh(next);
        }
    ));
    layout->addWidget(autoStartRow());
    layout->addWidget(strongSelectionRow());
}

void BasicSettingsSection::addVocabularyRows(QVBoxLayout *layout)
{
    const BasicSettingsSnapshot current = snapshot();
    layout->addWidget(toggleRow(
        bssTr8("启用词库"),
        bssTr8("开启后会在语音识别、模型处理和最终输出中使用词库规则。"),
        current.vocabularyEnabled,
        [this](bool enabled) {
            BasicSettingsSnapshot next = snapshot();
            next.vocabularyEnabled = enabled;
            applyAndRefresh(next);
        }
    ));
    layout->addWidget(comboRow(
        bssTr8("快捷键加入方式"),
        bssTr8("控制按“加入词库”快捷键时是否使用 AI 生成词条。"),
        QVector<QPair<QString, QString>>()
            << qMakePair(vocabularyAddModeTitle(vocabularyAddModeAi()), vocabularyAddModeAi())
            << qMakePair(vocabularyAddModeTitle(vocabularyAddModeAsk()), vocabularyAddModeAsk())
            << qMakePair(vocabularyAddModeTitle(vocabularyAddModeManual()), vocabularyAddModeManual()),
        current.vocabularyAddMode,
        [this](const QString &mode) {
            BasicSettingsSnapshot next = snapshot();
            next.vocabularyAddMode = mode;
            applyAndRefresh(next);
        }
    ));
    layout->addWidget(toggleRow(
        bssTr8("词库仅语音输入时启用"),
        bssTr8("开启后，只有本次功能实际使用语音输入时才启用词库。"),
        current.vocabularyOnlyForVoiceInput,
        [this](bool enabled) {
            BasicSettingsSnapshot next = snapshot();
            next.vocabularyOnlyForVoiceInput = enabled;
            applyAndRefresh(next);
        }
    ));
    layout->addWidget(numberRow(
        bssTr8("词库注入数量"),
        bssTr8("控制每次调用大模型时最多附加多少条当前输入命中的词库规则。0 表示不发给大模型。"),
        current.vocabularyPromptEntryLimit,
        0,
        100,
        bssTr8(" 条"),
        [this](int count) {
            BasicSettingsSnapshot next = snapshot();
            next.vocabularyPromptEntryLimit = count;
            applyAndRefresh(next);
        }
    ));
}

void BasicSettingsSection::addVoiceRows(QVBoxLayout *layout)
{
    const BasicSettingsSnapshot current = snapshot();
    layout->addWidget(toggleRow(
        bssTr8("启用浮动条"),
        bssTr8("开启后只在语音输入、识别和处理时临时显示；语音输入结束后自动关闭。"),
        current.floatingBarEnabled,
        [this](bool enabled) {
            BasicSettingsSnapshot next = snapshot();
            next.floatingBarEnabled = enabled;
            applyAndRefresh(next);
        }
    ));
    layout->addWidget(toggleRow(
        bssTr8("启用录音倒计时"),
        bssTr8("开启后按下快捷键不会立刻录音，会先显示 3 秒倒计时，避免还没准备好就开始说。"),
        current.preRecordCountdownEnabled,
        [this](bool enabled) {
            BasicSettingsSnapshot next = snapshot();
            next.preRecordCountdownEnabled = enabled;
            applyAndRefresh(next);
        }
    ));
    layout->addWidget(toggleRow(
        bssTr8("启用录音提示音"),
        bssTr8("开启后开始录音前会播放系统提示音。和倒计时可以同时使用，也可以单独使用。"),
        current.recordingBeepEnabled,
        [this](bool enabled) {
            BasicSettingsSnapshot next = snapshot();
            next.recordingBeepEnabled = enabled;
            applyAndRefresh(next);
        }
    ));
    layout->addWidget(toggleRow(
        bssTr8("听写后调用模型整理"),
        bssTr8("关闭时听写会跳过大模型，识别完成后直接写入，速度更快；开启后会更自然但更慢。"),
        current.dictatePolishEnabled,
        [this](bool enabled) {
            BasicSettingsSnapshot next = snapshot();
            next.dictatePolishEnabled = enabled;
            applyAndRefresh(next);
        }
    ));
}

void BasicSettingsSection::addNetworkRows(QVBoxLayout *layout)
{
    const BasicSettingsSnapshot current = snapshot();
    layout->addWidget(toggleRow(
        bssTr8("使用系统代理"),
        bssTr8("默认关闭，软件直连网络，不跟随 VPN 或 Windows 代理。注意：TUN 模式、透明代理或虚拟网卡会在网卡层接管流量，软件直连也可能被代理。"),
        current.useSystemProxy,
        [this](bool enabled) {
            BasicSettingsSnapshot next = snapshot();
            next.useSystemProxy = enabled;
            applyAndRefresh(next);
        }
    ));
}

QWidget *BasicSettingsSection::toggleRow(
    const QString &title,
    const QString &hint,
    bool checked,
    const std::function<void(bool)> &onChanged
)
{
    auto *frame = new QFrame;
    frame->setObjectName(QStringLiteral("card"));
    frame->setStyleSheet(cardStyle());
    auto *layout = new QHBoxLayout(frame);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(12);

    auto *labels = new QVBoxLayout;
    auto *name = new QLabel(title);
    name->setFont(appFont(11, QFont::DemiBold));
    labels->addWidget(name);

    auto *box = new QCheckBox;
    box->setChecked(checked);
    box->setFont(appFont(10, QFont::DemiBold));
    connect(box, &QCheckBox::toggled, this, [onChanged](bool enabled) {
        if (onChanged) {
            onChanged(enabled);
        }
    });

    layout->addLayout(labels, 1);
    layout->addWidget(box);
    attachSettingDetail(frame, title, settingDetailText(title, hint));
    return frame;
}

QWidget *BasicSettingsSection::numberRow(
    const QString &title,
    const QString &hint,
    int value,
    int minimum,
    int maximum,
    const QString &suffix,
    const std::function<void(int)> &onChanged
)
{
    auto *frame = new QFrame;
    frame->setObjectName(QStringLiteral("card"));
    frame->setStyleSheet(cardStyle());
    auto *layout = new QHBoxLayout(frame);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(12);

    auto *name = new QLabel(title);
    name->setFont(appFont(11, QFont::DemiBold));

    auto *box = new QSpinBox;
    box->setRange(minimum, maximum);
    box->setValue(qBound(minimum, value, maximum));
    box->setSuffix(suffix);
    box->setFixedWidth(112);
    box->setFont(appFont(10, QFont::DemiBold));
    box->setStyleSheet(QStringLiteral(
        "QSpinBox { background: #ffffff; border: 1px solid #d0d5dd; border-radius: 6px; padding: 4px 8px; }"
        "QSpinBox:focus { border: 1px solid #98a2b3; }"
    ));
    connect(box, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [onChanged](int current) {
        if (onChanged) {
            onChanged(current);
        }
    });

    layout->addWidget(name, 1);
    layout->addWidget(box);
    attachSettingDetail(frame, title, settingDetailText(title, hint));
    return frame;
}

QWidget *BasicSettingsSection::comboRow(
    const QString &title,
    const QString &hint,
    const QVector<QPair<QString, QString>> &items,
    const QString &currentValue,
    const std::function<void(const QString &)> &onChanged
)
{
    auto *frame = new QFrame;
    frame->setObjectName(QStringLiteral("card"));
    frame->setStyleSheet(cardStyle());
    auto *layout = new QHBoxLayout(frame);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(12);

    auto *name = new QLabel(title);
    name->setFont(appFont(11, QFont::DemiBold));

    auto *box = new QComboBox;
    for (const QPair<QString, QString> &item : items) {
        box->addItem(item.first, item.second);
    }
    const int index = box->findData(currentValue);
    box->setCurrentIndex(index >= 0 ? index : 0);
    box->setMinimumWidth(170);
    box->setFixedHeight(34);
    box->setFont(appFont(10, QFont::DemiBold));
    box->setStyleSheet(QStringLiteral(
        "QComboBox { background: #ffffff; border: 1px solid #d0d5dd; border-radius: 6px; padding: 0 10px; }"
        "QComboBox:focus { border: 1px solid #98a2b3; }"
    ));
    connect(box, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [box, onChanged](int) {
        if (onChanged) {
            onChanged(box->currentData().toString());
        }
    });

    layout->addWidget(name, 1);
    layout->addWidget(box);
    attachSettingDetail(frame, title, settingDetailText(title, hint));
    return frame;
}

QWidget *BasicSettingsSection::autoStartRow()
{
    auto *frame = new QFrame;
    frame->setObjectName(QStringLiteral("card"));
    frame->setStyleSheet(cardStyle());
    auto *layout = new QHBoxLayout(frame);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(12);

    auto *labels = new QVBoxLayout;
    auto *name = new QLabel(bssTr8("开机自启动"));
    name->setFont(appFont(11, QFont::DemiBold));
    labels->addWidget(name);

    auto *box = new QCheckBox;
    m_autoStartBox = box;
    box->setChecked(snapshot().autoStartEnabled);
    box->setFont(appFont(10, QFont::DemiBold));
    connect(box, &QCheckBox::toggled, this, [this, box](bool enabled) {
        QString error;
        if (!setWindowsAutoStartEnabled(enabled, &error)) {
            box->blockSignals(true);
            box->setChecked(!enabled);
            box->blockSignals(false);
            showAttentionWarning(this, bssTr8("开机自启动设置失败"), error);
            return;
        }

        BasicSettingsSnapshot next = snapshot();
        next.autoStartEnabled = enabled;
        applyAndRefresh(next);
    });

    layout->addLayout(labels, 1);
    layout->addWidget(box);
    attachSettingDetail(frame, bssTr8("开机自启动"), autoStartDetailText());
    return frame;
}

QWidget *BasicSettingsSection::strongSelectionRow()
{
    auto *frame = new QFrame;
    frame->setObjectName(QStringLiteral("card"));
    frame->setStyleSheet(cardStyle());
    auto *layout = new QHBoxLayout(frame);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(12);

    auto *labels = new QVBoxLayout;
    auto *name = new QLabel(bssTr8("强力选中"));
    name->setFont(appFont(11, QFont::DemiBold));
    labels->addWidget(name);

    auto *box = new QCheckBox;
    m_strongSelectionBox = box;
    box->setChecked(snapshot().strongSelectionEnabled);
    box->setFont(appFont(10, QFont::DemiBold));
    connect(box, &QCheckBox::toggled, this, [this](bool enabled) {
        BasicSettingsSnapshot next = snapshot();
        next.strongSelectionEnabled = enabled;
        applyAndRefresh(next);
    });

    layout->addLayout(labels, 1);
    layout->addWidget(box);
    attachSettingDetail(frame, bssTr8("强力选中"), strongSelectionDetailText());
    return frame;
}

void BasicSettingsSection::attachSettingDetail(
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

void BasicSettingsSection::installSettingDetailTarget(
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

void BasicSettingsSection::saveAndRefresh()
{
    if (m_callbacks.saveAndRefresh) {
        m_callbacks.saveAndRefresh();
    }
}

BasicSettingsSnapshot BasicSettingsSection::snapshot() const
{
    return m_callbacks.snapshotProvider
        ? m_callbacks.snapshotProvider()
        : BasicSettingsSnapshot();
}

void BasicSettingsSection::applyAndRefresh(
    const BasicSettingsSnapshot &settings)
{
    if (m_callbacks.applySnapshot) {
        m_callbacks.applySnapshot(settings);
    }
    saveAndRefresh();
}

QString BasicSettingsSection::settingDetailText(const QString &title, const QString &hint) const
{
    if (title == bssTr8("托盘常驻")) {
        return bssTr8("开启后，点击窗口关闭按钮时主界面会隐藏到系统托盘，后台快捷键、托盘菜单和语音功能继续可用。\n\n关闭后，点击窗口关闭按钮会退出程序，快捷键和后台功能也会停止。");
    }
    if (title == bssTr8("启用浮动条")) {
        return bssTr8("开启后，语音输入、识别和大模型处理时会在屏幕底部临时显示浮动条，用来显示当前状态、波形、复制、撤销和重试。\n\n关闭后，语音功能仍可用，但不会显示浮动条。");
    }
    if (title == bssTr8("启用词库")) {
        return bssTr8("开启后，听写、翻译、问答和自定义功能生成结果后，会按词库里的启用词条进行修正。\n\n词条可以设置作用范围。全局词条对所有功能生效；听写、翻译、问答或某个自定义功能词条只对对应功能生效。");
    }
    if (title == bssTr8("快捷键加入方式")) {
        return bssTr8("控制按“加入词库”快捷键时怎么处理选中文字。\n\n自动使用 AI：直接调用模型生成词条。\n每次询问：每次弹窗让你选择 AI 或手动填写。\n不使用 AI：直接打开手动词条编辑窗口。");
    }
    if (title == bssTr8("词库仅语音输入时启用")) {
        return bssTr8("开启后，只有这次功能实际使用了语音输入，词库才会参与预修正、发给大模型的少量词条提示和最终输出兜底修正。\n\n关闭后，只处理选中文字的翻译、问答和自定义功能也会使用词库。");
    }
    if (title == bssTr8("词库注入数量")) {
        return bssTr8("控制每次调用大模型时，最多把多少条“当前输入命中的词库规则”附加给模型。\n\n设为 0 表示不把词库规则发给大模型，但本地预修正和最终输出兜底仍然会使用完整词库。建议普通用户保持 16。");
    }
    if (title == bssTr8("启用录音倒计时")) {
        return bssTr8("开启后，按下需要语音输入的快捷键时不会立刻录音，会先显示倒计时。倒计时秒数可以在左侧“功能自定义”的每个功能里单独设置。\n\n适合需要一点准备时间再开始说话的场景。");
    }
    if (title == bssTr8("启用录音提示音")) {
        return bssTr8("开启后，开始录音前会播放提示音。提示音可以使用系统默认声音，也可以在“功能自定义”里给每个功能单独指定声音文件。\n\n它只影响需要语音输入的功能。");
    }
    if (title == bssTr8("听写后调用模型整理")) {
        return bssTr8("开启后，听写会先进行语音识别，再调用当前听写功能选择的大模型，把口语整理成更自然、清晰、可直接粘贴的文字。\n\n关闭后，识别结果会直接输出，速度更快，也能减少大模型网络失败的概率。");
    }
    if (title == bssTr8("使用系统代理")) {
        return bssTr8("开启后，大模型和语音接口请求会跟随 Windows 系统代理。\n\n关闭后，软件尽量直连网络，不主动使用系统代理。注意：TUN 模式、透明代理或虚拟网卡会在更底层接管流量，即使这里关闭代理，也可能仍被代理软件影响。");
    }
    return hint.trimmed().isEmpty() ? title : hint;
}

QString BasicSettingsSection::autoStartDetailText()
{
    return bssTr8("开启后，软件会写入 Windows 当前用户启动项，电脑登录后自动启动 vocekit，但只进入托盘后台，不会自动打开主界面。\n\n如果保存失败，通常是安全软件拦截了注册表写入。关闭后会移除这个启动项。");
}

QString BasicSettingsSection::strongSelectionDetailText()
{
    return bssTr8("普通选中读取会优先使用 Windows 文本接口，不会立即模拟 Ctrl+C，更安全但兼容性有限。\n\n开启强力选中后，当普通读取失败时，软件会用更强的方式尝试获取选中文字，能兼容更多网页、文档和特殊控件，但可能被安全软件提示风险。");
}
