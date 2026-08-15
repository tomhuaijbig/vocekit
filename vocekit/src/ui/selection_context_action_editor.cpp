#include "selection_context_action_editor.h"

#include "../domain/selection_context_actions.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QTextCursor>
#include <QTextDocument>
#include <QToolButton>
#include <QTimer>
#include <QVBoxLayout>

namespace {

const int kPromptLimit = 8000;
const int kInjectedUnavailableRole = Qt::UserRole + 1;

QString text8(const char *value)
{
    return QString::fromUtf8(value);
}

void addCatalogItems(
    QComboBox *combo,
    const QVector<QPair<QString, QString>> &items
)
{
    for (const QPair<QString, QString> &item : items) {
        if (combo->findData(item.second) >= 0) {
            continue;
        }
        combo->addItem(item.first, item.second);
    }
}

void removeInjectedUnavailableItems(QComboBox *combo)
{
    for (int index = combo->count() - 1; index >= 0; --index) {
        if (combo->itemData(index, kInjectedUnavailableRole).toBool()) {
            combo->removeItem(index);
        }
    }
}

void prepareEditor(QWidget *widget)
{
    widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    widget->setMinimumHeight(widget->sizeHint().height());
}

void prepareCombo(QComboBox *combo)
{
    combo->setSizeAdjustPolicy(
        QComboBox::AdjustToMinimumContentsLengthWithIcon);
    combo->setMinimumContentsLength(24);
    prepareEditor(combo);
    QObject::connect(combo, &QComboBox::currentTextChanged, combo,
                     [combo](const QString &text) {
        combo->setToolTip(text);
    });
    combo->setToolTip(combo->currentText());
}

void prepareButton(QAbstractButton *button)
{
    button->setMaximumHeight(QWIDGETSIZE_MAX);
    button->setMinimumHeight(qMax(34, button->sizeHint().height()));
    button->setStyleSheet(QStringLiteral("padding: 0 10px;"));
}

void restoreTextAsUndoableEdit(
    QPlainTextEdit *editor,
    const QString &previous
)
{
    const QString current = editor->toPlainText();
    int prefix = 0;
    while (prefix < current.size()
           && prefix < previous.size()
           && current.at(prefix) == previous.at(prefix)) {
        ++prefix;
    }
    int suffix = 0;
    while (suffix < current.size() - prefix
           && suffix < previous.size() - prefix
           && current.at(current.size() - suffix - 1)
               == previous.at(previous.size() - suffix - 1)) {
        ++suffix;
    }

    QTextCursor visibleCursor(editor->document());
    visibleCursor.setPosition(prefix);
    editor->setTextCursor(visibleCursor);

    QTextCursor correction(editor->document());
    correction.beginEditBlock();
    correction.setPosition(prefix);
    correction.setPosition(current.size() - suffix, QTextCursor::KeepAnchor);
    correction.insertText(previous.mid(
        prefix,
        previous.size() - prefix - suffix
    ));
    correction.endEditBlock();
}

QLabel *fieldLabel(const QString &text, QWidget *parent)
{
    QLabel *label = new QLabel(text, parent);
    label->setWordWrap(true);
    label->setMinimumHeight(label->sizeHint().height());
    return label;
}

bool sameCustomization(
    const SelectionContextActionCustomization &left,
    const SelectionContextActionCustomization &right
)
{
    return left.displayName == right.displayName
        && left.visible == right.visible
        && left.modelId == right.modelId
        && left.promptOverride == right.promptOverride
        && left.targetLanguage == right.targetLanguage
        && left.vocabularyScopeId == right.vocabularyScopeId
        && left.copyMode == right.copyMode;
}

struct ChangeDeliveryFrame
{
    ChangeDeliveryFrame(
        SelectionContextActionEditor *owner,
        const SelectionContextActionCustomization &deliveredValue
    );
    ~ChangeDeliveryFrame();

