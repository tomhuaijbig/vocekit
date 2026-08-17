#ifndef VOCEKIT_VOCABULARY_PAGE_CONTROLLER_H
#define VOCEKIT_VOCABULARY_PAGE_CONTROLLER_H

#include "../domain/app_legacy_types.h"
#include "../domain/history_types.h"
#include "../domain/vocabulary_io.h"

#include <QPointer>
#include <QStringList>
#include <QVector>

#include <functional>

class VocabularyPage;
class QWidget;

// 词库页面只需要自定义功能列表来生成作用范围和历史候选。
struct VocabularyPageSettingsSnapshot
{
    QVector<CustomFunctionDef> customFunctions;
};

struct VocabularyPageAccess
{
    std::function<VocabularyPageSettingsSnapshot()> settingsSnapshotProvider;
    VocabularyAiCallback vocabularyAi;
    std::function<QVector<HistoryEntry>()> historyEntries;
    std::function<void(const QStringList &, bool)> vocabularyChanged;
};

// 词库页控制器：集中管理词库页创建、刷新、新增编辑、候选推荐、删除和导入导出。
class VocabularyPageController
{
public:
    VocabularyPageController(
        QWidget *parent,
        const VocabularyPageAccess &access
    );

    QWidget *page();
    VocabularyPage *pageWidget() const;
    bool pageCreated() const;

    QVector<VocabularyScopeOption> scopeOptions() const;
    QString currentScopeId() const;
    QVector<VocabularyEntry> currentFilteredEntries() const;

    void refresh();
    void addEntry();
    void editEntry(const VocabularyEntry &existing = VocabularyEntry());
    void deleteEntry(const QString &id, const QString &source);
    void showCandidates();
    void importEntries();
    void exportEntries();
    void openDirectory();

private:
    bool addEntryDirect(VocabularyEntry entry, QString *error);
    QVector<VocabularyCandidate> collectCandidates() const;
    class VocabularyTransferController transferController();
    void notifyVocabularyChanged(const QStringList &entryIds, bool resetRequired) const;
    VocabularyPageSettingsSnapshot settingsSnapshot() const;

    QWidget *m_parent = nullptr;
    VocabularyPageAccess m_access;
    QPointer<VocabularyPage> m_page;
};

#endif // VOCEKIT_VOCABULARY_PAGE_CONTROLLER_H
