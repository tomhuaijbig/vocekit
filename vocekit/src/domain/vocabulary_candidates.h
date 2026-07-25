#ifndef VOCEKIT_VOCABULARY_CANDIDATES_H
#define VOCEKIT_VOCABULARY_CANDIDATES_H

#include "app_legacy_types.h"
#include "history_types.h"

#include <QVector>

// 词库候选生成输入：把历史记录、已有词条和自定义功能列表集中传入，
// 避免页面层直接保存推荐算法细节。
struct VocabularyCandidateRequest
{
    QVector<HistoryEntry> history;
    QVector<VocabularyEntry> existingEntries;
    QVector<CustomFunctionDef> customFunctions;
    int maxHistoryRecords = 1200;
    int maxCandidates = 80;
};

QVector<VocabularyCandidate> buildVocabularyCandidates(const VocabularyCandidateRequest &request);

#endif // VOCEKIT_VOCABULARY_CANDIDATES_H
