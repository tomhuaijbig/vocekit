#ifndef VOCEKIT_VOCABULARY_PAGE_H
#define VOCEKIT_VOCABULARY_PAGE_H

#include "../domain/app_legacy_types.h"
#include "../domain/vocabulary_io.h"

#include <QWidget>

#include <functional>

class QLineEdit;
class QTabWidget;

struct VocabularyPageCallbacks
{
    std::function<void()> addEntry;
    std::function<void()> showCandidates;
    std::function<void()> importEntries;
    std::function<void()> exportEntries;
    std::function<void()> openDirectory;
    std::function<void(const VocabularyEntry &)> editEntry;
    std::function<void(const QString &, const QString &)> deleteEntry;
};

class VocabularyPage : public QWidget
{
public:
    typedef std::function<QVector<VocabularyScopeOption>()> ScopeProvider;
    typedef std::function<QVector<VocabularyEntry>()> EntryProvider;

    explicit VocabularyPage(
        const ScopeProvider &scopeProvider,
        const EntryProvider &entryProvider,
        QWidget *parent = nullptr
    );

    void setCallbacks(const VocabularyPageCallbacks &callbacks);
    void refresh();
    QString currentScopeId() const;
    QString searchText() const;
    QVector<VocabularyEntry> currentFilteredEntries() const;

private:
    QWidget *tabContent(const QString &scopeId, const QString &scopeTitle);
    QWidget *entryCard(const VocabularyEntry &entry);
    QWidget *emptyCard() const;
    QVector<VocabularyScopeOption> scopes() const;
    QVector<VocabularyEntry> entries() const;

    ScopeProvider m_scopeProvider;
    EntryProvider m_entryProvider;
    VocabularyPageCallbacks m_callbacks;
    QLineEdit *m_searchEdit = nullptr;
    QTabWidget *m_tabs = nullptr;
};

#endif // VOCEKIT_VOCABULARY_PAGE_H
