#include "history_transfer_controller.h"

#include "../storage/history_archive.h"
#include "../storage/history_export.h"
#include "../ui/attention_message.h"

#include <QtWidgets>

static QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

HistoryTransferController::HistoryTransferController(QWidget *parent, const Callbacks &callbacks)
    : m_parent(parent), m_callbacks(callbacks)
{
}

void HistoryTransferController::backupHistoryRecords()
{
    const HistoryBackupResult result = backupHistoryRecordsToDirectory(
        recordDirectoryPath(),
        exportTimestamp()
    );
    if (!result.ok) {
        showAttentionWarning(m_parent, tr8("备份失败"), result.error.isEmpty() ? tr8("无法复制历史记录文件。") : result.error);
        return;
    }

    showAttentionInformation(
        m_parent,
        tr8("备份完成"),
        tr8("已备份 ") + QString::number(result.fileCount) + tr8(" 个文件到：\n") + result.targetPath
    );
}

void HistoryTransferController::importHistoryRecords()
{
    const QString selectedPath = QFileDialog::getExistingDirectory(
        m_parent,
        tr8("选择要导入的历史记录目录"),
        QDir::homePath(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );
    if (selectedPath.isEmpty()) {
        return;
    }

    const HistoryImportResult result = importHistoryRecordsFromDirectory(
        selectedPath,
        recordDirectoryPath()
    );
    if (!result.ok) {
        showAttentionWarning(m_parent, tr8("导入失败"), result.error.isEmpty() ? tr8("无法导入历史记录文件。") : result.error);
        return;
    }

    if (m_callbacks.historyChanged) {
        m_callbacks.historyChanged();
    }

    showAttentionInformation(
        m_parent,
        tr8("导入完成"),
        tr8("已导入 ") + QString::number(result.fileCount) + tr8(" 个文件。")
    );
}

void HistoryTransferController::exportHistoryText()
{
    QVector<HistoryEntry> entries;
    if (!selectedHistoryEntriesForExport(&entries)) {
        return;
    }

    const QString path = QFileDialog::getSaveFileName(
        m_parent,
        tr8("导出文本记录"),
        QDir::home().filePath(QStringLiteral("vocekit-history-text_") + exportTimestamp() + QStringLiteral(".txt")),
        tr8("文本文件 (*.txt)")
    );
    if (path.isEmpty()) {
        return;
    }

    if (!writeHistoryTextExport(entries, path)) {
        showAttentionWarning(m_parent, tr8("导出失败"), tr8("无法写入文本导出文件。"));
        return;
    }

    showAttentionInformation(
        m_parent,
        tr8("导出完成"),
        tr8("已导出 ") + QString::number(entries.size()) + tr8(" 条文本记录。")
    );
}

void HistoryTransferController::exportHistoryDetails()
{
    QVector<HistoryEntry> entries;
    if (!selectedHistoryEntriesForExport(&entries)) {
        return;
    }

    const QString path = QFileDialog::getSaveFileName(
        m_parent,
        tr8("导出详细记录"),
        QDir::home().filePath(QStringLiteral("vocekit-history-details_") + exportTimestamp() + QStringLiteral(".json")),
        tr8("JSON 文件 (*.json)")
    );
    if (path.isEmpty()) {
        return;
    }

    if (!writeHistoryDetailsExport(entries, path)) {
        showAttentionWarning(m_parent, tr8("导出失败"), tr8("无法写入详细记录文件。"));
        return;
    }

    showAttentionInformation(
        m_parent,
        tr8("导出完成"),
        tr8("已导出 ") + QString::number(entries.size()) + tr8(" 条详细记录。")
    );
}

void HistoryTransferController::exportHistoryAudio()
{
    QVector<HistoryEntry> entries;
    if (!selectedHistoryEntriesForExport(&entries)) {
        return;
    }

    const QString parentPath = QFileDialog::getExistingDirectory(
        m_parent,
        tr8("选择录音导出位置"),
        QDir::homePath(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );
    if (parentPath.isEmpty()) {
        return;
    }

    const QString targetPath = QDir(parentPath).filePath(QStringLiteral("vocekit-audio-export_") + exportTimestamp());
    int exported = 0;
    QString error;
    if (!exportHistoryAudioFiles(entries, targetPath, &exported, &error)) {
        showAttentionWarning(m_parent, tr8("导出失败"), error.isEmpty() ? tr8("无法复制录音文件。") : error);
        return;
    }

    if (exported == 0) {
        showAttentionInformation(m_parent, tr8("没有录音可导出"), tr8("当前筛选结果里没有存在于本地的录音文件。"));
        return;
    }

    showAttentionInformation(
        m_parent,
        tr8("导出完成"),
        tr8("已导出 ") + QString::number(exported) + tr8(" 个录音文件到：\n") + targetPath
    );
}

void HistoryTransferController::exportHistoryAll()
{
    QVector<HistoryEntry> entries;
    if (!selectedHistoryEntriesForExport(&entries)) {
        return;
    }

    const QString parentPath = QFileDialog::getExistingDirectory(
        m_parent,
        tr8("选择全部导出位置"),
        QDir::homePath(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );
    if (parentPath.isEmpty()) {
        return;
    }

    const QString targetPath = QDir(parentPath).filePath(QStringLiteral("vocekit-history-export_") + exportTimestamp());
    QDir target(targetPath);
    if (!target.exists() && !target.mkpath(QStringLiteral("."))) {
        showAttentionWarning(m_parent, tr8("导出失败"), tr8("无法创建全部导出目录。"));
        return;
    }

    const QString textPath = target.filePath(tr8("文本记录.txt"));
    const QString detailsPath = target.filePath(tr8("详细记录.json"));
    const QString audioPath = target.filePath(tr8("录音"));
    if (!writeHistoryTextExport(entries, textPath)) {
        showAttentionWarning(m_parent, tr8("导出失败"), tr8("无法写入文本导出文件。"));
        return;
    }
    if (!writeHistoryDetailsExport(entries, detailsPath)) {
        showAttentionWarning(m_parent, tr8("导出失败"), tr8("无法写入详细记录文件。"));
        return;
    }

    int exportedAudio = 0;
    QString error;
    if (!exportHistoryAudioFiles(entries, audioPath, &exportedAudio, &error)) {
        showAttentionWarning(m_parent, tr8("导出失败"), error.isEmpty() ? tr8("无法复制录音文件。") : error);
        return;
    }

    showAttentionInformation(
        m_parent,
        tr8("导出完成"),
        tr8("已导出 ") + QString::number(entries.size()) + tr8(" 条记录，录音 ")
            + QString::number(exportedAudio) + tr8(" 个。\n") + targetPath
    );
}

bool HistoryTransferController::selectedHistoryEntriesForExport(QVector<HistoryEntry> *entries) const
{
    if (!entries) {
        return false;
    }

    const QVector<HistoryEntry> filtered = m_callbacks.filteredEntries
        ? m_callbacks.filteredEntries()
        : QVector<HistoryEntry>();
    if (filtered.isEmpty()) {
        showAttentionInformation(m_parent, tr8("没有可导出记录"), tr8("当前筛选条件下没有历史记录。"));
        entries->clear();
        return false;
    }

    const QVector<HistoryEntry> selected = m_callbacks.selectedEntries
        ? m_callbacks.selectedEntries()
        : QVector<HistoryEntry>();
    if (selected.isEmpty()) {
        showAttentionInformation(
            m_parent,
            tr8("没有选中记录"),
            tr8("请先点击“选择记录”，勾选要导出的历史记录；也可以点击“全选当前”导出当前筛选结果。")
        );
        entries->clear();
        return false;
    }

    *entries = selected;
    return true;
}

bool HistoryTransferController::writeHistoryTextExport(const QVector<HistoryEntry> &entries, const QString &path) const
{
    return writeHistoryTextExportFile(
        entries,
        path,
        [this](const HistoryEntry &entry) {
            return m_callbacks.detailPlainText ? m_callbacks.detailPlainText(entry) : QString();
        }
    );
}

bool HistoryTransferController::writeHistoryDetailsExport(const QVector<HistoryEntry> &entries, const QString &path) const
{
    HistoryExportOptions options;
    options.filterMode = currentModeId();
    options.searchText = searchText();
    options.selectionOnly = true;
    return writeHistoryDetailsExportFile(
        entries,
        path,
        options,
        [this](const HistoryEntry &entry) {
            return m_callbacks.entryJson ? m_callbacks.entryJson(entry) : QJsonObject();
        }
    );
}

bool HistoryTransferController::exportHistoryAudioFiles(const QVector<HistoryEntry> &entries, const QString &targetPath, int *exported, QString *error) const
{
    if (exported) {
        *exported = 0;
    }
    if (error) {
        error->clear();
    }

    const HistoryAudioExportResult result = exportHistoryAudioFilesToDirectory(entries, targetPath);
    if (exported) {
        *exported = result.exported;
    }
    if (error) {
        *error = result.error;
    }
    return result.ok;
}

QString HistoryTransferController::recordDirectoryPath() const
{
    return m_callbacks.recordDirectoryPath ? m_callbacks.recordDirectoryPath() : QString();
}

QString HistoryTransferController::currentModeId() const
{
    return m_callbacks.currentModeId ? m_callbacks.currentModeId() : QStringLiteral("__all");
}

QString HistoryTransferController::searchText() const
{
    return m_callbacks.searchText ? m_callbacks.searchText() : QString();
}

QString HistoryTransferController::exportTimestamp()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
}