    QPointer<SelectionContextActionEditor> owner;
    const SelectionContextActionCustomization deliveredValue;
    const ChangeDeliveryFrame *previous;
};

thread_local const ChangeDeliveryFrame *activeChangeDelivery = nullptr;

ChangeDeliveryFrame::ChangeDeliveryFrame(
    SelectionContextActionEditor *frameOwner,
    const SelectionContextActionCustomization &frameValue
)
    : owner(frameOwner),
      deliveredValue(frameValue),
      previous(activeChangeDelivery)
{
    activeChangeDelivery = this;
}

ChangeDeliveryFrame::~ChangeDeliveryFrame()
{
    activeChangeDelivery = previous;
}

bool isSaveEchoForActiveDelivery(
    SelectionContextActionEditor *owner,
    const SelectionContextActionCustomization &value
)
{
    for (const ChangeDeliveryFrame *frame = activeChangeDelivery;
         frame;
         frame = frame->previous) {
        if (frame->owner.data() == owner
            && sameCustomization(frame->deliveredValue, value)) {
            return true;
        }
    }
    return false;
}

void deliverChangedCallback(
    SelectionContextActionEditor *owner,
    const SelectionContextActionCustomization &value,
    const std::function<void(
        const SelectionContextActionCustomization &)> &callback
)
{
    ChangeDeliveryFrame delivery(owner, value);
    callback(value);
}

} // namespace

