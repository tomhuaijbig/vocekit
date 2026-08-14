#include "selection_context_settings_card.h"

#include "ui_style.h"

#include "../domain/selection_context_actions.h"

#include <QtWidgets>

namespace {

QString sccTr8(const char *text)
{
    return QString::fromUtf8(text);
}

QStringList normalizeExecutables(const QStringList &values)
{
    QStringList normalized;
    for (const QString &value : values) {
        const QString executable = QFileInfo(value.trimmed())
            .fileName()
            .toLower();
        if (!executable.isEmpty() && !normalized.contains(executable)) {
            normalized.append(executable);
        }
    }
    return normalized;
}

QVector<QPair<QString, QString>> defaultCatalog()
{
    QVector<QPair<QString, QString>> result;
    for (const QString &id : defaultSelectionContextActionOrder()) {
        result.append(qMakePair(selectionContextActionTitle(id), id));
    }
    return result;
}

QLabel *descriptionLabel(const QString &text, QWidget *parent)
{
    QLabel *label = new QLabel(text, parent);
    label->setWordWrap(true);
    label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    label->setStyleSheet(QStringLiteral("color:#667085;"));
    label->setMinimumHeight(label->sizeHint().height());
    return label;
}

QWidget *toggleLine(
    const QString &title,
    const QString &detail,
    QCheckBox **box,
    QWidget *parent)
{
    QFrame *row = new QFrame(parent);
    row->setObjectName(QStringLiteral("selectionContextSettingRow"));
    QHBoxLayout *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 4, 0, 4);
    layout->setSpacing(12);
    QVBoxLayout *labels = new QVBoxLayout;
    labels->setSpacing(3);
    QLabel *name = new QLabel(title, row);
    QFont titleFont = name->font();
    titleFont.setWeight(QFont::DemiBold);
    name->setFont(titleFont);
    name->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    name->setMinimumHeight(name->sizeHint().height());
    labels->addWidget(name);
    if (!detail.trimmed().isEmpty()) {
        labels->addWidget(descriptionLabel(detail, row));
    }
    *box = new QCheckBox(row);
    layout->addLayout(labels, 1);
    layout->addWidget(*box, 0, Qt::AlignVCenter);
    return row;
}

} // namespace

SelectionContextSettingsCard::SelectionContextSettingsCard(
    const SelectionContextSettings &settings,
    const Callbacks &callbacks,
    QWidget *parent)
    : QFrame(parent),
      m_settings(settings),
      m_callbacks(callbacks),
      m_actionCatalog(defaultCatalog())
{
    setObjectName(QStringLiteral("selectionContextSettingsCard"));
    setFrameShape(QFrame::NoFrame);
    setStyleSheet(QStringLiteral(
        "QFrame#selectionContextSettingsCard {"
        " background:#ffffff; border:1px solid #dfe4ec; border-radius:10px;"
        "}"
        "QFrame#selectionContextSettingsCard QLabel { border:none; }"
        "QFrame#selectionContextSettingsCard QPushButton {"
        " padding:0px 12px;"
        "}"
    ));
    buildUi();
    setSettings(settings);
}

SelectionContextSettings SelectionContextSettingsCard::settings() const
{
    return m_settings;
}

void SelectionContextSettingsCard::setSettings(
    const SelectionContextSettings &settings)
{
    m_updating = true;
    m_settings = settings;
    m_settings.minimumTextLength = qBound(
        1,
        m_settings.minimumTextLength,
        1000
    );
    m_settings.pauseMinutes = qBound(1, m_settings.pauseMinutes, 1440);
    m_settings.blockedApplications = normalizeExecutables(
        m_settings.blockedApplications
    );
    if (m_enabled) {
        m_enabled->setChecked(m_settings.enabled);
        m_keyboard->setChecked(m_settings.keyboardSelectionEnabled);
        m_minimumLength->setValue(m_settings.minimumTextLength);
        m_closeOutside->setChecked(m_settings.closeOnOutsideClick);
        m_pin->setChecked(m_settings.pinEnabled);
        m_pauseMinutes->setValue(m_settings.pauseMinutes);
        m_blockedApplications->setPlainText(
            m_settings.blockedApplications.join(QStringLiteral("\n"))
        );
        rebuildActionList();
        updateConsentResetVisibility();
    }
    m_updating = false;
}

