#include "selection_context_action_editor.h"

#include "../domain/selection_context_actions.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QTextCursor>
#include <QToolButton>
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
    common->addWidget(displayNameEdit_, 1);

    visibleCheck_ = new QCheckBox(text8("显示"), this);
    visibleCheck_->setObjectName(QStringLiteral("selectionActionVisible"));
    prepareButton(visibleCheck_);
    common->addWidget(visibleCheck_);

    expandButton_ = new QToolButton(this);
    expandButton_->setObjectName(QStringLiteral("selectionActionExpand"));
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
        modelCombo_->addItem(text8("跟随对应内置功能"), QString());
        addCatalogItems(modelCombo_, catalogs.models);
        prepareCombo(modelCombo_);
        specific->addRow(fieldLabel(text8("模型"), specificFields_), modelCombo_);

        promptEdit_ = new QPlainTextEdit(specificFields_);
        promptEdit_->setObjectName(QStringLiteral("selectionActionPrompt"));
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
        specific->addRow(fieldLabel(text8("提示词"), specificFields_),
                         promptContainer);
    }

    if (actionId_ == selectionContextActionTranslate()) {
        targetLanguageCombo_ = new QComboBox(specificFields_);
        targetLanguageCombo_->setObjectName(
            QStringLiteral("selectionActionTargetLanguage"));
        targetLanguageCombo_->setEditable(true);
        targetLanguageCombo_->setInsertPolicy(QComboBox::NoInsert);
        if (catalogs.targetLanguages.isEmpty()
            || !catalogs.targetLanguages.first().second.isEmpty()) {
            targetLanguageCombo_->addItem(
                text8("跟随全局目标语言"), QString());
        }
        addCatalogItems(targetLanguageCombo_, catalogs.targetLanguages);
        prepareCombo(targetLanguageCombo_);
        specific->addRow(fieldLabel(text8("目标语言"), specificFields_),
                         targetLanguageCombo_);
    }

    if (actionId_ == selectionContextActionSave()) {
        vocabularyScopeCombo_ = new QComboBox(specificFields_);
        vocabularyScopeCombo_->setObjectName(
            QStringLiteral("selectionActionVocabularyScope"));
        addCatalogItems(vocabularyScopeCombo_, catalogs.vocabularyScopes);
        if (vocabularyScopeCombo_->findData(QStringLiteral("__global")) < 0) {
            vocabularyScopeCombo_->insertItem(
                0, text8("全局词库"), QStringLiteral("__global"));
        }
        prepareCombo(vocabularyScopeCombo_);
        specific->addRow(fieldLabel(text8("默认作用范围"), specificFields_),
                         vocabularyScopeCombo_);
    }

    if (actionId_ == selectionContextActionCopy()) {
        copyModeCombo_ = new QComboBox(specificFields_);
        copyModeCombo_->setObjectName(QStringLiteral("selectionActionCopyMode"));
        copyModeCombo_->addItem(text8("保留原文"), QStringLiteral("original"));
        copyModeCombo_->addItem(text8("去除首尾空白"), QStringLiteral("trim"));
        prepareCombo(copyModeCombo_);
        specific->addRow(fieldLabel(text8("复制文本"), specificFields_),
                         copyModeCombo_);
    }

    root->addWidget(specificFields_);

    connect(displayNameEdit_, &QLineEdit::textChanged, this,
            [this](const QString &) { notifyChanged(); });
    connect(visibleCheck_, &QCheckBox::toggled, this,
            [this](bool) { notifyChanged(); });
    connect(expandButton_, &QToolButton::clicked, this,
            [this]() { setExpanded(!isExpanded()); });
    connect(restoreButton, &QPushButton::clicked, this, [this]() {
        if (callbacks_.restoreRequested) {
            callbacks_.restoreRequested();
        }
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
                QSignalBlocker blocker(promptEdit_);
                promptEdit_->setPlainText(lastValidPrompt_);
                QTextCursor cursor = promptEdit_->textCursor();
                cursor.movePosition(QTextCursor::End);
                promptEdit_->setTextCursor(cursor);
                updatePromptCount(lastValidPrompt_.size());
                if (callbacks_.validationWarning) {
                    callbacks_.validationWarning(
                        text8("提示词不能超过 8000 个字符。"));
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
    if (rejectedPrompt && callbacks_.validationWarning) {
        callbacks_.validationWarning(
            text8("提示词不能超过 8000 个字符。"));
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
    expandButton_->setText(expanded_ ? text8("收起") : text8("设置"));
    expandButton_->setArrowType(
        expanded_ ? Qt::UpArrow : Qt::DownArrow);
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
    if (callbacks_.changed) {
        callbacks_.changed(current);
    }
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
