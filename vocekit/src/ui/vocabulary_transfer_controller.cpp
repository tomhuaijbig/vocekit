#include "vocabulary_transfer_controller.h"

#include "../file_utils.h"
#include "../storage/vocabulary_store.h"
#include "../ui/attention_message.h"

#include <QtWidgets>
#include <QDateTime>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

static QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

VocabularyTransferController::VocabularyTransferController(QWidget *parent, const Callbacks &callbacks)
    : m_parent(parent), m_callbacks(callbacks)
{
}

void VocabularyTransferController::importEntries()
{
    const QString path = QFileDialog::getOpenFileName(
        m_parent,
        tr8("导入词库"),
        QDir::homePath(),
        tr8("词库文件 (*.json *.csv *.txt);;所有文件 (*.*)")
    );
    if (path.isEmpty()) {
        return;
    }

    const QVector<VocabularyEntry> imported = parseVocabularyImportFile(path, scopeOptions());
    if (imported.isEmpty()) {
        showAttentionWarning(m_parent, tr8("导入失败"), tr8("没有从文件里解析到可用词条。"));
        return;
    }

    QVector<VocabularyEntry> entries = loadEntries();
    QSet<QString> existingKeys;
    for (const VocabularyEntry &entry : entries) {
        existingKeys.insert(vocabularyEntryUniqueKey(entry));
    }

    int added = 0;
    int skipped = 0;
    const QVector<VocabularyScopeOption> scopes = scopeOptions();
    for (VocabularyEntry entry : imported) {
        entry.source = entry.source.trimmed();
        entry.target = entry.target.trimmed();
        entry.aliases = entry.aliases.trimmed();
        entry.scopeId = normalizeVocabularyImportScope(entry.scopeId, scopes);
        entry.matchMode = normalizeVocabularyImportMatchMode(entry.matchMode);
        if (entry.source.isEmpty() || entry.target.isEmpty() || !vocabularyEntryHasCorrection(entry)) {
            ++skipped;
            continue;
        }

        const QString key = vocabularyEntryUniqueKey(entry);
        if (existingKeys.contains(key)) {
            ++skipped;
            continue;
        }

        entry.id = nextVocabularyEntryId(entries);
        entries.append(entry);
        existingKeys.insert(key);
        ++added;
    }

    if (added == 0) {
        showAttentionInformation(m_parent, tr8("没有导入"), tr8("文件中的词条为空、重复，或没有修正效果。"));
        return;
    }
    if (!saveEntries(entries)) {
        showAttentionWarning(m_parent, tr8("导入失败"), tr8("无法写入 config/lexicon/entries.json。"));
        return;
    }

    notifyVocabularyChanged();
    showAttentionInformation(
        m_parent,
        tr8("导入完成"),
        tr8("已导入 ") + QString::number(added) + tr8(" 条词条，跳过 ") + QString::number(skipped) + tr8(" 条。")
    );
}

void VocabularyTransferController::exportEntries()
{
    const QVector<VocabularyEntry> entries = filteredEntries();
    if (entries.isEmpty()) {
        showAttentionInformation(m_parent, tr8("没有可导出词条"), tr8("当前词库标签和搜索条件下没有词条。"));
        return;
    }

    QString selectedFilter;
    const QString path = QFileDialog::getSaveFileName(
        m_parent,
        tr8("导出词库"),
        QDir::home().filePath(
            QStringLiteral("vocekit-vocabulary_")
            + exportTimestamp()
            + QStringLiteral(".json")
        ),
        tr8("JSON 文件 (*.json);;CSV 文件 (*.csv);;文本文件 (*.txt)"),
        &selectedFilter
    );
    if (path.isEmpty()) {
        return;
    }

    const QString suffix = QFileInfo(path).suffix().toLower();
    bool ok = false;
    if (suffix == QStringLiteral("csv") || selectedFilter.contains(QStringLiteral("CSV"), Qt::CaseInsensitive)) {
        ok = writeTextFile(path, vocabularyCsvExportText(entries));
    } else if (suffix == QStringLiteral("txt") || selectedFilter.contains(tr8("文本"))) {
        ok = writeTextFile(path, vocabularyPlainExportText(entries, scopeOptions()));
    } else {
        QJsonArray items;
        for (const VocabularyEntry &entry : entries) {
            items.append(vocabularyEntryToJsonObject(entry));
        }
        QJsonObject root;
        root.insert(QStringLiteral("exportedAt"), QDateTime::currentDateTime().toString(Qt::ISODate));
        root.insert(QStringLiteral("entryCount"), entries.size());
        root.insert(QStringLiteral("scope"), currentScopeId());
        root.insert(QStringLiteral("searchText"), searchText());
        root.insert(QStringLiteral("entries"), items);
        ok = writeBytesAtomically(path, QJsonDocument(root).toJson(QJsonDocument::Indented));
    }

    if (!ok) {
        showAttentionWarning(m_parent, tr8("导出失败"), tr8("无法写入词库导出文件。"));
        return;
    }
    showAttentionInformation(m_parent, tr8("导出完成"), tr8("已导出 ") + QString::number(entries.size()) + tr8(" 条词条。"));
}

QVector<VocabularyScopeOption> VocabularyTransferController::scopeOptions() const
{
    return m_callbacks.scopeOptions ? m_callbacks.scopeOptions() : builtinVocabularyScopeOptions();
}

QVector<VocabularyEntry> VocabularyTransferController::filteredEntries() const
{
    return m_callbacks.filteredEntries ? m_callbacks.filteredEntries() : QVector<VocabularyEntry>();
}

QVector<VocabularyEntry> VocabularyTransferController::loadEntries() const
{
    return m_callbacks.loadEntries ? m_callbacks.loadEntries() : QVector<VocabularyEntry>();
}

bool VocabularyTransferController::saveEntries(const QVector<VocabularyEntry> &entries) const
{
    return m_callbacks.saveEntries ? m_callbacks.saveEntries(entries) : false;
}

QString VocabularyTransferController::currentScopeId() const
{
    return m_callbacks.currentScopeId ? m_callbacks.currentScopeId() : QStringLiteral("__all");
}

QString VocabularyTransferController::searchText() const
{
    return m_callbacks.searchText ? m_callbacks.searchText() : QString();
}

void VocabularyTransferController::notifyVocabularyChanged() const
{
    if (m_callbacks.vocabularyChanged) {
        m_callbacks.vocabularyChanged();
    }
}

QString VocabularyTransferController::exportTimestamp()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
}
