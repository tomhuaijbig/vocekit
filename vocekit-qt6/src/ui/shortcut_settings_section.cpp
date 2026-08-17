#include "shortcut_settings_section.h"

#include "app_dialogs.h"
#include "attention_message.h"
#include "shortcut_display.h"
#include "ui_style.h"

#include "../capture/screenshot_types.h"

#include <QtWidgets>

namespace {

QString sssTr8(const char *text)
{
    return QString::fromUtf8(text);
}

QString shortcutBadgeStyle()
{
    return QStringLiteral(
        "QLabel {"
        "  background: #f2f4f7;"
        "  border: 1px solid #d0d5dd;"
        "  border-radius: 6px;"
        "  color: #111827;"
        "  font-weight: 600;"
        "}"
    );
}

QString keyEditorStyle()
{
    return QStringLiteral(
        "QKeySequenceEdit {"
        "  background: #ffffff;"
        "  border: 1px solid #d0d5dd;"
        "  border-radius: 6px;"
        "  padding: 8px;"
        "}"
    );
}

} // namespace

ShortcutSettingsSection::ShortcutSettingsSection(
    const Callbacks &callbacks,
    QWidget *parent
)
    : QWidget(parent),
      m_callbacks(callbacks)
{
    auto *pageLayout = new QVBoxLayout(this);
    pageLayout->setContentsMargins(0, 0, 0, 0);

    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto *holder = new QWidget;
    m_rowsLayout = new QVBoxLayout(holder);
    m_rowsLayout->setContentsMargins(22, 22, 22, 22);
    m_rowsLayout->setSpacing(14);

    scroll->setWidget(holder);
    pageLayout->addWidget(scroll, 1);

    refreshFromSettings();
}

