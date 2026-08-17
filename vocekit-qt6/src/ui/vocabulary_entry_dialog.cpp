#include "vocabulary_entry_dialog.h"

#include "attention_message.h"
#include "ui_style.h"
#include "../domain/vocabulary_ai.h"
#include "../storage/vocabulary_store.h"

#include <QtWidgets>

namespace {

QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

} // namespace

VocabularyEntryDialog::VocabularyEntryDialog(const Options &options, QWidget *parent)
    : HelpDialog(
          tr8("词条说明"),
          tr8("原词/错词是识别或输出中可能出现的写法，标准写法是希望软件改成的结果。\n\n作用范围决定词条在哪些功能里生效；匹配方式决定命中规则。你可以手动填写，也可以点击“AI 填充”让模型先生成草稿，再检查后保存。\n\n用加入词库快捷键时，会按“设置 -> 常用设置”里的“快捷键加入方式”执行：自动使用 AI、每次询问或不使用 AI。"),
          parent
      ),
      m_options(options),
      m_editing(!options.existing.id.trimmed().isEmpty())
{
    setWindowTitle(m_editing ? tr8("编辑词条") : tr8("新增词条"));
    setMinimumSize(640, 520);
    setFont(appFont());
    setStyleSheet(QStringLiteral("QDialog { background: #f6f7f9; } QLabel { color: #111827; }"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(22, 20, 22, 18);
    root->setSpacing(14);

    auto *header = new QHBoxLayout;
    auto *title = new QLabel(m_editing ? tr8("编辑词条") : tr8("新增词条"));
    title->setFont(appFont(18, QFont::DemiBold));
    header->addWidget(title, 1);
    root->addLayout(header);

    auto *formCard = new QFrame;
    formCard->setObjectName(QStringLiteral("card"));
    formCard->setStyleSheet(cardStyle());
    auto *form = new QGridLayout(formCard);
    form->setContentsMargins(16, 14, 16, 14);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(12);

    m_source = new QLineEdit;
    m_source->setText(options.existing.source);
    m_source->setPlaceholderText(tr8("例如：deepseep、项目简称、容易识别错的词"));
    m_target = new QLineEdit;
    m_target->setText(options.existing.target);
    m_target->setPlaceholderText(tr8("例如：DeepSeek、正式项目名、固定译名"));
    m_aliases = new QLineEdit;
    m_aliases->setText(options.existing.aliases);
    m_aliases->setPlaceholderText(tr8("可选，多个别名用逗号分隔"));

    m_scope = new QComboBox;
    const QVector<VocabularyScopeOption> scopes =
        options.scopes.isEmpty() ? builtinVocabularyScopeOptions() : options.scopes;
    for (const VocabularyScopeOption &scope : scopes) {
        m_scope->addItem(scope.title, scope.id);
    }

    m_match = new QComboBox;
    m_match->addItem(tr8("精确匹配"), QStringLiteral("exact"));
    m_match->addItem(tr8("忽略大小写"), QStringLiteral("caseInsensitive"));
    m_match->addItem(tr8("包含匹配"), QStringLiteral("contains"));
    m_match->addItem(tr8("正则匹配"), QStringLiteral("regex"));

    m_enabled = new QCheckBox;
    m_enabled->setChecked(m_editing ? options.existing.enabled : true);
    m_enabled->setFont(appFont(10, QFont::DemiBold));

    m_note = new QTextEdit;
    m_note->setPlainText(options.existing.note);
    m_note->setPlaceholderText(tr8("可选，记录这个词条适用场景或使用注意事项"));
    m_note->setMinimumHeight(100);

    setScopeById(options.existing.scopeId);
    setMatchModeById(options.existing.matchMode);

    form->addWidget(new QLabel(tr8("原词 / 错词")), 0, 0);
    form->addWidget(m_source, 0, 1);
    form->addWidget(new QLabel(tr8("标准写法")), 1, 0);
    form->addWidget(m_target, 1, 1);
    form->addWidget(new QLabel(tr8("别名")), 2, 0);
    form->addWidget(m_aliases, 2, 1);
    form->addWidget(new QLabel(tr8("作用范围")), 3, 0);
    form->addWidget(m_scope, 3, 1);
    form->addWidget(new QLabel(tr8("匹配方式")), 4, 0);
    form->addWidget(m_match, 4, 1);
    form->addWidget(new QLabel(tr8("状态")), 5, 0);
    form->addWidget(m_enabled, 5, 1);
    form->addWidget(new QLabel(tr8("备注")), 6, 0, Qt::AlignTop);
    form->addWidget(m_note, 6, 1);
    form->setColumnStretch(1, 1);
    root->addWidget(formCard, 1);

    auto *buttons = new QHBoxLayout;
    auto *fillWithAi = new QPushButton(tr8("AI 填充"));
    fillWithAi->setFixedHeight(38);
    fillWithAi->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
    fillWithAi->setEnabled(static_cast<bool>(m_options.aiCallback));
    buttons->addWidget(fillWithAi);
    buttons->addStretch();

    auto *cancel = new QPushButton(tr8("取消"));
    cancel->setFixedHeight(38);
    cancel->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
    auto *save = new QPushButton(tr8("保存词条"));
    save->setFixedHeight(38);
    save->setStyleSheet(buttonStyle(QStringLiteral("#111827")));
    buttons->addWidget(cancel);
    buttons->addWidget(save);
    root->addLayout(buttons);

    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(fillWithAi, &QPushButton::clicked, this, [this]() {
        this->fillWithAi();
    });
    connect(save, &QPushButton::clicked, this, [this]() {
        if (validateCurrentEntry()) {
            accept();
        }
    });
}

VocabularyEntry VocabularyEntryDialog::entry() const
{
    return entryFromForm();
}

void VocabularyEntryDialog::fillWithAi()
{
    const QString sourceText = m_source->text().trimmed();
    const QString targetText = m_target->text().trimmed();
    const QString aliasesText = m_aliases->text().trimmed();
    const QString noteText = m_note->toPlainText().trimmed();

    QStringList seedParts;
    if (!sourceText.isEmpty()) seedParts << sourceText;
    if (!targetText.isEmpty()) seedParts << targetText;
    if (!aliasesText.isEmpty()) seedParts << aliasesText;
    if (!noteText.isEmpty()) seedParts << noteText;
    const QString input = seedParts.join(tr8("\n"));
    if (input.isEmpty()) {
        showAttentionInformation(this, tr8("缺少文字"), tr8("请先填写原词/错词、标准写法、别名或备注，再让 AI 生成词条。"));
        return;
    }
    if (!m_options.aiCallback) {
        showAttentionInformation(this, tr8("AI 不可用"), tr8("当前没有可用的大模型生成入口。"));
        return;
    }

    QString error;
    QStringList formContext;
    formContext << compactPromptField(tr8("当前原词/错词"), sourceText)
                << compactPromptField(tr8("当前标准写法"), targetText)
                << compactPromptField(tr8("当前别名"), aliasesText)
                << compactPromptField(tr8("当前作用范围"), m_scope->currentText())
                << compactPromptField(tr8("当前匹配方式"), m_match->currentText())
                << compactPromptField(tr8("当前备注"), noteText)
                << tr8("要求：已经填写的字段优先保留；只补全缺失字段、明显错别字或可用别名。不要把整段句子当成 source 或 target。");
    const VocabularySuggestion suggestion = m_options.aiCallback(
        input,
        m_scope->currentData().toString(),
        &error,
        targetText,
        formContext.join(tr8("\n"))
    );
    if (!suggestion.valid) {
        showAttentionWarning(this, tr8("AI 生成失败"), error.isEmpty() ? tr8("模型没有返回可用词条。") : error);
        return;
    }

    if (sourceText.isEmpty()) {
        m_source->setText(suggestion.entry.source);
    }
    const bool currentHasNoCorrection = sourceText == targetText && aliasesText.isEmpty();
    if (targetText.isEmpty() || (currentHasNoCorrection && !suggestion.entry.target.trimmed().isEmpty())) {
        m_target->setText(suggestion.entry.target);
    }
    m_aliases->setText(mergeVocabularyAliasText(aliasesText, suggestion.entry.aliases));
    setScopeById(suggestion.entry.scopeId, true);
    setMatchModeById(suggestion.entry.matchMode, true);
    m_enabled->setChecked(suggestion.entry.enabled);
    if (noteText.isEmpty() && !suggestion.entry.note.trimmed().isEmpty()) {
        m_note->setPlainText(suggestion.entry.note);
    }
}

bool VocabularyEntryDialog::validateCurrentEntry()
{
    const VocabularyEntry current = entryFromForm();
    if (current.source.isEmpty() || current.target.isEmpty()) {
        showAttentionWarning(this, tr8("词条不完整"), tr8("请填写原词/错词和标准写法。"));
        return false;
    }
    if (!vocabularyEntryHasCorrection(current)) {
        showAttentionWarning(
            this,
            tr8("词条无修正效果"),
            tr8("原词/错词和标准写法完全相同，且没有可替换的别名。请把容易识别错的写法填到“原词/错词”或“别名”，把正确写法填到“标准写法”。")
        );
        return false;
    }
    return true;
}

VocabularyEntry VocabularyEntryDialog::entryFromForm() const
{
    VocabularyEntry result = m_options.existing;
    result.source = m_source->text().trimmed();
    result.target = m_target->text().trimmed();
    result.aliases = m_aliases->text().trimmed();
    result.scopeId = m_scope->currentData().toString();
    result.matchMode = m_match->currentData().toString();
    result.enabled = m_enabled->isChecked();
    result.note = m_note->toPlainText();
    return result;
}

void VocabularyEntryDialog::setScopeById(const QString &scopeId, bool onlyWhenGlobal)
{
    if (onlyWhenGlobal && normalizeVocabularyScope(m_scope->currentData().toString()) != QStringLiteral("__global")) {
        return;
    }
    const int index = m_scope->findData(normalizeVocabularyScope(scopeId));
    if (index >= 0) {
        m_scope->setCurrentIndex(index);
    }
}

void VocabularyEntryDialog::setMatchModeById(const QString &matchMode, bool onlyWhenDefault)
{
    if (onlyWhenDefault && normalizeVocabularyMatchMode(m_match->currentData().toString()) != QStringLiteral("caseInsensitive")) {
        return;
    }
    const int index = m_match->findData(normalizeVocabularyMatchMode(matchMode));
    if (index >= 0) {
        m_match->setCurrentIndex(index);
    }
}
