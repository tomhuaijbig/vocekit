#include "history_entry_actions_controller.h"

#include "../storage/history_paths.h"
#include "../storage/history_record_service.h"
#include "../storage/history_store.h"
#include "../ui/app_dialogs.h"
#include "../ui/attention_message.h"
#include "../ui/ui_style.h"

#include <QtWidgets>
#include <QDesktopServices>

static QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

HistoryEntryActionsController::HistoryEntryActionsController(QWidget *parent, const Callbacks &callbacks)
    : m_parent(parent), m_callbacks(callbacks)
{
}

void HistoryEntryActionsController::deleteSelectedHistoryEntries()
{
    const bool hasSelected = m_callbacks.hasSelectedEntries
        ? m_callbacks.hasSelectedEntries()
        : false;
    if (!hasSelected) {
        showAttentionInformation(m_parent, tr8("没有选中记录"), tr8("请先勾选要删除的历史记录。"));
        return;
    }

    const int count = m_callbacks.selectedCount ? m_callbacks.selectedCount() : 0;
    if (QMessageBox::question(
            m_parent,
            tr8("批量删除"),
            tr8("确定删除选中的 ") + QString::number(count) + tr8(" 条历史记录和对应录音吗？")
        ) != QMessageBox::Yes) {
        return;
    }

    const QStringList files = m_callbacks.selectedFilePaths
        ? m_callbacks.selectedFilePaths()
        : QStringList();
    const HistoryDeleteResult removed =
        HistoryRecordService(historyRootPath(recordDirectoryPath()))
            .removeEntries(files);

    if (m_callbacks.clearSelection) {
        m_callbacks.clearSelection();
    }
    if (m_callbacks.updateBatchButtons) {
        m_callbacks.updateBatchButtons();
    }
    if (!removed.ok) {
        showAttentionWarning(m_parent, tr8("删除失败"), tr8("部分文件无法删除，请检查文件是否正在被占用。"));
    }
    notifyHistoryChangedAfterMutation(files);
}

