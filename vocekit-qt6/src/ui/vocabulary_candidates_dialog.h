#ifndef VOCEKIT_VOCABULARY_CANDIDATES_DIALOG_H
#define VOCEKIT_VOCABULARY_CANDIDATES_DIALOG_H

#include "app_dialogs.h"
#include "../domain/app_legacy_types.h"
#include "../domain/vocabulary_io.h"

#include <functional>

// 词库候选推荐弹窗：展示从历史记录里提取出的候选词条，并把编辑/加入动作交给调用方处理。
class VocabularyCandidatesDialog : public AppDialog
{
public:
    typedef std::function<void(const VocabularyEntry &)> EditCallback;
    typedef std::function<bool(const VocabularyEntry &, QString *)> AddCallback;

    explicit VocabularyCandidatesDialog(
        const QVector<VocabularyCandidate> &candidates,
        const QVector<VocabularyScopeOption> &scopes,
        QWidget *parent = nullptr
    );

    void setEditCallback(const EditCallback &callback);
    void setAddCallback(const AddCallback &callback);

private:
    QWidget *candidateCard(const VocabularyCandidate &candidate);
    QString scopeTitle(const QString &scopeId) const;

    QVector<VocabularyCandidate> m_candidates;
    QVector<VocabularyScopeOption> m_scopes;
    EditCallback m_editCallback;
    AddCallback m_addCallback;
};

#endif // VOCEKIT_VOCABULARY_CANDIDATES_DIALOG_H
