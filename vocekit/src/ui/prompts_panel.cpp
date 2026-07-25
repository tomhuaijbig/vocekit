#include "prompts_panel.h"

#include "attention_message.h"
#include "history_row_frame.h"
#include "ui_style.h"

#include <QtWidgets>

namespace {

QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

void clearLayout(QLayout *layout)
{
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        if (item->layout()) {
            clearLayout(item->layout());
        }
        delete item;
    }
}

} // namespace

PromptsPanel::PromptsPanel(
    const PromptsPanelAccess &access,
    const std::function<void()> &settingsChanged,
    QWidget *parent
)
    : QWidget(parent), m_access(access), m_settingsChanged(settingsChanged)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(16);

    auto *top = new QHBoxLayout;
    auto *title = new QLabel(tr8("提示词库"));
    title->setFont(appFont(24, QFont::DemiBold));
    top->addWidget(title, 1);

    m_lock = new QCheckBox(tr8("锁定提示词"));
    m_lock->setChecked(promptLocked());
    m_lock->setFont(appFont(10, QFont::DemiBold));
    connect(m_lock, &QCheckBox::toggled, this, [this](bool locked) {
        if (!m_access.setPromptLocked) {
            return;
        }
        QString error;
        if (!m_access.setPromptLocked(locked, &error)) {
            m_lock->blockSignals(true);
            m_lock->setChecked(!locked);
            m_lock->blockSignals(false);
            showAttentionWarning(
                this,
                tr8("保存失败"),
                error.isEmpty() ? tr8("无法保存提示词锁定状态。") : error
            );
            return;
        }
        updatePromptEditorLock();
        notifySettingsChanged();
    });

    auto *add = new QPushButton(tr8("新增提示词"));
    add->setFixedHeight(38);
    add->setStyleSheet(buttonStyle(QStringLiteral("#111827")));
    connect(add, &QPushButton::clicked, this, [this]() { addPromptLibraryItemFromUi(); });

    m_duplicateButton = new QPushButton(tr8("复制当前"));
    m_duplicateButton->setFixedHeight(38);
    m_duplicateButton->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
    connect(m_duplicateButton, &QPushButton::clicked, this, [this]() { duplicateCurrentPrompt(); });

    m_deleteButton = new QPushButton(tr8("删除"));
    m_deleteButton->setFixedHeight(38);
    m_deleteButton->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#b42318")));
    connect(m_deleteButton, &QPushButton::clicked, this, [this]() { deleteCurrentPrompt(); });

    m_saveButton = new QPushButton(tr8("保存提示词"));
    m_saveButton->setFixedHeight(38);
    m_saveButton->setStyleSheet(buttonStyle(QStringLiteral("#111827")));
    connect(m_saveButton, &QPushButton::clicked, this, [this]() { savePromptFromEditor(); });

    top->addWidget(m_lock);
    top->addWidget(add);
    top->addWidget(m_duplicateButton);
    top->addWidget(m_deleteButton);
    top->addWidget(m_saveButton);
    layout->addLayout(top);

    auto *body = new QHBoxLayout;
    body->setSpacing(14);

    auto *listCard = new QFrame;
    listCard->setObjectName(QStringLiteral("card"));
    listCard->setStyleSheet(cardStyle());
    listCard->setMinimumWidth(310);
    listCard->setMaximumWidth(380);
    auto *listLayout = new QVBoxLayout(listCard);
    listLayout->setContentsMargins(14, 14, 14, 14);
    listLayout->setSpacing(10);

    m_searchEdit = new QLineEdit;
    m_searchEdit->setMinimumHeight(38);
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setPlaceholderText(tr8("搜索提示词"));
    m_searchEdit->setStyleSheet(QStringLiteral(
        "QLineEdit { background: #ffffff; border: 1px solid #d0d5dd; border-radius: 8px; padding: 0 12px; }"
    ));
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this]() {
        refresh();
    });
    listLayout->addWidget(m_searchEdit);

    auto *listScroll = new QScrollArea;
    listScroll->setWidgetResizable(true);
    listScroll->setFrameShape(QFrame::NoFrame);
    listScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *listHolder = new QWidget;
    m_listLayout = new QVBoxLayout(listHolder);
    m_listLayout->setContentsMargins(0, 0, 4, 0);
    m_listLayout->setSpacing(8);
    listScroll->setWidget(listHolder);
    listLayout->addWidget(listScroll, 1);

    auto *editorCard = new QFrame;
    editorCard->setObjectName(QStringLiteral("card"));
    editorCard->setStyleSheet(cardStyle());
    auto *editorLayout = new QVBoxLayout(editorCard);
    editorLayout->setContentsMargins(18, 16, 18, 16);
    editorLayout->setSpacing(12);

    auto *form = new QGridLayout;
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(10);
    m_typeLabel = new QLabel;
    m_typeLabel->setMinimumHeight(34);
    m_typeLabel->setAlignment(Qt::AlignCenter);
    m_typeLabel->setStyleSheet(QStringLiteral(
        "QLabel { background: #eef2ff; color: #1d4ed8; border-radius: 6px; font-weight: 600; padding: 6px 10px; }"
    ));
    m_nameEdit = new QLineEdit;
    m_nameEdit->setMinimumHeight(36);
    m_nameEdit->setStyleSheet(QStringLiteral(
        "QLineEdit { background: #ffffff; border: 1px solid #d0d5dd; border-radius: 6px; padding: 6px 10px; }"
    ));
    m_scopeBox = new QComboBox;
    m_scopeBox->setMinimumHeight(36);
    const QStringList scopes = promptScopeOptions();
    for (const QString &scope : scopes) {
        m_scopeBox->addItem(scope, scope);
    }
    m_scopeBox->setStyleSheet(QStringLiteral(
        "QComboBox { background: #ffffff; border: 1px solid #d0d5dd; border-radius: 6px; padding: 6px 10px; }"
    ));
    form->addWidget(m_typeLabel, 0, 0);
    form->addWidget(new QLabel(tr8("名称")), 0, 1);
    form->addWidget(m_nameEdit, 0, 2);
    form->addWidget(new QLabel(tr8("范围")), 0, 3);
    form->addWidget(m_scopeBox, 0, 4);
    form->setColumnStretch(2, 1);
    editorLayout->addLayout(form);

    m_editor = new QTextEdit;
    m_editor->setMinimumHeight(420);
    m_editor->setStyleSheet(QStringLiteral(
        "QTextEdit { background: #ffffff; border: 1px solid #dde2ea; border-radius: 8px; padding: 12px; }"
    ));
    editorLayout->addWidget(m_editor, 1);

    body->addWidget(listCard);
    body->addWidget(editorCard, 1);
    layout->addLayout(body, 1);

    refresh();
}

