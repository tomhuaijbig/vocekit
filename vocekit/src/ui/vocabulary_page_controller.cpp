#include "vocabulary_page_controller.h"

#include "../config/app_paths.h"
#include "../domain/vocabulary_candidates.h"
#include "../storage/vocabulary_store.h"
#include "attention_message.h"
#include "vocabulary_candidates_dialog.h"
#include "vocabulary_entry_dialog.h"
#include "vocabulary_page.h"
#include "vocabulary_transfer_controller.h"

#include <QtWidgets>
#include <QDesktopServices>
#include <QUrl>

static QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

VocabularyPageController::VocabularyPageController(
    QWidget *parent,
    const VocabularyPageAccess &access
)
    : m_parent(parent),
      m_access(access)
{
}

QWidget *VocabularyPageController::page()
{
    if (m_page) {
        return m_page;
    }

    m_page = new VocabularyPage(
        [this]() { return scopeOptions(); },
        []() { return loadVocabularyEntries(); },
        m_parent
    );

    VocabularyPageCallbacks callbacks;
    callbacks.addEntry = [this]() { addEntry(); };
    callbacks.showCandidates = [this]() { showCandidates(); };
    callbacks.importEntries = [this]() { importEntries(); };
    callbacks.exportEntries = [this]() { exportEntries(); };
    callbacks.openDirectory = [this]() { openDirectory(); };
    callbacks.editEntry = [this](const VocabularyEntry &entry) {
        editEntry(entry);
    };
    callbacks.deleteEntry = [this](const QString &id, const QString &source) {
        deleteEntry(id, source);
    };
    m_page->setCallbacks(callbacks);
    m_page->refresh();
    return m_page;
}

VocabularyPage *VocabularyPageController::pageWidget() const
{
    return m_page;
}

bool VocabularyPageController::pageCreated() const
{
    return !m_page.isNull();
}

QVector<VocabularyScopeOption> VocabularyPageController::scopeOptions() const
{
    QVector<VocabularyScopeOption> options = builtinVocabularyScopeOptions();
    const VocabularyPageSettingsSnapshot snapshot = settingsSnapshot();
    for (const CustomFunctionDef &fn : snapshot.customFunctions) {
        const QString title = fn.name.trimmed().isEmpty() ? tr8("自定义") : fn.name.trimmed();
        options.append({fn.id, title});
    }
    return options;
}

QString VocabularyPageController::currentScopeId() const
{
    return m_page ? m_page->currentScopeId() : QStringLiteral("__all");
}

QVector<VocabularyEntry> VocabularyPageController::currentFilteredEntries() const
{
    return m_page ? m_page->currentFilteredEntries() : QVector<VocabularyEntry>();
}

void VocabularyPageController::refresh()
{
    if (m_page) {
        m_page->refresh();
    }
}

void VocabularyPageController::addEntry()
{
    editEntry();
}

void VocabularyPageController::editEntry(const VocabularyEntry &existing)
{
    const bool editing = !existing.id.trimmed().isEmpty();
    VocabularyEntryDialog::Options options;
    options.existing = existing;
    options.scopes = scopeOptions();
    options.aiCallback = m_access.vocabularyAi;
    VocabularyEntryDialog dialog(options, m_parent);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    VocabularyEntry entry = dialog.entry();
    QVector<VocabularyEntry> entries = loadVocabularyEntries();
    if (editing) {
        bool found = false;
        for (VocabularyEntry &item : entries) {
            if (item.id == existing.id) {
                item = entry;
                found = true;
                break;
            }
        }
        if (!found) {
            entry.id = existing.id;
            entries.append(entry);
        }
    } else {
        entry.id = nextVocabularyEntryId(entries);
        entries.append(entry);
    }

    if (!saveVocabularyEntries(entries)) {
        showAttentionWarning(m_parent, tr8("保存失败"), tr8("无法写入 config/lexicon/entries.json。"));
        return;
    }
    notifyVocabularyChanged(QStringList() << entry.id, false);
}

void VocabularyPageController::deleteEntry(const QString &id, const QString &source)
{
    if (QMessageBox::question(
            m_parent,
            tr8("删除词条"),
            tr8("确定删除“") + source + tr8("”吗？")
        ) != QMessageBox::Yes) {
        return;
    }

    QVector<VocabularyEntry> entries = loadVocabularyEntries();
    bool removed = false;
    for (int i = entries.size() - 1; i >= 0; --i) {
        if (entries[i].id == id) {
            entries.remove(i);
            removed = true;
        }
    }
    if (!removed) {
        showAttentionWarning(m_parent, tr8("删除失败"), tr8("没有找到要删除的词条。"));
        return;
    }
    if (!saveVocabularyEntries(entries)) {
        showAttentionWarning(m_parent, tr8("保存失败"), tr8("无法写入 config/lexicon/entries.json。"));
        return;
    }
    notifyVocabularyChanged(QStringList() << id, false);
}

