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

QVector<QPair<QString, QString>> normalizedCatalog(
    const QVector<QPair<QString, QString>> &source,
    bool allowEmptyId,
    bool rejectAllScope)
{
    QVector<QPair<QString, QString>> result;
    QStringList ids;
    for (const QPair<QString, QString> &option : source) {
        const QString id = option.second.trimmed();
        if ((!allowEmptyId && id.isEmpty())
            || (rejectAllScope && id == QStringLiteral("__all"))
            || ids.contains(id)) {
            continue;
        }
        ids.append(id);
        const QString title = option.first.trimmed();
        result.append(qMakePair(title.isEmpty() ? id : title, id));
    }
    return result;
}

bool sameCustomization(
    const SelectionContextActionCustomization &left,
    const SelectionContextActionCustomization &right)
{
    return left.displayName == right.displayName
        && left.visible == right.visible
        && left.modelId == right.modelId
        && left.promptOverride == right.promptOverride
        && left.targetLanguage == right.targetLanguage
        && left.vocabularyScopeId == right.vocabularyScopeId
        && left.copyMode == right.copyMode;
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
      m_callbacks(callbacks)
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
    return snapshotFromWidgets();
}

void SelectionContextSettingsCard::setSettings(
    const SelectionContextSettings &settings)
{
    m_updating = true;
    m_settings = settings;
    m_settings.actionOrder = normalizeSelectionContextActionOrder(
        m_settings.actionOrder);
    SelectionContextActionNormalizationContext context;
    context.actionOrder = m_settings.actionOrder;
    m_settings.actionCustomizations =
        normalizeSelectionContextActionCustomizations(
            m_settings.actionCustomizations,
            context
        );
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
        bool canRefreshEditors = m_actions
            && m_actions->count() == m_settings.actionOrder.size();
        for (int row = 0; canRefreshEditors
             && row < m_actions->count(); ++row) {
            const QString id = m_settings.actionOrder.at(row);
            SelectionContextActionEditor *editor =
                m_actionEditors.value(id, nullptr);
            canRefreshEditors = editor
                && m_actions->item(row)->data(Qt::UserRole).toString() == id
                && m_actions->itemWidget(m_actions->item(row)) == editor;
        }
        if (!canRefreshEditors) {
            rebuildActionEditors();
        } else {
            for (int row = 0; row < m_actions->count(); ++row) {
                QListWidgetItem *item = m_actions->item(row);
                const QString id = item->data(Qt::UserRole).toString();
                SelectionContextActionEditor *editor =
                    m_actionEditors.value(id, nullptr);
                const SelectionContextActionCustomization incoming =
                    m_settings.actionCustomizations.value(id);
                if (!sameCustomization(editor->customization(), incoming)) {
                    editor->setCustomization(incoming);
                }
                updateItemPresentation(id);
            }
            setExpandedAction(m_expandedActionId);
            updateButtonMetrics();
            updateActionListMetrics();
        }
        updateConsentResetVisibility();
    }
    m_updating = false;
}

void SelectionContextSettingsCard::setCatalogs(
    const SelectionContextActionEditor::Catalogs &catalogs)
{
    SelectionContextActionEditor::Catalogs accepted;
    accepted.models = normalizedCatalog(catalogs.models, false, false);
    accepted.vocabularyScopes = normalizedCatalog(
        catalogs.vocabularyScopes, false, true);
    accepted.targetLanguages = normalizedCatalog(
        catalogs.targetLanguages, true, false);
    if (accepted.models == m_catalogs.models
        && accepted.vocabularyScopes == m_catalogs.vocabularyScopes
        && accepted.targetLanguages == m_catalogs.targetLanguages) {
        return;
    }
    if (m_actions) {
        m_settings = snapshotFromWidgets();
    }
    m_catalogs = accepted;
    rebuildActionEditors();
}