PromptRuntimeSnapshot PromptsPanel::promptSnapshot() const
{
    return m_access.prompts.snapshotProvider
        ? m_access.prompts.snapshotProvider()
        : PromptRuntimeSnapshot();
}

bool PromptsPanel::promptLocked() const
{
    return promptSnapshot().settings.promptLocked;
}

QStringList PromptsPanel::promptScopeOptions() const
{
    return QStringList()
        << tr8("通用")
        << tr8("听写")
        << tr8("翻译")
        << tr8("问答")
        << tr8("自定义");
}

void PromptsPanel::setPromptScopeCurrent(const QString &scope)
{
    if (!m_scopeBox) {
        return;
    }
    const QString value = scope.trimmed().isEmpty() ? tr8("通用") : scope.trimmed();
    int index = m_scopeBox->findData(value);
    if (index < 0) {
        m_scopeBox->addItem(value, value);
        index = m_scopeBox->findData(value);
    }
    m_scopeBox->setCurrentIndex(qMax(0, index));
}

bool PromptsPanel::promptMatchesSearch(const PromptTargetInfo &target, const QString &keyword) const
{
    const QString trimmed = keyword.trimmed();
    if (trimmed.isEmpty()) {
        return true;
    }
    return target.title.contains(trimmed, Qt::CaseInsensitive)
        || target.scope.contains(trimmed, Qt::CaseInsensitive)
        || sharedPromptText(m_access.prompts, target).contains(trimmed, Qt::CaseInsensitive);
}

