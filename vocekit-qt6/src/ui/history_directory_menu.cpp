#include "history_directory_menu.h"

#include "../config/app_paths.h"
#include "../storage/history_paths.h"

#include <QAction>
#include <QDate>
#include <QWidget>

namespace {

QString menuText(const char *text)
{
    return QString::fromUtf8(text);
}

} // namespace

QMenu *recordDirectoryOpenMenu(
    QWidget *parent,
    QObject *receiver,
    const std::function<QString()> &recordDirectoryProvider
)
{
    auto *menu = new QMenu(parent);
    menu->setStyleSheet(QStringLiteral(
        "QMenu { background: #ffffff; border: 1px solid #d0d5dd; padding: 6px; }"
        "QMenu::item { padding: 8px 28px 8px 12px; color: #111827; }"
        "QMenu::item:selected { background: #eef2ff; }"
    ));

    QAction *current = menu->addAction(menuText("当前保存目录"));
    QObject::connect(current, &QAction::triggered, receiver, [recordDirectoryProvider]() {
        ensureHistoryRootStructure(recordDirectoryProvider());
        openDirectoryPath(historyRootPath(recordDirectoryProvider()));
    });

    QAction *backup = menu->addAction(menuText("备份文件"));
    QObject::connect(backup, &QAction::triggered, receiver, [recordDirectoryProvider]() {
        ensureHistoryRootStructure(recordDirectoryProvider());
        openDirectoryPath(historyBackupDirectory(recordDirectoryProvider()));
    });

    QAction *todayAudio = menu->addAction(menuText("今天总录音目录"));
    QObject::connect(todayAudio, &QAction::triggered, receiver, [recordDirectoryProvider]() {
        openDirectoryPath(recordDirectoryForDate(recordDirectoryProvider()));
    });

    QAction *todayText = menu->addAction(menuText("今天总文本目录"));
    QObject::connect(todayText, &QAction::triggered, receiver, [recordDirectoryProvider]() {
        openDirectoryPath(historyAllDateDirectory(
            recordDirectoryProvider(),
            historyAllTextFolderName(),
            QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"))
        ));
    });

    QAction *todayDetail = menu->addAction(menuText("今天总详细记录目录"));
    QObject::connect(todayDetail, &QAction::triggered, receiver, [recordDirectoryProvider]() {
        openDirectoryPath(historyAllDateDirectory(
            recordDirectoryProvider(),
            historyAllDetailFolderName(),
            QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"))
        ));
    });

    QAction *defaultDir = menu->addAction(menuText("默认保存目录"));
    QObject::connect(defaultDir, &QAction::triggered, receiver, []() {
        ensureHistoryRootStructure(defaultRecordDirectory());
        openDirectoryPath(defaultRecordDirectory());
    });

    return menu;
}