void SelectionContextSettingsCard::setActionCatalog(
    const QVector<QPair<QString, QString>> &catalog)
{
    QVector<QPair<QString, QString>> normalized;
    QStringList ids;
    for (const QPair<QString, QString> &option : catalog) {
        const QString id = option.second.trimmed();
        if (id.isEmpty() || ids.contains(id)) {
            continue;
        }
        ids.append(id);
        normalized.append(qMakePair(
            option.first.trimmed().isEmpty() ? id : option.first.trimmed(),
            id
        ));
    }
    if (normalized.isEmpty()) {
        normalized = defaultCatalog();
    }
    m_actionCatalog = normalized;
    rebuildActionList();
}

void SelectionContextSettingsCard::changeEvent(QEvent *event)
{
    QFrame::changeEvent(event);
    if (event && event->type() == QEvent::FontChange) {
        updateButtonMetrics();
    }
}

void SelectionContextSettingsCard::buildUi()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(13);

    QLabel *title = new QLabel(sccTr8("选中文字工具条"), this);
    title->setObjectName(QStringLiteral("selectionContextSettingsTitle"));
    QFont titleFont = title->font();
    if (titleFont.pixelSize() > 0) {
        titleFont.setPixelSize(titleFont.pixelSize() + 4);
    } else {
        titleFont.setPointSize(qMax(1, titleFont.pointSize() + 3));
    }
    titleFont.setWeight(QFont::DemiBold);
    title->setFont(titleFont);
    title->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    title->setMinimumHeight(title->sizeHint().height());
    layout->addWidget(title);
    QLabel *summary = descriptionLabel(
        sccTr8("选中文字后显示快捷操作；复制和保存始终在本地执行，模型动作首次发送前会明确询问。"),
        this
    );
    summary->setObjectName(QStringLiteral("selectionContextSettingsSummary"));
    layout->addWidget(summary);

    layout->addWidget(toggleLine(
        sccTr8("启用自动弹出"),
        sccTr8("关闭后仍可通过专用快捷键手动打开。"),
        &m_enabled,
        this
    ));
    m_enabled->setObjectName(QStringLiteral("selectionContextEnabledToggle"));
    layout->addWidget(toggleLine(
        sccTr8("检测键盘选区"),
        sccTr8("允许 Shift 加方向键等键盘选择操作触发检测。"),
        &m_keyboard,
        this
    ));
    m_keyboard->setObjectName(QStringLiteral("selectionContextKeyboardToggle"));

    QFrame *minimumRow = new QFrame(this);
    QHBoxLayout *minimumLayout = new QHBoxLayout(minimumRow);
    minimumLayout->setContentsMargins(0, 4, 0, 4);
    QLabel *minimumLabel = new QLabel(sccTr8("自动弹出最小文字长度"), minimumRow);
    minimumLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    minimumLabel->setMinimumHeight(minimumLabel->sizeHint().height());
    m_minimumLength = new QSpinBox(minimumRow);
    m_minimumLength->setObjectName(QStringLiteral("selectionContextMinimumLengthSpin"));
    m_minimumLength->setRange(1, 1000);
    m_minimumLength->setSuffix(sccTr8(" 字"));
    minimumLayout->addWidget(minimumLabel, 1);
    minimumLayout->addWidget(m_minimumLength);
    layout->addWidget(minimumRow);

    layout->addWidget(toggleLine(
        sccTr8("点击空白处自动关闭"),
        sccTr8("固定结果卡不受此选项影响。"),
        &m_closeOutside,
        this
    ));
    m_closeOutside->setObjectName(QStringLiteral("selectionContextCloseOutsideToggle"));
    layout->addWidget(toggleLine(
        sccTr8("允许固定结果卡"),
        sccTr8("固定后更换选区不会关闭结果，但不能替换原选区。"),
        &m_pin,
        this
    ));
    m_pin->setObjectName(QStringLiteral("selectionContextPinToggle"));

    QFrame *pauseRow = new QFrame(this);
    QHBoxLayout *pauseLayout = new QHBoxLayout(pauseRow);
    pauseLayout->setContentsMargins(0, 4, 0, 4);
    QLabel *pauseLabel = new QLabel(sccTr8("托盘暂停时长"), pauseRow);
    pauseLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    pauseLabel->setMinimumHeight(pauseLabel->sizeHint().height());
    m_pauseMinutes = new QSpinBox(pauseRow);
    m_pauseMinutes->setObjectName(QStringLiteral("selectionContextPauseMinutesSpin"));
    m_pauseMinutes->setRange(1, 1440);
    m_pauseMinutes->setSuffix(sccTr8(" 分钟"));
    pauseLayout->addWidget(pauseLabel, 1);
    pauseLayout->addWidget(m_pauseMinutes);
    layout->addWidget(pauseRow);

    QLabel *actionTitle = new QLabel(sccTr8("工具条按钮顺序"), this);
    QFont sectionFont = actionTitle->font();
    sectionFont.setWeight(QFont::DemiBold);
    actionTitle->setFont(sectionFont);
    actionTitle->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    actionTitle->setMinimumHeight(actionTitle->sizeHint().height());
    layout->addWidget(actionTitle);
    layout->addWidget(descriptionLabel(
        sccTr8("拖动调整顺序。自定义功能会继续显示在“更多”菜单中。"),
        this
    ));
    m_actions = new QListWidget(this);
    m_actions->setObjectName(QStringLiteral("selectionContextActionList"));
    m_actions->setDragDropMode(QAbstractItemView::InternalMove);
    m_actions->setDefaultDropAction(Qt::MoveAction);
    m_actions->setSelectionMode(QAbstractItemView::SingleSelection);
    m_actions->setMinimumHeight(178);
    m_actions->setMaximumHeight(286);
    layout->addWidget(m_actions);

    QLabel *blockedTitle = new QLabel(sccTr8("不显示工具条的应用"), this);
    blockedTitle->setFont(sectionFont);
    blockedTitle->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    blockedTitle->setMinimumHeight(blockedTitle->sizeHint().height());
    layout->addWidget(blockedTitle);
    layout->addWidget(descriptionLabel(
        sccTr8("每行填写一个可执行文件名，例如 password-manager.exe。完整路径会自动规范为文件名。"),
        this
    ));
    m_blockedApplications = new QTextEdit(this);
    m_blockedApplications->setObjectName(
        QStringLiteral("selectionContextBlockedApplicationsEdit")
    );
    m_blockedApplications->setAcceptRichText(false);
    m_blockedApplications->setMinimumHeight(100);
    m_blockedApplications->setPlaceholderText(
        sccTr8("例如：\npassword-manager.exe\nprivate-notes.exe")
    );
    layout->addWidget(m_blockedApplications);

    QHBoxLayout *strongLayout = new QHBoxLayout;
    QLabel *strongHint = descriptionLabel(
        sccTr8("读取兼容性仍由“强力选中”全局设置控制，这里不会创建第二份开关。"),
        this
    );
    m_strongSelectionLink = new QPushButton(sccTr8("查看强力选中说明"), this);
    m_strongSelectionLink->setObjectName(
        QStringLiteral("selectionContextStrongSelectionLink")
    );
    strongLayout->addWidget(strongHint, 1);
    strongLayout->addWidget(m_strongSelectionLink, 0, Qt::AlignTop);
    layout->addLayout(strongLayout);

    m_resetConsent = new QPushButton(sccTr8("下次发送前再次提示"), this);
    m_resetConsent->setObjectName(
        QStringLiteral("selectionContextResetConsentButton")
    );
    m_resetConsent->setToolTip(
        sccTr8("清除已确认状态；下一次模型动作会再次询问是否发送选中文字。")
    );
    layout->addWidget(m_resetConsent, 0, Qt::AlignLeft);

    const QList<QCheckBox *> toggles = QList<QCheckBox *>()
        << m_enabled << m_keyboard << m_closeOutside << m_pin;
    for (QCheckBox *toggle : toggles) {
        connect(toggle, &QCheckBox::toggled, this, [this](bool) {
            notifyChanged();
        });
    }
    connect(
        m_minimumLength,
        static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
        this,
        [this](int) { notifyChanged(); }
    );
    connect(
        m_pauseMinutes,
        static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
        this,
        [this](int) { notifyChanged(); }
    );
    connect(m_blockedApplications, &QTextEdit::textChanged, this, [this]() {
        notifyChanged();
    });
    connect(m_actions->model(), &QAbstractItemModel::rowsMoved,
            this, [this]() { queueActionOrderChanged(); });
    connect(m_actions->model(), &QAbstractItemModel::rowsInserted,
            this, [this]() { queueActionOrderChanged(); });
    connect(m_actions->model(), &QAbstractItemModel::rowsRemoved,
            this, [this]() { queueActionOrderChanged(); });
    connect(m_resetConsent, &QPushButton::clicked, this, [this]() {
        if (m_updating) {
            return;
        }
        m_settings.networkConsentAcknowledged = false;
        updateConsentResetVisibility();
        if (m_callbacks.settingsChanged) {
            m_callbacks.settingsChanged(m_settings);
        }
    });
    connect(m_strongSelectionLink, &QPushButton::clicked, this, [this]() {
        const std::function<void()> callback =
            m_callbacks.showStrongSelectionDetails;
        if (callback) {
            callback();
        }
    });
    updateButtonMetrics();
}