QString HistoryEntryActionsController::createFavoriteFolderDialog()
{
    AppDialog dialog(m_parent);
    dialog.setWindowTitle(tr8("新建收藏夹"));
    dialog.setModal(true);
    dialog.setMinimumSize(420, 190);
    dialog.setStyleSheet(QStringLiteral("QDialog { background: #f6f7f9; } QLabel { color: #111827; }"));

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(12);

    auto *title = new QLabel(tr8("收藏夹名称"));
    title->setFont(appFont(13, QFont::DemiBold));
    auto *input = new QLineEdit;
    input->setMinimumHeight(40);
    input->setPlaceholderText(tr8("例如：常用翻译、重要问答"));
    input->setStyleSheet(QStringLiteral(
        "QLineEdit { background: #ffffff; border: 1px solid #d0d5dd; border-radius: 8px; padding: 0 12px; }"
    ));

    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    auto *cancel = new QPushButton(tr8("取消"));
    cancel->setFont(appFont(10, QFont::DemiBold));
    cancel->setFixedSize(86, 42);
    cancel->setStyleSheet(compactButtonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
    auto *create = new QPushButton(tr8("创建"));
    create->setFont(appFont(10, QFont::DemiBold));
    create->setFixedSize(86, 42);
    create->setStyleSheet(compactButtonStyle(QStringLiteral("#111827")));
    buttons->addWidget(cancel);
    buttons->addWidget(create);

    layout->addWidget(title);
    layout->addWidget(input);
    layout->addStretch();
    layout->addLayout(buttons);

    QObject::connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(create, &QPushButton::clicked, &dialog, [&dialog]() {
        dialog.accept();
    });

    if (dialog.exec() != QDialog::Accepted) {
        return QString();
    }

    const QString name = input->text().trimmed();
    if (name.isEmpty()) {
        showAttentionWarning(m_parent, tr8("名称不能为空"), tr8("收藏夹必须填写名称。"));
        return QString();
    }

    const bool added = m_callbacks.addFavoriteFolder
        ? m_callbacks.addFavoriteFolder(name)
        : false;
    if (!added) {
        showAttentionWarning(m_parent, tr8("无法创建"), tr8("收藏夹名称为空或已经存在。"));
        return QString();
    }

    const bool saved = m_callbacks.saveSettings ? m_callbacks.saveSettings() : false;
    if (!saved) {
        showAttentionWarning(m_parent, tr8("保存失败"), tr8("无法写入 config/settings.json。"));
        return QString();
    }

    return name;
}

void HistoryEntryActionsController::playHistoryAudio(const HistoryEntry &entry)
{
    QString audioPath = entry.audio.trimmed();
    if ((audioPath.isEmpty() || !QFileInfo::exists(audioPath)) && QFileInfo::exists(entry.allAudioFile)) {
        audioPath = entry.allAudioFile;
    }
    if (audioPath.isEmpty() || !QFileInfo::exists(audioPath)) {
        showAttentionWarning(m_parent, tr8("无法播放"), tr8("这条记录没有可播放的录音文件。"));
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(audioPath));
}

void HistoryEntryActionsController::toggleHistoryFavorite(const QString &filePath)
{
    if (filePath.trimmed().isEmpty()) {
        return;
    }

    const HistoryEntry entry = HistoryStore::entryFromFile(filePath);
    if (!HistoryRecordService(historyRootPath(recordDirectoryPath()))
             .updateFavorite(filePath, !entry.favorite).ok) {
        showAttentionWarning(m_parent, tr8("保存失败"), tr8("无法更新收藏状态。"));
        return;
    }
    notifyHistoryChangedAfterMutation(QStringList() << filePath);
}

void HistoryEntryActionsController::setHistoryFavoriteFolder(const QString &filePath, const QString &folder)
{
    const QString trimmed = folder.trimmed();
    if (filePath.trimmed().isEmpty() || trimmed.isEmpty()) {
        return;
    }

    if (!favoriteFolders().contains(trimmed)) {
        if (m_callbacks.addFavoriteFolder) {
            m_callbacks.addFavoriteFolder(trimmed);
        }
        if (m_callbacks.saveSettings) {
            m_callbacks.saveSettings();
        }
    }

    if (!HistoryRecordService(historyRootPath(recordDirectoryPath()))
             .updateFavorite(filePath, true, trimmed).ok) {
        showAttentionWarning(m_parent, tr8("保存失败"), tr8("无法更新收藏夹。"));
        return;
    }
    notifyHistoryChangedAfterMutation(QStringList() << filePath);
}

void HistoryEntryActionsController::deleteHistoryEntry(const HistoryEntry &entry)
{
    if (entry.filePath.trimmed().isEmpty()) {
        return;
    }

    if (QMessageBox::question(
            m_parent,
            tr8("删除历史记录"),
            tr8("确定删除这条历史记录和对应录音吗？")
        ) != QMessageBox::Yes) {
        return;
    }

    const HistoryDeleteResult removed =
        HistoryRecordService(historyRootPath(recordDirectoryPath()))
            .removeEntry(entry);
    if (!removed.ok) {
        showAttentionWarning(m_parent, tr8("删除失败"), tr8("部分文件无法删除，请检查文件是否正在被占用。"));
    }
    notifyHistoryChangedAfterMutation(QStringList() << entry.filePath);
}

void HistoryEntryActionsController::notifyHistoryChangedAfterMutation(
    const QStringList &recordIds,
    bool resetRequired
)
{
    if (m_callbacks.historyChanged) {
        m_callbacks.historyChanged(recordIds, resetRequired);
    }
}

QString HistoryEntryActionsController::recordDirectoryPath() const
{
    return m_callbacks.recordDirectoryPath ? m_callbacks.recordDirectoryPath() : QString();
}

QStringList HistoryEntryActionsController::favoriteFolders() const
{
    return m_callbacks.favoriteFolders ? m_callbacks.favoriteFolders() : QStringList();
}
