#ifndef VOCEKIT_VOCABULARY_PAGE_ACCESS_FACTORY_H
#define VOCEKIT_VOCABULARY_PAGE_ACCESS_FACTORY_H

#include "vocabulary_page_controller.h"

class HubSettingsState;

// 词库页的装配输入。工厂负责生成设置快照并补齐安全回调。
struct VocabularyPageAccessFactoryDependencies
{
    HubSettingsState *settings = nullptr;
    VocabularyAiCallback vocabularyAi;
    std::function<QVector<HistoryEntry>()> historyEntries;
    std::function<void(const QStringList &, bool)> vocabularyChanged;
};

VocabularyPageAccess createVocabularyPageAccess(
    const VocabularyPageAccessFactoryDependencies &dependencies
);

#endif // VOCEKIT_VOCABULARY_PAGE_ACCESS_FACTORY_H