void SelectionContextSettingsCard::rebuildActionList()
{
    if (!m_actions) {
        return;
    }
    const bool previousUpdating = m_updating;
    m_updating = true;
    QStringList catalogIds;
    QMap<QString, QString> titles;
    for (const QPair<QString, QString> &option : m_actionCatalog) {
        catalogIds.append(option.second);
        titles.insert(option.second, option.first);
    }
    QStringList order;
    for (const QString &rawId : m_settings.actionOrder) {
        const QString id = rawId.trimmed();
        if (catalogIds.contains(id) && !order.contains(id)) {
            order.append(id);
        }
    }
    for (const QString &id : catalogIds) {
        if (!order.contains(id)) {
            order.append(id);
        }
    }
    m_actions->clear();
    for (const QString &id : order) {
        QListWidgetItem *item = new QListWidgetItem(titles.value(id), m_actions);
        item->setData(Qt::UserRole, id);
        item->setFlags(item->flags() | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
    }
    m_settings.actionOrder = order;
    m_updating = previousUpdating;
}

void SelectionContextSettingsCard::updateButtonMetrics()
{
    const QList<QPushButton *> buttons = findChildren<QPushButton *>();
    for (QPushButton *button : buttons) {
        button->setMinimumHeight(qMax(
            40,
            QFontMetrics(button->font()).height() + 16
        ));
        button->setMaximumHeight(QWIDGETSIZE_MAX);
    }
}

void SelectionContextSettingsCard::updateConsentResetVisibility()
{
    if (m_resetConsent) {
        m_resetConsent->setVisible(
            m_settings.networkConsentAcknowledged
        );
    }
}

void SelectionContextSettingsCard::readWidgets()
{
    m_settings.enabled = m_enabled->isChecked();
    m_settings.keyboardSelectionEnabled = m_keyboard->isChecked();
    m_settings.minimumTextLength = m_minimumLength->value();
    m_settings.closeOnOutsideClick = m_closeOutside->isChecked();
    m_settings.pinEnabled = m_pin->isChecked();
    m_settings.pauseMinutes = m_pauseMinutes->value();
    QStringList actionOrder;
    for (int i = 0; i < m_actions->count(); ++i) {
        const QString id = m_actions->item(i)
            ->data(Qt::UserRole)
            .toString()
            .trimmed();
        if (!id.isEmpty() && !actionOrder.contains(id)) {
            actionOrder.append(id);
        }
    }
    m_settings.actionOrder = actionOrder;
    m_settings.blockedApplications = normalizeExecutables(
        m_blockedApplications->toPlainText().split(
            QRegExp(QStringLiteral("[\\r\\n]+")),
            QString::SkipEmptyParts
        )
    );
}

void SelectionContextSettingsCard::notifyChanged()
{
    if (m_updating) {
        return;
    }
    readWidgets();
    if (m_callbacks.settingsChanged) {
        m_callbacks.settingsChanged(m_settings);
    }
}

void SelectionContextSettingsCard::queueActionOrderChanged()
{
    if (m_updating || m_actionChangeQueued) {
        return;
    }
    m_actionChangeQueued = true;
    QTimer::singleShot(0, this, [this]() {
        m_actionChangeQueued = false;
        notifyChanged();
    });
}