SelectionContextActionEditor::SelectionContextActionEditor(
    const QString &actionId,
    const Catalogs &catalogs,
    const Callbacks &callbacks,
    QWidget *parent
)
    : QFrame(parent),
      actionId_(actionId),
      callbacks_(callbacks)
{
    setObjectName(QStringLiteral("selectionContextActionEditor"));
    setFrameShape(QFrame::StyledPanel);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 10, 12, 10);
    root->setSpacing(10);

    QHBoxLayout *common = new QHBoxLayout;
    common->setSpacing(8);
    QLabel *nameLabel = fieldLabel(text8("显示名称"), this);
    nameLabel->setObjectName(
        QStringLiteral("selectionActionDisplayNameLabel"));
    common->addWidget(nameLabel);

    displayNameEdit_ = new QLineEdit(this);
    displayNameEdit_->setObjectName(QStringLiteral("selectionActionDisplayName"));
    prepareEditor(displayNameEdit_);
    nameLabel->setBuddy(displayNameEdit_);
    common->addWidget(displayNameEdit_, 1);

    visibleCheck_ = new QCheckBox(text8("显示"), this);
    visibleCheck_->setObjectName(QStringLiteral("selectionActionVisible"));
    prepareButton(visibleCheck_);
    common->addWidget(visibleCheck_);

    expandButton_ = new QToolButton(this);
    expandButton_->setObjectName(QStringLiteral("selectionActionExpand"));
    expandButton_->setCheckable(true);
    expandButton_->setAccessibleName(text8("展开或收起动作设置"));
    expandButton_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    prepareButton(expandButton_);
    common->addWidget(expandButton_);

    QPushButton *restoreButton = new QPushButton(
        text8("恢复此项默认设置"), this);
    restoreButton->setObjectName(QStringLiteral("selectionActionRestore"));
    prepareButton(restoreButton);
    common->addWidget(restoreButton);
    root->addLayout(common);

    specificFields_ = new QWidget(this);
    specificFields_->setObjectName(QStringLiteral("selectionActionSpecificFields"));
    QFormLayout *specific = new QFormLayout(specificFields_);
    specific->setContentsMargins(0, 4, 0, 0);
    specific->setHorizontalSpacing(10);
    specific->setVerticalSpacing(8);
    specific->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    const bool isAiAction = actionId_ == selectionContextActionAiSearch()
        || actionId_ == selectionContextActionTranslate()
        || actionId_ == selectionContextActionExplain();
    if (isAiAction) {
        modelCombo_ = new QComboBox(specificFields_);
        modelCombo_->setObjectName(QStringLiteral("selectionActionModel"));
        modelCombo_->setAccessibleName(text8("模型"));
        modelCombo_->addItem(text8("跟随对应内置功能"), QString());
        addCatalogItems(modelCombo_, catalogs.models);
        prepareCombo(modelCombo_);
        QLabel *modelLabel = fieldLabel(text8("模型"), specificFields_);
        modelLabel->setObjectName(QStringLiteral("selectionActionModelLabel"));
        modelLabel->setBuddy(modelCombo_);
        specific->addRow(modelLabel, modelCombo_);

        promptEdit_ = new QPlainTextEdit(specificFields_);
        promptEdit_->setObjectName(QStringLiteral("selectionActionPrompt"));
        promptEdit_->setAccessibleName(text8("提示词"));
        promptEdit_->setPlaceholderText(text8("留空时使用内置默认提示词"));
        prepareEditor(promptEdit_);
        promptCountLabel_ = new QLabel(specificFields_);
        promptCountLabel_->setObjectName(QStringLiteral("selectionActionPromptCount"));
        promptCountLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        promptCountLabel_->setMinimumHeight(promptCountLabel_->sizeHint().height());
        QWidget *promptContainer = new QWidget(specificFields_);
        QVBoxLayout *promptLayout = new QVBoxLayout(promptContainer);
        promptLayout->setContentsMargins(0, 0, 0, 0);
        promptLayout->setSpacing(4);
        promptLayout->addWidget(promptEdit_);
        promptLayout->addWidget(promptCountLabel_);
        QLabel *promptLabel = fieldLabel(text8("提示词"), specificFields_);
        promptLabel->setObjectName(QStringLiteral("selectionActionPromptLabel"));
        promptLabel->setBuddy(promptEdit_);
        specific->addRow(promptLabel, promptContainer);
    }

    if (actionId_ == selectionContextActionTranslate()) {
        targetLanguageCombo_ = new QComboBox(specificFields_);
        targetLanguageCombo_->setObjectName(
            QStringLiteral("selectionActionTargetLanguage"));
        targetLanguageCombo_->setAccessibleName(text8("目标语言"));
        targetLanguageCombo_->setEditable(true);
        targetLanguageCombo_->setInsertPolicy(QComboBox::NoInsert);
        if (catalogs.targetLanguages.isEmpty()
            || !catalogs.targetLanguages.first().second.isEmpty()) {
            targetLanguageCombo_->addItem(
                text8("跟随全局目标语言"), QString());
        }
        addCatalogItems(targetLanguageCombo_, catalogs.targetLanguages);
        prepareCombo(targetLanguageCombo_);
        QLabel *targetLanguageLabel = fieldLabel(
            text8("目标语言"), specificFields_);
        targetLanguageLabel->setObjectName(
            QStringLiteral("selectionActionTargetLanguageLabel"));
        targetLanguageLabel->setBuddy(targetLanguageCombo_);
        specific->addRow(targetLanguageLabel, targetLanguageCombo_);
    }

    if (actionId_ == selectionContextActionSave()) {
        vocabularyScopeCombo_ = new QComboBox(specificFields_);
        vocabularyScopeCombo_->setObjectName(
            QStringLiteral("selectionActionVocabularyScope"));
        vocabularyScopeCombo_->setAccessibleName(text8("默认作用范围"));
        addCatalogItems(vocabularyScopeCombo_, catalogs.vocabularyScopes);
        if (vocabularyScopeCombo_->findData(QStringLiteral("__global")) < 0) {
            vocabularyScopeCombo_->insertItem(
            0, text8("全局词库"), QStringLiteral("__global"));
        }
        prepareCombo(vocabularyScopeCombo_);
        QLabel *vocabularyScopeLabel = fieldLabel(
            text8("默认作用范围"), specificFields_);
        vocabularyScopeLabel->setObjectName(
            QStringLiteral("selectionActionVocabularyScopeLabel"));
        vocabularyScopeLabel->setBuddy(vocabularyScopeCombo_);
        specific->addRow(vocabularyScopeLabel, vocabularyScopeCombo_);
    }

    if (actionId_ == selectionContextActionCopy()) {
        copyModeCombo_ = new QComboBox(specificFields_);
        copyModeCombo_->setObjectName(QStringLiteral("selectionActionCopyMode"));
        copyModeCombo_->setAccessibleName(text8("复制文本"));
        copyModeCombo_->addItem(text8("保留原文"), QStringLiteral("original"));
        copyModeCombo_->addItem(text8("去除首尾空白"), QStringLiteral("trim"));
        prepareCombo(copyModeCombo_);
        QLabel *copyModeLabel = fieldLabel(text8("复制文本"), specificFields_);
        copyModeLabel->setObjectName(
            QStringLiteral("selectionActionCopyModeLabel"));
        copyModeLabel->setBuddy(copyModeCombo_);
        specific->addRow(copyModeLabel, copyModeCombo_);
    }

    root->addWidget(specificFields_);

    connect(displayNameEdit_, &QLineEdit::textChanged, this,
            [this](const QString &) { notifyChanged(); });
    connect(visibleCheck_, &QCheckBox::toggled, this,
            [this](bool) { notifyChanged(); });
    connect(expandButton_, &QToolButton::clicked, this,
            [this]() { setExpanded(!isExpanded()); });
    connect(restoreButton, &QPushButton::clicked, this, [this]() {
        const std::function<void()> restore = callbacks_.restoreRequested;
        if (!restore) {
            return;
        }
        restore();
    });

    if (modelCombo_) {
        connect(modelCombo_,
                static_cast<void (QComboBox::*)(int)>(
                    &QComboBox::currentIndexChanged),
                this, [this](int) { notifyChanged(); });
    }
    if (promptEdit_) {
        connect(promptEdit_, &QPlainTextEdit::textChanged, this, [this]() {
            if (updating_) {
                return;
            }
            const QString current = promptEdit_->toPlainText();
            if (current.size() > kPromptLimit) {
                const std::function<void(const QString &)> warning =
                    callbacks_.validationWarning;
                const QString message =
                    text8("提示词不能超过 8000 个字符。");
                const quint64 warningGeneration = changeDeliveryGeneration_;
                {
                    QSignalBlocker blocker(promptEdit_);
                    const int rejectedCursorPosition =
                        promptEdit_->textCursor().position();
                    if (promptEdit_->document()->isUndoAvailable()) {
                        promptEdit_->undo();
                    }
                    if (promptEdit_->toPlainText() != lastValidPrompt_) {
                        restoreTextAsUndoableEdit(
                            promptEdit_, lastValidPrompt_);
                    }
                    if (promptEdit_->toPlainText() != lastValidPrompt_) {
                        promptEdit_->setPlainText(lastValidPrompt_);
                    }
                    QTextCursor cursor(promptEdit_->document());
                    cursor.setPosition(qMin(
                        rejectedCursorPosition,
                        lastValidPrompt_.size()
                    ));
                    promptEdit_->setTextCursor(cursor);
                    updatePromptCount(lastValidPrompt_.size());
                }
                if (warning) {
                    const QPointer<SelectionContextActionEditor> alive(this);
                    QTimer::singleShot(0, [alive, warning, message,
                                          warningGeneration]() {
                        if (!alive
                            || alive->changeDeliveryGeneration_
                                != warningGeneration) {
                            return;
                        }
                        warning(message);
                    });
                }
                return;
            }
            lastValidPrompt_ = current;
            updatePromptCount(current.size());
            notifyChanged();
        });
    }
    if (targetLanguageCombo_) {
        connect(targetLanguageCombo_,
                static_cast<void (QComboBox::*)(int)>(
                    &QComboBox::currentIndexChanged),
                this, [this](int) { notifyChanged(); });
        connect(targetLanguageCombo_->lineEdit(), &QLineEdit::textChanged,
                this, [this](const QString &) { notifyChanged(); });
    }
    if (vocabularyScopeCombo_) {
        connect(vocabularyScopeCombo_,
                static_cast<void (QComboBox::*)(int)>(
                    &QComboBox::currentIndexChanged),
                this, [this](int) { notifyChanged(); });
    }
    if (copyModeCombo_) {
        connect(copyModeCombo_,
                static_cast<void (QComboBox::*)(int)>(
                    &QComboBox::currentIndexChanged),
                this, [this](int) { notifyChanged(); });
    }

    value_ = SelectionContextActionCustomization();
    setCustomization(value_);
    setExpanded(false);
}

