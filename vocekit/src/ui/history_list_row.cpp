#include "history_list_row.h"

#include "history_row_frame.h"
#include "ui_style.h"

#include <QtWidgets>

static QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

static QPushButton *createHistoryMenuButton(
    const HistoryEntry &entry,
    const HistoryRowCallbacks &callbacks,
    QWidget *context
)
{
    auto *button = new QPushButton(tr8("操作"));
    button->setFixedSize(68, 34);
    button->setFont(appFont(9, QFont::DemiBold));
    button->setCursor(Qt::PointingHandCursor);
    button->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background: #ffffff;"
        "  color: #111827;"
        "  border: 1px solid #d0d5dd;"
        "  border-radius: 6px;"
        "  padding: 0;"
        "}"
        "QPushButton:hover { background: #f2f4f7; }"
        "QPushButton::menu-indicator { width: 0; image: none; }"
    ));

    auto *menu = new QMenu(button);
    menu->setStyleSheet(QStringLiteral(
        "QMenu { background: #ffffff; border: 1px solid #d0d5dd; padding: 6px; }"
        "QMenu::item { padding: 8px 28px 8px 12px; color: #111827; }"
        "QMenu::item:selected { background: #eef2ff; }"
    ));

    QAction *detail = menu->addAction(tr8("查看详情"));
    QAction *play = menu->addAction(tr8("播放录音"));
    QAction *copy = menu->addAction(tr8("复制内容"));
    QAction *favorite = menu->addAction(entry.favorite ? tr8("取消收藏") : tr8("收藏"));
    QMenu *folderMenu = menu->addMenu(tr8("加入收藏夹"));
    QAction *newFolder = folderMenu->addAction(tr8("新建收藏夹..."));
    const QStringList folders = callbacks.favoriteFolders ? callbacks.favoriteFolders() : QStringList();
    if (!folders.isEmpty()) {
        folderMenu->addSeparator();
    }
    for (const QString &folder : folders) {
        QAction *folderAction = folderMenu->addAction(folder);
        QObject::connect(folderAction, &QAction::triggered, context, [callbacks, entry, folder]() {
            if (callbacks.setFavoriteFolder) {
                callbacks.setFavoriteFolder(entry.filePath, folder);
            }
        });
    }
    QAction *remove = menu->addAction(tr8("删除"));

    QObject::connect(detail, &QAction::triggered, context, [callbacks, entry]() {
        if (callbacks.openDetail) {
            callbacks.openDetail(entry);
        }
    });
    QObject::connect(play, &QAction::triggered, context, [callbacks, entry]() {
        if (callbacks.playAudio) {
            callbacks.playAudio(entry);
        }
    });
    QObject::connect(copy, &QAction::triggered, context, [callbacks, entry]() {
        if (callbacks.copyContent) {
            callbacks.copyContent(entry);
        }
    });
    QObject::connect(favorite, &QAction::triggered, context, [callbacks, entry]() {
        if (callbacks.toggleFavorite) {
            callbacks.toggleFavorite(entry);
        }
    });
    QObject::connect(newFolder, &QAction::triggered, context, [callbacks, entry]() {
        if (!callbacks.createFavoriteFolder || !callbacks.setFavoriteFolder) {
            return;
        }
        const QString folder = callbacks.createFavoriteFolder();
        if (!folder.isEmpty()) {
            callbacks.setFavoriteFolder(entry.filePath, folder);
        }
    });
    QObject::connect(remove, &QAction::triggered, context, [callbacks, entry]() {
        if (callbacks.deleteEntry) {
            callbacks.deleteEntry(entry);
        }
    });

    button->setMenu(menu);
    return button;
}

QWidget *createHistoryRowWidget(
    const HistoryEntry &entry,
    QListWidget *list,
    bool batchMode,
    const HistoryRowCallbacks &callbacks,
    QWidget *parent
)
{
    auto *row = new HistoryRowFrame(parent);
    row->setObjectName(QStringLiteral("historyRow"));
    row->setClickCallback([callbacks, entry, batchMode]() {
        if (!batchMode && callbacks.openDetail) {
            callbacks.openDetail(entry);
        }
    });

    const int viewportWidth = list && list->viewport() ? list->viewport()->width() : 0;
    const int rowHeight = callbacks.rowHeight ? callbacks.rowHeight(entry, viewportWidth) : 140;
    row->setMinimumHeight(rowHeight);

    auto *layout = new QVBoxLayout(row);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(10);

    auto *title = new QLabel(callbacks.titleText ? callbacks.titleText(entry) : entry.mode);
    title->setFont(appFont(10, QFont::DemiBold));
    title->setStyleSheet(QStringLiteral("color: #111827;"));
    title->setWordWrap(true);
    title->setMinimumHeight(30);
    title->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    title->setAttribute(Qt::WA_TransparentForMouseEvents);

    auto *preview = new QLabel(callbacks.previewText ? callbacks.previewText(entry) : entry.output);
    preview->setFont(appFont(10));
    preview->setStyleSheet(QStringLiteral("color: #667085;"));
    preview->setWordWrap(true);
    preview->setMinimumHeight(qMax(78, rowHeight - 70));
    preview->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    preview->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    preview->setAttribute(Qt::WA_TransparentForMouseEvents);

    auto *header = new QHBoxLayout;
    header->setContentsMargins(0, 0, 0, 0);
    header->setSpacing(10);
    if (batchMode) {
        auto *select = new QCheckBox;
        select->setFixedSize(28, 30);
        select->setChecked(callbacks.isSelected ? callbacks.isSelected(entry) : false);
        select->setToolTip(tr8("选择这条记录"));
        QObject::connect(select, &QCheckBox::toggled, row, [callbacks, entry](bool checked) {
            if (callbacks.setSelected) {
                callbacks.setSelected(entry, checked);
            }
        });
        header->addWidget(select, 0, Qt::AlignTop);
    }
    header->addWidget(title, 1);
    header->addWidget(createHistoryMenuButton(entry, callbacks, row), 0, Qt::AlignTop);

    layout->addLayout(header);
    layout->addWidget(preview, 1);

    row->setStyleSheet(QStringLiteral(
        "QWidget#historyRow { background: #f9fafb; border: 1px solid #eef0f4; border-radius: 6px; }"
        "QLabel { border: none; background: transparent; }"
    ));

    return row;
}
