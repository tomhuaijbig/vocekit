#ifndef VOCEKIT_VOCABULARY_TRANSFER_CONTROLLER_H
#define VOCEKIT_VOCABULARY_TRANSFER_CONTROLLER_H

#include "../domain/vocabulary_io.h"

#include <QString>
#include <QVector>

#include <functional>

class QWidget;

// 词库导入导出控制器：集中处理文件对话、解析、去重、保存和提示。
class VocabularyTransferController
{
public:
    struct Callbacks
    {
        std::function<QVector<VocabularyScopeOption>()> scopeOptions;
        std::function<QVector<VocabularyEntry>()> filteredEntries;
        std::function<QVector<VocabularyEntry>()> loadEntries;
        std::function<bool(const QVector<VocabularyEntry> &)> saveEntries;
        std::function<QString()> currentScopeId;
        std::function<QString()> searchText;
        std::function<void()> vocabularyChanged;
    };

    VocabularyTransferController(QWidget *parent, const Callbacks &callbacks);

    void importEntries();
    void exportEntries();

private:
    QVector<VocabularyScopeOption> scopeOptions() const;
    QVector<VocabularyEntry> filteredEntries() const;
    QVector<VocabularyEntry> loadEntries() const;
    bool saveEntries(const QVector<VocabularyEntry> &entries) const;
    QString currentScopeId() const;
    QString searchText() const;
    void notifyVocabularyChanged() const;

    static QString exportTimestamp();

    QWidget *m_parent = nullptr;
    Callbacks m_callbacks;
};

#endif // VOCEKIT_VOCABULARY_TRANSFER_CONTROLLER_H