QWidget *PromptsPanel::promptListCard(const PromptTargetInfo &target)
{
    auto *frame = new HistoryRowFrame;
    frame->setObjectName(QStringLiteral("card"));
    frame->setStyleSheet(cardStyle() + (target.id == m_currentPromptId
        ? QStringLiteral("QFrame#card { border: 2px solid #2563eb; background: #f8fbff; }")
        : QString()));
    frame->setClickCallback([this, target]() {
        selectPromptTarget(target.id);
    });

    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(8);

    auto *top = new QHBoxLayout;
    auto *name = new QLabel(target.title);
    name->setFont(appFont(11, QFont::DemiBold));
    name->setWordWrap(true);
    auto *type = new QLabel(target.library ? tr8("自定义") : (target.custom ? tr8("功能") : tr8("内置")));
    type->setAlignment(Qt::AlignCenter);
    type->setMinimumSize(58, 26);
    type->setStyleSheet(QStringLiteral(
        "QLabel { background: #eef2ff; color: #1d4ed8; border-radius: 5px; font-weight: 600; }"
    ));
    top->addWidget(name, 1);
    top->addWidget(type);
    layout->addLayout(top);

    auto *scope = new QLabel(target.scope.trimmed().isEmpty() ? tr8("通用") : target.scope.trimmed());
    scope->setStyleSheet(QStringLiteral("color: #047857; font-weight: 600;"));
    layout->addWidget(scope);
    return frame;
}

void PromptsPanel::refresh()
{
    if (!m_listLayout) {
        return;
    }

    clearLayout(m_listLayout);

    const QVector<PromptTargetInfo> targets = sharedPromptTargets(
        m_access.prompts
    );
    if (m_currentPromptId.trimmed().isEmpty() && !targets.isEmpty()) {
        m_currentPromptId = targets.first().id;
    }

    bool currentStillExists = false;
    const QString keyword = m_searchEdit ? m_searchEdit->text() : QString();
    for (const PromptTargetInfo &target : targets) {
        if (target.id == m_currentPromptId) {
            currentStillExists = true;
        }
        if (promptMatchesSearch(target, keyword)) {
            m_listLayout->addWidget(promptListCard(target));
        }
    }
    if (!currentStillExists && !targets.isEmpty()) {
        m_currentPromptId = targets.first().id;
    }
    m_listLayout->addStretch();
    loadPromptEditor();
}

void PromptsPanel::selectPromptTarget(const QString &id)
{
    if (m_currentPromptId == id) {
        loadPromptEditor();
        return;
    }
    m_currentPromptId = id;
    refresh();
}

void PromptsPanel::loadPromptEditor()
{
    if (!m_editor) {
        return;
    }
    const PromptTargetInfo target = sharedPromptTargetForId(
        m_access.prompts,
        m_currentPromptId
    );
    if (m_nameEdit) {
        m_nameEdit->setText(target.title);
        m_nameEdit->setReadOnly(!target.library);
    }
    if (m_typeLabel) {
        m_typeLabel->setText(target.library ? tr8("自定义") : (target.custom ? tr8("功能") : tr8("内置")));
    }
    setPromptScopeCurrent(target.scope);
    if (m_scopeBox) {
        m_scopeBox->setEnabled(target.library);
    }
    m_editor->setPlainText(sharedPromptText(m_access.prompts, target));
    updatePromptEditorLock();
}

void PromptsPanel::updatePromptEditorLock()
{
    const bool locked = promptLocked();
    if (m_lock && m_lock->isChecked() != locked) {
        m_lock->blockSignals(true);
        m_lock->setChecked(locked);
        m_lock->blockSignals(false);
    }
    if (m_editor) {
        m_editor->setDisabled(locked);
    }
    if (m_saveButton) {
        m_saveButton->setDisabled(locked);
    }

    const PromptTargetInfo target = sharedPromptTargetForId(
        m_access.prompts,
        m_currentPromptId
    );
    if (m_nameEdit) {
        m_nameEdit->setDisabled(locked || !target.library);
    }
    if (m_scopeBox) {
        m_scopeBox->setDisabled(locked || !target.library);
    }
    if (m_deleteButton) {
        m_deleteButton->setEnabled(!locked && target.library);
    }
    if (m_duplicateButton) {
        m_duplicateButton->setEnabled(!locked && !m_currentPromptId.trimmed().isEmpty());
    }
}