void VocabularyPageController::showCandidates()
{
    const QVector<VocabularyCandidate> candidates = collectCandidates();
    if (candidates.isEmpty()) {
        showAttentionInformation(m_parent, tr8("暂无候选"), tr8("没有从历史记录里找到新的词库候选。"));
        return;
    }

    VocabularyCandidatesDialog dialog(candidates, scopeOptions(), m_parent);
    dialog.setEditCallback([this](const VocabularyEntry &entry) {
        editEntry(entry);
    });
    dialog.setAddCallback([this](const VocabularyEntry &entry, QString *error) {
        return addEntryDirect(entry, error);
    });
    dialog.exec();
}

void VocabularyPageController::importEntries()
{
    transferController().importEntries();
}

void VocabularyPageController::exportEntries()
{
    transferController().exportEntries();
}

void VocabularyPageController::openDirectory()
{
    const QString path = QDir(appBasePath()).filePath(QStringLiteral("config/lexicon"));
    QDir().mkpath(path);
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

bool VocabularyPageController::addEntryDirect(VocabularyEntry entry, QString *error)
{
    if (error) {
        error->clear();
    }
    entry.source = entry.source.trimmed();
    entry.target = entry.target.trimmed();
    entry.aliases = entry.aliases.trimmed();
    entry.scopeId = normalizeVocabularyImportScope(entry.scopeId, scopeOptions());
    entry.matchMode = normalizeVocabularyImportMatchMode(entry.matchMode);
    if (entry.source.isEmpty() || entry.target.isEmpty() || !vocabularyEntryHasCorrection(entry)) {
        if (error) {
            *error = tr8("词条缺少原词、标准写法，或没有修正效果。");
        }
        return false;
    }

    QVector<VocabularyEntry> entries = loadVocabularyEntries();
    const QString key = vocabularyEntryUniqueKey(entry);
    for (const VocabularyEntry &existing : entries) {
        if (vocabularyEntryUniqueKey(existing) == key) {
            if (error) {
                *error = tr8("已经存在相同词条。");
            }
            return false;
        }
    }

    entry.id = nextVocabularyEntryId(entries);
    entries.append(entry);
    if (!saveVocabularyEntries(entries)) {
        if (error) {
            *error = tr8("无法写入 config/lexicon/entries.json。");
        }
        return false;
    }

    notifyVocabularyChanged(QStringList() << entry.id, false);
    return true;
}

QVector<VocabularyCandidate> VocabularyPageController::collectCandidates() const
{
    VocabularyCandidateRequest request;
    request.history = m_access.historyEntries
        ? m_access.historyEntries()
        : QVector<HistoryEntry>();
    request.existingEntries = loadVocabularyEntries();
    request.customFunctions = settingsSnapshot().customFunctions;
    return buildVocabularyCandidates(request);
}

VocabularyTransferController VocabularyPageController::transferController()
{
    VocabularyTransferController::Callbacks callbacks;
    callbacks.scopeOptions = [this]() { return scopeOptions(); };
    callbacks.filteredEntries = [this]() { return currentFilteredEntries(); };
    callbacks.loadEntries = []() { return loadVocabularyEntries(); };
    callbacks.saveEntries = [](const QVector<VocabularyEntry> &entries) {
        return saveVocabularyEntries(entries);
    };
    callbacks.currentScopeId = [this]() { return currentScopeId(); };
    callbacks.searchText = [this]() {
        return m_page ? m_page->searchText() : QString();
    };
    callbacks.vocabularyChanged = [this]() {
        notifyVocabularyChanged(QStringList(), true);
    };
    return VocabularyTransferController(m_parent, callbacks);
}

void VocabularyPageController::notifyVocabularyChanged(const QStringList &entryIds, bool resetRequired) const
{
    if (m_access.vocabularyChanged) {
        m_access.vocabularyChanged(entryIds, resetRequired);
        return;
    }
    if (m_page) {
        m_page->refresh();
    }
}

VocabularyPageSettingsSnapshot VocabularyPageController::settingsSnapshot() const
{
    return m_access.settingsSnapshotProvider
        ? m_access.settingsSnapshotProvider()
        : VocabularyPageSettingsSnapshot();
}