void SelectionContextActionEditor::setCustomization(
    const SelectionContextActionCustomization &value
)
{
    if (isSaveEchoForActiveDelivery(this, value)) {
        return;
    }
    ++changeDeliveryGeneration_;
    pendingChanges_.clear();
    changeDrainScheduled_ = false;
    updating_ = true;
    SelectionContextActionCustomization accepted = value;
    const bool rejectedPrompt = promptEdit_
        && value.promptOverride.size() > kPromptLimit;
    if (rejectedPrompt) {
        accepted.promptOverride = lastValidPrompt_;
    } else if (promptEdit_) {
        lastValidPrompt_ = value.promptOverride;
    }
    value_ = accepted;
    displayNameEdit_->setText(accepted.displayName);
    visibleCheck_->setChecked(accepted.visible);

    if (modelCombo_) {
        selectCatalogValue(
            modelCombo_, accepted.modelId, text8("不可用"), false);
    }
    if (promptEdit_) {
        promptEdit_->setPlainText(lastValidPrompt_);
        updatePromptCount(lastValidPrompt_.size());
    }
    if (targetLanguageCombo_) {
        selectCatalogValue(
            targetLanguageCombo_, accepted.targetLanguage, QString(), true);
    }
    if (vocabularyScopeCombo_) {
        selectCatalogValue(
            vocabularyScopeCombo_, accepted.vocabularyScopeId,
            text8("不可用"), false);
    }
    if (copyModeCombo_) {
        int index = copyModeCombo_->findData(accepted.copyMode);
        if (index < 0) {
            index = copyModeCombo_->findData(QStringLiteral("original"));
        }
        copyModeCombo_->setCurrentIndex(index);
    }
    updating_ = false;
    lastNotifiedValue_ = customization();
    hasLastNotifiedValue_ = true;
    if (rejectedPrompt) {
        const std::function<void(const QString &)> warning =
            callbacks_.validationWarning;
        if (warning) {
            warning(text8("提示词不能超过 8000 个字符。"));
        }
        return;
    }
}