void PromptsPanel::savePromptFromEditor()
{
    if (!m_editor) {
        return;
    }
    if (promptLocked()) {
        showAttentionInformation(this, tr8("提示词已锁定"), tr8("请先取消锁定后再修改提示词。"));
        return;
    }

    const PromptTargetInfo target = sharedPromptTargetForId(
        m_access.prompts,
        m_currentPromptId
    );
    if (target.library) {
        PromptLibraryItem item;
        const PromptRuntimeSnapshot snapshot = promptSnapshot();
        for (const PromptLibraryItem &existing : snapshot.libraryItems) {
            if (existing.id == target.id) {
                item = existing;
                break;
            }
        }
        item.name = m_nameEdit ? m_nameEdit->text().trimmed() : item.name;
        item.scope = m_scopeBox ? m_scopeBox->currentData().toString().trimmed() : item.scope;
        item.content = m_editor->toPlainText();
        if (item.name.isEmpty()) {
            showAttentionWarning(this, tr8("名称不能为空"), tr8("请填写提示词名称。"));
            return;
        }
        QString error;
        if (!m_access.saveLibraryPromptItem
            || !m_access.saveLibraryPromptItem(item, &error)) {
            showAttentionWarning(
                this,
                tr8("保存失败"),
                error.isEmpty() ? tr8("无法写入 config/prompts.json。") : error
            );
            return;
        }
    } else {
        QString error;
        if (!saveSharedPromptText(
            m_access.prompts,
            target,
            m_editor->toPlainText(),
            &error)) {
            showAttentionWarning(this, tr8("保存失败"), error.isEmpty() ? tr8("无法保存提示词。") : error);
            return;
        }
    }

    notifySettingsChanged();
    showAttentionInformation(this, tr8("已保存"), tr8("提示词已保存。"));
}

void PromptsPanel::addPromptLibraryItemFromUi()
{
    if (promptLocked()) {
        showAttentionInformation(this, tr8("提示词已锁定"), tr8("请先取消锁定后再新增提示词。"));
        return;
    }
    PromptLibraryItem item;
    item.name = tr8("新提示词");
    item.scope = tr8("通用");
    item.content = tr8("请在这里编写提示词。");
    QString error;
    if (!m_access.createLibraryPromptItem
        || !m_access.createLibraryPromptItem(&item, &error)) {
        showAttentionWarning(
            this,
            tr8("保存失败"),
            error.isEmpty() ? tr8("无法写入 config/prompts.json。") : error
        );
        return;
    }
    m_currentPromptId = item.id;
    notifySettingsChanged();
}

void PromptsPanel::duplicateCurrentPrompt()
{
    if (m_currentPromptId.trimmed().isEmpty()) {
        return;
    }
    if (promptLocked()) {
        showAttentionInformation(this, tr8("提示词已锁定"), tr8("请先取消锁定后再复制提示词。"));
        return;
    }
    const PromptTargetInfo target = sharedPromptTargetForId(
        m_access.prompts,
        m_currentPromptId
    );
    PromptLibraryItem item;
    item.name = target.title + tr8(" 副本");
    item.scope = target.scope.trimmed().isEmpty() ? tr8("通用") : target.scope.trimmed();
    item.content = sharedPromptText(m_access.prompts, target);
    QString error;
    if (!m_access.createLibraryPromptItem
        || !m_access.createLibraryPromptItem(&item, &error)) {
        showAttentionWarning(
            this,
            tr8("保存失败"),
            error.isEmpty() ? tr8("无法写入 config/prompts.json。") : error
        );
        return;
    }
    m_currentPromptId = item.id;
    notifySettingsChanged();
}

void PromptsPanel::deleteCurrentPrompt()
{
    if (promptLocked()) {
        showAttentionInformation(this, tr8("提示词已锁定"), tr8("请先取消锁定后再删除提示词。"));
        return;
    }
    const PromptTargetInfo target = sharedPromptTargetForId(
        m_access.prompts,
        m_currentPromptId
    );
    if (!target.library) {
        showAttentionInformation(this, tr8("不能删除"), tr8("内置提示词和功能同名提示词不能删除，可以复制成独立提示词后再修改。"));
        return;
    }
    if (QMessageBox::question(this, tr8("删除提示词"), tr8("确定删除“") + target.title + tr8("”吗？使用它的功能会切回默认提示词。")) != QMessageBox::Yes) {
        return;
    }
    QString error;
    if (!m_access.deleteLibraryPromptItem
        || !m_access.deleteLibraryPromptItem(target.id, &error)) {
        showAttentionWarning(
            this,
            tr8("删除失败"),
            error.isEmpty() ? tr8("无法保存提示词删除结果。") : error
        );
        return;
    }
    m_currentPromptId.clear();
    notifySettingsChanged();
}

void PromptsPanel::notifySettingsChanged()
{
    if (m_settingsChanged) {
        m_settingsChanged();
    } else {
        refresh();
    }
}
