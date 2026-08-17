#ifndef VOCEKIT_VOCABULARY_ENTRY_DIALOG_H
#define VOCEKIT_VOCABULARY_ENTRY_DIALOG_H

#include "app_dialogs.h"
#include "../domain/app_legacy_types.h"
#include "../domain/vocabulary_io.h"

class QCheckBox;
class QComboBox;
class QLineEdit;
class QTextEdit;

// 词库词条编辑弹窗：只负责编辑、AI 填充和表单校验，不直接写入词库文件。
class VocabularyEntryDialog : public HelpDialog
{
public:
    struct Options
    {
        VocabularyEntry existing;
        QVector<VocabularyScopeOption> scopes;
        VocabularyAiCallback aiCallback;
    };

    explicit VocabularyEntryDialog(const Options &options, QWidget *parent = nullptr);

    VocabularyEntry entry() const;

private:
    void fillWithAi();
    bool validateCurrentEntry();
    VocabularyEntry entryFromForm() const;
    void setScopeById(const QString &scopeId, bool onlyWhenGlobal = false);
    void setMatchModeById(const QString &matchMode, bool onlyWhenDefault = false);

    Options m_options;
    bool m_editing = false;
    QLineEdit *m_source = nullptr;
    QLineEdit *m_target = nullptr;
    QLineEdit *m_aliases = nullptr;
    QComboBox *m_scope = nullptr;
    QComboBox *m_match = nullptr;
    QCheckBox *m_enabled = nullptr;
    QTextEdit *m_note = nullptr;
};

#endif // VOCEKIT_VOCABULARY_ENTRY_DIALOG_H