SelectionContextActionCustomization
SelectionContextActionEditor::customization() const
{
    SelectionContextActionCustomization result = value_;
    result.displayName = displayNameEdit_->text();
    result.visible = visibleCheck_->isChecked();
    if (modelCombo_) {
        result.modelId = modelCombo_->currentData().toString();
    }
    if (promptEdit_) {
        result.promptOverride = promptEdit_->toPlainText();
    }
    if (targetLanguageCombo_) {
        const int index = targetLanguageCombo_->currentIndex();
        if (index >= 0
            && targetLanguageCombo_->currentText()
                == targetLanguageCombo_->itemText(index)) {
            result.targetLanguage =
                targetLanguageCombo_->itemData(index).toString();
        } else {
            result.targetLanguage = targetLanguageCombo_->currentText();
        }
    }
    if (vocabularyScopeCombo_) {
        result.vocabularyScopeId =
            vocabularyScopeCombo_->currentData().toString();
    }
    if (copyModeCombo_) {
        result.copyMode = copyModeCombo_->currentData().toString();
    }
    return result;
}

void SelectionContextActionEditor::setExpanded(bool expanded)
{
    expanded_ = expanded;
    specificFields_->setVisible(expanded_);
    expandButton_->setChecked(expanded_);
    expandButton_->setText(expanded_ ? text8("收起") : text8("设置"));
    expandButton_->setArrowType(
        expanded_ ? Qt::UpArrow : Qt::DownArrow);
    expandButton_->setAccessibleDescription(
        expanded_ ? text8("动作设置当前已展开")
                  : text8("动作设置当前已收起"));
}