void ShortcutSettingsSection::refreshFromSettings()
{
    if (!m_rowsLayout) {
        return;
    }

    while (QLayoutItem *item = m_rowsLayout->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
    m_hotkeyLabels.clear();

    const ShortcutSettingsSnapshot current = snapshot();
    m_rowsLayout->addWidget(shortcutSectionLabel(sssTr8("内置快捷键")));
    for (const ShortcutSettingsItem &item : current.builtInItems) {
        m_rowsLayout->addWidget(hotkeyRow(item));
    }

    if (!current.customItems.isEmpty()) {
        m_rowsLayout->addWidget(shortcutSectionLabel(sssTr8("自定义功能快捷键")));
        for (const ShortcutSettingsItem &item : current.customItems) {
            m_rowsLayout->addWidget(customHotkeyRow(item));
        }
    }

    bool hasScreenshotShortcut = false;
    for (const ShortcutSettingsItem &item : current.builtInItems) {
        hasScreenshotShortcut = hasScreenshotShortcut || item.screenshotShortcutEnabled;
    }
    for (const ShortcutSettingsItem &item : current.customItems) {
        hasScreenshotShortcut = hasScreenshotShortcut || item.screenshotShortcutEnabled;
    }

    if (hasScreenshotShortcut) {
        m_rowsLayout->addWidget(shortcutSectionLabel(sssTr8("截图快捷键")));
        for (const ShortcutSettingsItem &item : current.builtInItems) {
            if (item.screenshotShortcutEnabled) {
                m_rowsLayout->addWidget(screenshotHotkeyRow(item));
            }
        }
        for (const ShortcutSettingsItem &item : current.customItems) {
            if (item.screenshotShortcutEnabled) {
                m_rowsLayout->addWidget(screenshotHotkeyRow(item));
            }
        }
    }

    m_rowsLayout->addStretch();
}

bool ShortcutSettingsSection::eventFilter(QObject *watched, QEvent *event)
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

QLabel *ShortcutSettingsSection::shortcutSectionLabel(const QString &title)
{
    auto *label = new QLabel(title);
    label->setFont(appFont(13, QFont::DemiBold));
    label->setStyleSheet(QStringLiteral("color: #111827; padding-top: 4px;"));
    return label;
}

QWidget *ShortcutSettingsSection::hotkeyRow(const ShortcutSettingsItem &item)
{
    auto *frame = new QFrame;
    frame->setObjectName(QStringLiteral("card"));
    frame->setStyleSheet(cardStyle());
    auto *layout = new QHBoxLayout(frame);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(10);

    auto *labels = new QVBoxLayout;
    auto *name = new QLabel(item.title);
    name->setFont(appFont(11, QFont::DemiBold));
    labels->addWidget(name);

    auto *key = new QLabel(displayShortcut(item.shortcut));
    key->setAlignment(Qt::AlignCenter);
    key->setFixedHeight(34);
    key->setMinimumWidth(170);
    key->setStyleSheet(shortcutBadgeStyle());
    m_hotkeyLabels.insert(item.id, key);

    auto *change = new QPushButton(sssTr8("更改"));
    change->setFixedHeight(34);
    change->setCursor(Qt::PointingHandCursor);
    change->setStyleSheet(buttonStyle(QStringLiteral("#111827")));
    connect(change, &QPushButton::clicked, this, [this, item]() {
        editHotkey(item);
    });

    auto *reset = new QPushButton(sssTr8("重置"));
    reset->setFixedHeight(34);
    reset->setCursor(Qt::PointingHandCursor);
    reset->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
    connect(reset, &QPushButton::clicked, this, [this, item]() {
        if (!m_callbacks.setFunctionShortcut) {
            return;
        }
        m_callbacks.setFunctionShortcut(item.id, item.defaultShortcut);
        saveAndRefresh();
    });

    layout->addLayout(labels, 1);
    layout->addWidget(key);
    layout->addWidget(change);
    layout->addWidget(reset);
    attachSettingDetail(frame, item.title, hotkeyDetailText(item));
    return frame;
}

QWidget *ShortcutSettingsSection::customHotkeyRow(const ShortcutSettingsItem &item)
{
    auto *frame = new QFrame;
    frame->setObjectName(QStringLiteral("card"));
    frame->setStyleSheet(cardStyle());
    auto *layout = new QHBoxLayout(frame);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(10);

    auto *labels = new QVBoxLayout;
    auto *name = new QLabel(item.title.trimmed().isEmpty() ? sssTr8("自定义功能") : item.title.trimmed());
    name->setFont(appFont(11, QFont::DemiBold));
    labels->addWidget(name);

    auto *key = new QLabel(displayShortcut(item.shortcut));
    key->setAlignment(Qt::AlignCenter);
    key->setFixedHeight(34);
    key->setMinimumWidth(170);
    key->setStyleSheet(shortcutBadgeStyle());
    m_hotkeyLabels.insert(item.id, key);

    auto *change = new QPushButton(sssTr8("更改"));
    change->setFixedHeight(34);
    change->setCursor(Qt::PointingHandCursor);
    change->setStyleSheet(buttonStyle(QStringLiteral("#111827")));
    connect(change, &QPushButton::clicked, this, [this, item]() {
        editCustomHotkey(item);
    });

    auto *reset = new QPushButton(sssTr8("重置"));
    reset->setFixedHeight(34);
    reset->setCursor(Qt::PointingHandCursor);
    reset->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
    connect(reset, &QPushButton::clicked, this, [this, item]() {
        ShortcutSettingsItem current;
        if (!shortcutItemById(item.id, &current)) {
            showAttentionWarning(this, sssTr8("自定义功能不存在"), sssTr8("这个自定义功能已经被删除，请刷新后再试。"));
            return;
        }
        const QString shortcut = suggestedShortcutForCustom(current.id);
        if (shortcut.trimmed().isEmpty()) {
            showAttentionWarning(this, sssTr8("没有可用快捷键"), sssTr8("没有找到可自动分配的推荐快捷键，请手动设置。"));
            return;
        }
        if (m_callbacks.setFunctionShortcut) {
            m_callbacks.setFunctionShortcut(current.id, shortcut);
        }
        saveAndRefresh();
    });

    layout->addLayout(labels, 1);
    layout->addWidget(key);
    layout->addWidget(change);
    layout->addWidget(reset);
    attachSettingDetail(frame, name->text(), customHotkeyDetailText(item));
    return frame;
}

QWidget *ShortcutSettingsSection::screenshotHotkeyRow(
    const ShortcutSettingsItem &item)
{
    auto *frame = new QFrame;
    frame->setObjectName(QStringLiteral("card"));
    frame->setStyleSheet(cardStyle());
    auto *layout = new QHBoxLayout(frame);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(10);

    auto *labels = new QVBoxLayout;
    auto *name = new QLabel(item.title + sssTr8("截图"));
    name->setFont(appFont(11, QFont::DemiBold));
    labels->addWidget(name);

    auto *key = new QLabel(displayShortcut(item.screenshotShortcut));
    key->setAlignment(Qt::AlignCenter);
    key->setFixedHeight(34);
    key->setMinimumWidth(170);
    key->setStyleSheet(shortcutBadgeStyle());
    m_hotkeyLabels.insert(screenshotHotkeyLogicalId(item.id), key);

    auto *change = new QPushButton(sssTr8("更改"));
    change->setFixedHeight(34);
    change->setCursor(Qt::PointingHandCursor);
    change->setStyleSheet(buttonStyle(QStringLiteral("#111827")));
    connect(change, &QPushButton::clicked, this, [this, item]() {
        editScreenshotHotkey(item);
    });

    auto *reset = new QPushButton(sssTr8("重置"));
    reset->setFixedHeight(34);
    reset->setCursor(Qt::PointingHandCursor);
    reset->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
    connect(reset, &QPushButton::clicked, this, [this, item]() {
        if (!m_callbacks.setScreenshotShortcut) {
            return;
        }
        m_callbacks.setScreenshotShortcut(item.id, item.defaultScreenshotShortcut);
        saveAndRefresh();
    });

    layout->addLayout(labels, 1);
    layout->addWidget(key);
    layout->addWidget(change);
    layout->addWidget(reset);
    attachSettingDetail(frame, name->text(), screenshotHotkeyDetailText());
    return frame;
}

ShortcutSettingsSnapshot ShortcutSettingsSection::snapshot() const
{
    return m_callbacks.snapshotProvider
        ? m_callbacks.snapshotProvider()
        : ShortcutSettingsSnapshot();
}

bool ShortcutSettingsSection::shortcutItemById(
    const QString &id,
    ShortcutSettingsItem *out) const
{
    const ShortcutSettingsSnapshot current = snapshot();
    QVector<ShortcutSettingsItem> items = current.builtInItems;
    items += current.customItems;
    for (const ShortcutSettingsItem &item : items) {
        if (item.id == id) {
            if (out) {
                *out = item;
            }
            return true;
        }
    }
    return false;
}

QString ShortcutSettingsSection::suggestedShortcutForCustom(const QString &id) const
{
    if (!m_callbacks.conflictsWithOther) {
        return QString();
    }
    for (int i = 1; i <= 9; ++i) {
        const QString value = QStringLiteral("Ctrl+Alt+%1").arg(i);
        QString otherTitle;
        if (!m_callbacks.conflictsWithOther(id, value, &otherTitle)) {
            return value;
        }
    }
    return QString();
}

void ShortcutSettingsSection::editHotkey(const ShortcutSettingsItem &item)
{
    AppDialog dialog(this);
    dialog.setWindowTitle(sssTr8("更改快捷键"));
    dialog.setModal(true);
    dialog.setMinimumWidth(420);

    auto *root = new QVBoxLayout(&dialog);
    root->setContentsMargins(18, 16, 18, 16);
    root->setSpacing(12);

    auto *title = new QLabel(item.title);
    title->setFont(appFont(14, QFont::DemiBold));
    root->addWidget(title);

    auto *editor = new QKeySequenceEdit(QKeySequence(item.shortcut));
    editor->setFixedHeight(42);
    editor->setStyleSheet(keyEditorStyle());
    root->addWidget(editor);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    auto *cancel = new QPushButton(sssTr8("取消"));
    cancel->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
    auto *ok = new QPushButton(sssTr8("保存"));
    ok->setStyleSheet(buttonStyle(QStringLiteral("#111827")));
    buttons->addWidget(cancel);
    buttons->addWidget(ok);
    root->addLayout(buttons);

    connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(ok, &QPushButton::clicked, &dialog, [&]() {
        const QString value = editor->keySequence().toString(QKeySequence::PortableText);
        if (value.trimmed().isEmpty()) {
            showAttentionWarning(&dialog, sssTr8("快捷键无效"), sssTr8("请选择一个有效的快捷键。"));
            return;
        }

        QString otherTitle;
        if (m_callbacks.conflictsWithOther
            && m_callbacks.conflictsWithOther(item.id, value, &otherTitle)) {
            showAttentionWarning(
                &dialog,
                sssTr8("快捷键冲突"),
                sssTr8("无法把快捷键“") + displayShortcut(value) + sssTr8("”分配给“") + item.title
                    + sssTr8("”，因为它已经被“") + otherTitle + sssTr8("”使用。请修改其中一个快捷键。")
            );
            return;
        }

        if (m_callbacks.setFunctionShortcut) {
            m_callbacks.setFunctionShortcut(item.id, value);
        }
        saveAndRefresh();
        dialog.accept();
    });

    dialog.exec();
}

void ShortcutSettingsSection::editCustomHotkey(const ShortcutSettingsItem &item)
{
    ShortcutSettingsItem current;
    if (!shortcutItemById(item.id, &current)) {
        showAttentionWarning(this, sssTr8("自定义功能不存在"), sssTr8("这个自定义功能已经被删除，请刷新后再试。"));
        return;
    }

    AppDialog dialog(this);
    dialog.setWindowTitle(sssTr8("更改快捷键"));
    dialog.setModal(true);
    dialog.setMinimumWidth(420);

    auto *root = new QVBoxLayout(&dialog);
    root->setContentsMargins(18, 16, 18, 16);
    root->setSpacing(12);

    auto *title = new QLabel(current.title.trimmed().isEmpty() ? sssTr8("自定义功能") : current.title.trimmed());
    title->setFont(appFont(14, QFont::DemiBold));
    root->addWidget(title);

    auto *editor = new QKeySequenceEdit(QKeySequence(current.shortcut));
    editor->setFixedHeight(42);
    editor->setStyleSheet(keyEditorStyle());
    root->addWidget(editor);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    auto *cancel = new QPushButton(sssTr8("取消"));
    cancel->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
    auto *ok = new QPushButton(sssTr8("保存"));
    ok->setStyleSheet(buttonStyle(QStringLiteral("#111827")));
    buttons->addWidget(cancel);
    buttons->addWidget(ok);
    root->addLayout(buttons);

    connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(ok, &QPushButton::clicked, &dialog, [this, &dialog, editor, current]() mutable {
        const QString value = editor->keySequence().toString(QKeySequence::PortableText);
        if (value.trimmed().isEmpty()) {
            showAttentionWarning(&dialog, sssTr8("快捷键无效"), sssTr8("请选择一个有效的快捷键。"));
            return;
        }

        QString otherTitle;
        if (m_callbacks.conflictsWithOther
            && m_callbacks.conflictsWithOther(current.id, value, &otherTitle)) {
            showAttentionWarning(
                &dialog,
                sssTr8("快捷键冲突"),
                sssTr8("无法把快捷键“") + displayShortcut(value) + sssTr8("”分配给“") + current.title
                    + sssTr8("”，因为它已经被“") + otherTitle + sssTr8("”使用。请修改其中一个快捷键。")
            );
            return;
        }

        if (m_callbacks.setFunctionShortcut) {
            m_callbacks.setFunctionShortcut(current.id, value);
        }
        saveAndRefresh();
        dialog.accept();
    });

    dialog.exec();
}

void ShortcutSettingsSection::editScreenshotHotkey(
    const ShortcutSettingsItem &item)
{
    AppDialog dialog(this);
    dialog.setWindowTitle(sssTr8("更改截图快捷键"));
    dialog.setModal(true);
    dialog.setMinimumWidth(440);

    auto *root = new QVBoxLayout(&dialog);
    root->setContentsMargins(18, 16, 18, 16);
    root->setSpacing(12);

    auto *title = new QLabel(item.title + sssTr8("截图"));
    title->setFont(appFont(14, QFont::DemiBold));
    root->addWidget(title);

    auto *editor = new QKeySequenceEdit(QKeySequence(
        item.screenshotShortcut
    ));
    editor->setFixedHeight(42);
    editor->setStyleSheet(keyEditorStyle());
    root->addWidget(editor);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    auto *cancel = new QPushButton(sssTr8("取消"));
    cancel->setStyleSheet(buttonStyle(
        QStringLiteral("#ffffff"),
        QStringLiteral("#111827")
    ));
    auto *ok = new QPushButton(sssTr8("保存"));
    ok->setStyleSheet(buttonStyle(QStringLiteral("#111827")));
    buttons->addWidget(cancel);
    buttons->addWidget(ok);
    root->addLayout(buttons);

    connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(ok, &QPushButton::clicked, &dialog, [this, &dialog, editor, item]() {
        const QString value = editor->keySequence()
            .toString(QKeySequence::PortableText)
            .trimmed();
        if (value.isEmpty()) {
            showAttentionWarning(
                &dialog,
                sssTr8("截图快捷键不能为空"),
                sssTr8("请选择一个有效的截图快捷键。")
            );
            return;
        }

        QString otherTitle;
        if (m_callbacks.conflictsWithOther
            && m_callbacks.conflictsWithOther(
            screenshotHotkeyLogicalId(item.id),
            value,
            &otherTitle)) {
            showAttentionWarning(
                &dialog,
                sssTr8("快捷键冲突"),
                sssTr8("无法把截图快捷键“")
                    + displayShortcut(value)
                    + sssTr8("”分配给“")
                    + item.title
                    + sssTr8("截图”，因为它已经被“")
                    + otherTitle
                    + sssTr8("”使用。")
            );
            return;
        }
        if (m_callbacks.setScreenshotShortcut) {
            m_callbacks.setScreenshotShortcut(item.id, value);
        }
        saveAndRefresh();
        dialog.accept();
    });

    dialog.exec();
}

void ShortcutSettingsSection::attachSettingDetail(
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

void ShortcutSettingsSection::installSettingDetailTarget(
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

void ShortcutSettingsSection::saveAndRefresh()
{
    if (m_callbacks.saveAndRefresh) {
        m_callbacks.saveAndRefresh();
    } else {
        refreshFromSettings();
    }
}

QString ShortcutSettingsSection::hotkeyDetailText(
    const ShortcutSettingsItem &item) const
{
    return sssTr8("当前快捷键：") + displayShortcut(item.shortcut)
        + sssTr8("\n\n作用：") + item.hint
        + sssTr8("\n\n点击“更改”可以重新录入快捷键；点击“重置”会恢复默认快捷键。保存时会检测是否和其它功能冲突。");
}

QString ShortcutSettingsSection::customHotkeyDetailText(
    const ShortcutSettingsItem &item)
{
    return sssTr8("当前快捷键：") + displayShortcut(item.shortcut)
        + sssTr8("\n\n这是左侧“功能自定义”里创建的自定义功能快捷键。点击“更改”可以重新录入快捷键；点击“重置”会自动选择一个当前未被占用的推荐快捷键。\n\n保存时会检测是否和听写、翻译、问答、加入词库、打开主界面或其它自定义功能冲突。");
}

QString ShortcutSettingsSection::screenshotHotkeyDetailText()
{
    return sssTr8("这个快捷键只负责开始截图，不会直接开始录音。它只在该功能选择了独立截图快捷键触发方式时注册。");
}