void SelectionContextSettingsCard::changeEvent(QEvent *event)
{
    QFrame::changeEvent(event);
    if (event && event->type() == QEvent::FontChange) {
        updateButtonMetrics();
        updateActionListMetrics();
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

    QLabel *actionTitle = new QLabel(sccTr8("工具条功能"), this);
    QFont sectionFont = actionTitle->font();
    sectionFont.setWeight(QFont::DemiBold);
    actionTitle->setFont(sectionFont);
    actionTitle->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    actionTitle->setMinimumHeight(actionTitle->sizeHint().height());
    layout->addWidget(actionTitle);
    layout->addWidget(descriptionLabel(
        sccTr8("拖动调整顺序；每项都可修改名称、显示状态和专属设置。自定义功能会继续显示在“更多”菜单中。"),
        this
    ));
    m_actions = new QListWidget(this);
    m_actions->setObjectName(QStringLiteral("selectionContextActionList"));
    m_actions->setDragDropMode(QAbstractItemView::InternalMove);
    m_actions->setDefaultDropAction(Qt::MoveAction);
    m_actions->setSelectionMode(QAbstractItemView::SingleSelection);
    m_actions->setSpacing(6);
    m_actions->setMinimumHeight(340);
    m_actions->setMaximumHeight(QWIDGETSIZE_MAX);
    layout->addWidget(m_actions);
    m_restoreAllActions = new QPushButton(
        sccTr8("恢复全部默认设置"), this);
    m_restoreAllActions->setObjectName(
        QStringLiteral("selectionContextRestoreAllButton"));
    layout->addWidget(m_restoreAllActions, 0, Qt::AlignLeft);

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
    connect(m_restoreAllActions, &QPushButton::clicked, this, [this]() {
        restoreAllActionDefaults();
    });
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

void SelectionContextSettingsCard::rebuildActionEditors()
{
    if (!m_actions) {
        return;
    }
    const bool previousUpdating = m_updating;
    m_updating = true;
    const QStringList order = normalizeSelectionContextActionOrder(
        m_settings.actionOrder);
    m_settings.actionOrder = order;
    while (m_actions->count() > 0) {
        QListWidgetItem *item = m_actions->item(0);
        QWidget *widget = m_actions->itemWidget(item);
        if (widget) {
            m_actions->removeItemWidget(item);
            delete widget;
        }
        item = m_actions->takeItem(0);
        delete item;
    }
    m_actionEditors.clear();
    for (const QString &id : order) {
        QListWidgetItem *item = new QListWidgetItem(m_actions);
        item->setData(Qt::UserRole, id);
        item->setFlags(item->flags() | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
        SelectionContextActionEditor::Callbacks callbacks;
        callbacks.changed = [this, id](
            const SelectionContextActionCustomization &value) {
            applyCustomization(id, value);
        };
        callbacks.restoreRequested = [this, id]() {
            restoreActionDefaults(id);
        };
        callbacks.validationWarning = [this](const QString &warning) {
            const std::function<void(const QString &)> callback =
                m_callbacks.validationWarning;
            if (callback) {
                callback(warning);
            }
        };
        SelectionContextActionEditor *editor =
            new SelectionContextActionEditor(
                id, m_catalogs, callbacks, m_actions);
        editor->setObjectName(
            QStringLiteral("selectionActionEditor_") + id);
        editor->setCustomization(
            m_settings.actionCustomizations.value(id));
        m_actionEditors.insert(id, editor);
        m_actions->setItemWidget(item, editor);
        updateItemPresentation(id);
        QToolButton *expand = editor->findChild<QToolButton *>(
            QStringLiteral("selectionActionExpand"));
        if (expand) {
            connect(expand, &QToolButton::clicked, this, [this, id]() {
                SelectionContextActionEditor *current =
                    m_actionEditors.value(id, nullptr);
                setExpandedAction(
                    current && current->isExpanded() ? id : QString());
            });
        }
        QCheckBox *visible = editor->findChild<QCheckBox *>(
            QStringLiteral("selectionActionVisible"));
        if (visible) {
            connect(visible, &QCheckBox::toggled, this,
                    [this, id, editor](bool checked) {
                if (m_updating || checked) {
                    return;
                }
                const SelectionContextActionCustomization candidate =
                    editor->customization();
                if (!wouldHideLastVisibleAction(id, candidate)) {
                    return;
                }
                SelectionContextActionCustomization restored = candidate;
                restored.visible = true;
                editor->setCustomization(restored);
                warnLastVisibleAction();
            });
        }
    }
    setExpandedAction(m_expandedActionId);
    updateButtonMetrics();
    updateActionListMetrics();
    m_updating = previousUpdating;
}

void SelectionContextSettingsCard::setExpandedAction(const QString &actionId)
{
    const QString acceptedId = m_actionEditors.contains(actionId)
        ? actionId
        : QString();
    m_expandedActionId = acceptedId;
    for (auto it = m_actionEditors.begin();
         it != m_actionEditors.end(); ++it) {
        if (it.value()) {
            it.value()->setExpanded(it.key() == acceptedId);
        }
    }
    updateActionListMetrics();
}

void SelectionContextSettingsCard::restoreActionDefaults(
    const QString &actionId)
{
    if (!m_actionEditors.contains(actionId)) {
        return;
    }
    SelectionContextSettings next = snapshotFromWidgets();
    const SelectionContextActionCustomization value =
        defaultSelectionContextActionCustomizations().value(actionId);
    next.actionCustomizations.insert(actionId, value);
    m_settings = next;
    m_actionEditors.value(actionId)->setCustomization(value);
    updateItemPresentation(actionId);
    const std::function<void(const SelectionContextSettings &)> callback =
        m_callbacks.settingsChanged;
    if (callback) {
        callback(m_settings);
    }
}

void SelectionContextSettingsCard::restoreAllActionDefaults()
{
    const std::function<bool()> confirm =
        m_callbacks.confirmRestoreAllSelectionActions;
    const QPointer<SelectionContextSettingsCard> alive(this);
    if (!confirm || !confirm() || !alive) {
        return;
    }
    m_settings = snapshotFromWidgets();
    m_settings.actionCustomizations =
        defaultSelectionContextActionCustomizations();
    const bool previousUpdating = m_updating;
    m_updating = true;
    for (const QString &id : defaultSelectionContextActionOrder()) {
        SelectionContextActionEditor *editor =
            m_actionEditors.value(id, nullptr);
        if (editor) {
            editor->setCustomization(
                m_settings.actionCustomizations.value(id));
            updateItemPresentation(id);
        }
    }
    m_updating = previousUpdating;
    const std::function<void(const SelectionContextSettings &)> callback =
        m_callbacks.settingsChanged;
    if (callback) {
        callback(m_settings);
    }
}

bool SelectionContextSettingsCard::applyCustomization(
    const QString &actionId,
    const SelectionContextActionCustomization &value)
{
    if (!m_actionEditors.contains(actionId)) {
        return false;
    }
    if (wouldHideLastVisibleAction(actionId, value)) {
        SelectionContextActionCustomization restored = value;
        restored.visible = true;
        m_actionEditors.value(actionId)->setCustomization(restored);
        warnLastVisibleAction();
        return false;
    }

    SelectionContextSettings next = snapshotFromWidgets();
    SelectionContextActionCustomizationMap values =
        next.actionCustomizations;
    values.insert(actionId, value);
    SelectionContextActionNormalizationContext context;
    context.actionOrder = next.actionOrder;
    for (const QPair<QString, QString> &scope : m_catalogs.vocabularyScopes) {
        context.writableVocabularyScopeIds.append(scope.second);
    }
    const SelectionContextActionCustomizationMap normalized =
        normalizeSelectionContextActionCustomizations(values, context);
    const SelectionContextActionCustomization accepted =
        normalized.value(actionId);
    next.actionOrder = context.actionOrder;
    next.actionCustomizations = normalized;
    m_settings = next;
    SelectionContextActionEditor *editor = m_actionEditors.value(actionId);
    if (editor && !sameCustomization(value, accepted)) {
        editor->setCustomization(accepted);
    }
    updateItemPresentation(actionId);
    const std::function<void(const SelectionContextSettings &)> callback =
        m_callbacks.settingsChanged;
    if (callback) {
        callback(m_settings);
    }
    return true;
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

void SelectionContextSettingsCard::updateActionListMetrics()
{
    if (!m_actions) {
        return;
    }
    int totalHeight = 2 * m_actions->frameWidth();
    for (int row = 0; row < m_actions->count(); ++row) {
        QListWidgetItem *item = m_actions->item(row);
        SelectionContextActionEditor *editor =
            m_actionEditors.value(
                item->data(Qt::UserRole).toString(), nullptr);
        if (!editor) {
            continue;
        }
        const QSize size = editor->sizeHint();
        item->setSizeHint(size);
        totalHeight += size.height() + m_actions->spacing();
    }
    m_actions->setMinimumHeight(qMin(totalHeight + 8, 720));
    m_actions->doItemsLayout();
    m_actions->updateGeometry();
}

void SelectionContextSettingsCard::updateItemPresentation(
    const QString &actionId)
{
    if (!m_actions) {
        return;
    }
    SelectionContextActionEditor *editor =
        m_actionEditors.value(actionId, nullptr);
    if (!editor) {
        return;
    }
    for (int row = 0; row < m_actions->count(); ++row) {
        QListWidgetItem *item = m_actions->item(row);
        if (item->data(Qt::UserRole).toString() != actionId) {
            continue;
        }
        const QString displayName = editor->customization().displayName;
        // The row is painted by its item widget. Keeping DisplayRole text
        // would let the list delegate paint a second label underneath the
        // transparent editor and visibly overlap its fields.
        item->setData(Qt::DisplayRole, QString());
        item->setData(Qt::AccessibleTextRole, displayName);
        item->setSizeHint(editor->sizeHint());
        return;
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
    m_settings = snapshotFromWidgets();
}

SelectionContextSettings
SelectionContextSettingsCard::snapshotFromWidgets() const
{
    SelectionContextSettings result = m_settings;
    if (!m_enabled) {
        return result;
    }
    result.enabled = m_enabled->isChecked();
    result.keyboardSelectionEnabled = m_keyboard->isChecked();
    result.minimumTextLength = m_minimumLength->value();
    result.closeOnOutsideClick = m_closeOutside->isChecked();
    result.pinEnabled = m_pin->isChecked();
    result.pauseMinutes = m_pauseMinutes->value();
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
    result.actionOrder = normalizeSelectionContextActionOrder(actionOrder);
    for (const QString &id : defaultSelectionContextActionOrder()) {
        SelectionContextActionEditor *editor =
            m_actionEditors.value(id, nullptr);
        if (editor) {
            result.actionCustomizations.insert(id, editor->customization());
        }
    }
    result.blockedApplications = normalizeExecutables(
        m_blockedApplications->toPlainText().split(
            QRegExp(QStringLiteral("[\\r\\n]+")),
            QString::SkipEmptyParts
        )
    );
    return result;
}

bool SelectionContextSettingsCard::wouldHideLastVisibleAction(
    const QString &actionId,
    const SelectionContextActionCustomization &value) const
{
    if (value.visible) {
        return false;
    }
    for (const QString &id : defaultSelectionContextActionOrder()) {
        if (id == actionId) {
            continue;
        }
        SelectionContextActionEditor *editor =
            m_actionEditors.value(id, nullptr);
        const SelectionContextActionCustomization other = editor
            ? editor->customization()
            : m_settings.actionCustomizations.value(id);
        if (other.visible) {
            return false;
        }
    }
    return true;
}

void SelectionContextSettingsCard::warnLastVisibleAction()
{
    const std::function<void(const QString &)> warning =
        m_callbacks.validationWarning;
    if (warning) {
        warning(sccTr8("请至少保留一个工具条功能；如不需要工具条，请关闭上方总开关。"));
    }
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
        if (m_updating) {
            return;
        }
        readWidgets();
        rebuildActionEditors();
        const std::function<void(const SelectionContextSettings &)> callback =
            m_callbacks.settingsChanged;
        if (callback) {
            callback(m_settings);
        }
    });
}