bool SelectionContextActionEditor::isExpanded() const
{
    return expanded_;
}

void SelectionContextActionEditor::notifyChanged()
{
    if (updating_) {
        return;
    }
    const SelectionContextActionCustomization current = customization();
    if (hasLastNotifiedValue_
        && sameCustomization(current, lastNotifiedValue_)) {
        return;
    }
    lastNotifiedValue_ = current;
    hasLastNotifiedValue_ = true;
    value_ = current;
    const std::function<void(const SelectionContextActionCustomization &)>
        changed = callbacks_.changed;
    if (!changed) {
        return;
    }
    pendingChanges_.enqueue(current);
    scheduleChangeDrain();
}

void SelectionContextActionEditor::scheduleChangeDrain()
{
    if (changeDrainScheduled_ || pendingChanges_.isEmpty()) {
        return;
    }
    changeDrainScheduled_ = true;
    const quint64 generation = changeDeliveryGeneration_;
    const QPointer<SelectionContextActionEditor> alive(this);
    QTimer::singleShot(0, [alive, generation]() {
        if (!alive
            || alive->changeDeliveryGeneration_ != generation) {
            return;
        }
        alive->drainOnePendingChange(generation);
    });
}

void SelectionContextActionEditor::drainOnePendingChange(quint64 generation)
{
    if (generation != changeDeliveryGeneration_) {
        return;
    }
    if (pendingChanges_.isEmpty()) {
        changeDrainScheduled_ = false;
        return;
    }

    const SelectionContextActionCustomization current =
        pendingChanges_.dequeue();
    const std::function<void(const SelectionContextActionCustomization &)>
        changed = callbacks_.changed;
    changeDrainScheduled_ = false;
    if (!pendingChanges_.isEmpty()) {
        scheduleChangeDrain();
    }
    if (!changed) {
        return;
    }
    deliverChangedCallback(this, current, changed);
}

void SelectionContextActionEditor::updatePromptCount(int length)
{
    if (!promptCountLabel_) {
        return;
    }
    promptCountLabel_->setText(
        QStringLiteral("%1 / %2").arg(length).arg(kPromptLimit));
    promptCountLabel_->setMinimumHeight(promptCountLabel_->sizeHint().height());
}

void SelectionContextActionEditor::selectCatalogValue(
    QComboBox *combo,
    const QString &value,
    const QString &unavailableSuffix,
    bool allowEditableValue
)
{
    removeInjectedUnavailableItems(combo);
    int index = combo->findData(value);
    if (index >= 0) {
        combo->setCurrentIndex(index);
        return;
    }
    if (value.isEmpty()) {
        combo->setCurrentIndex(combo->count() > 0 ? 0 : -1);
        return;
    }
    if (allowEditableValue && combo->isEditable()) {
        combo->setCurrentIndex(-1);
        combo->setEditText(value);
        return;
    }
    const QString label = unavailableSuffix.isEmpty()
        ? value
        : QStringLiteral("%1（%2）").arg(value, unavailableSuffix);
    combo->addItem(label, value);
    const int injected = combo->count() - 1;
    combo->setItemData(injected, true, kInjectedUnavailableRole);
    combo->setCurrentIndex(injected);
}
