#include "logs_panel.h"

#include "../runtime_log.h"
#include "app_dialogs.h"
#include "history_row_frame.h"
#include "ui_style.h"

#include <QtWidgets>
#include <QDesktopServices>
#include <QRegularExpression>
#include <QStringConverter>

namespace {

QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

void clearLayout(QLayout *layout)
{
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        if (item->layout()) {
            clearLayout(item->layout());
        }
        delete item;
    }
}

} // namespace

LogsPanel::LogsPanel(
    const LogPaginationSnapshot &pagination,
    QWidget *parent
)
    : QWidget(parent), m_pagination(pagination)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(16);

    auto *top = new QHBoxLayout;
    auto *title = new QLabel(tr8("日志"));
    title->setFont(appFont(24, QFont::DemiBold));
    auto *refresh = new QPushButton(tr8("刷新"));
    refresh->setFixedHeight(38);
    refresh->setStyleSheet(buttonStyle(QStringLiteral("#111827")));
    auto *openFolder = new QPushButton(tr8("打开目录"));
    openFolder->setFixedHeight(38);
    openFolder->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
    top->addWidget(title, 1);
    top->addWidget(refresh);
    top->addWidget(openFolder);
    layout->addLayout(top);

    auto *filterRow = new QHBoxLayout;
    filterRow->setSpacing(10);

    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText(tr8("搜索日志内容、错误、接口、耗时或功能名称"));
    m_searchEdit->setMinimumHeight(40);
    m_searchEdit->setStyleSheet(QStringLiteral(
        "QLineEdit { background: #ffffff; border: 1px solid #d0d5dd; border-radius: 6px; padding: 8px 12px; }"
    ));

    m_filterBox = new QComboBox;
    m_filterBox->setMinimumSize(160, 40);
    m_filterBox->addItem(tr8("全部日志"), QStringLiteral("all"));
    m_filterBox->addItem(tr8("错误与失败"), QStringLiteral("error"));
    m_filterBox->addItem(tr8("接口与网络"), QStringLiteral("interface"));
    m_filterBox->addItem(tr8("快捷键"), QStringLiteral("hotkey"));
    m_filterBox->addItem(tr8("大模型"), QStringLiteral("model"));
    m_filterBox->addItem(tr8("慢请求"), QStringLiteral("slow"));
    m_filterBox->setStyleSheet(QStringLiteral(
        "QComboBox { background: #ffffff; border: 1px solid #d0d5dd; border-radius: 6px; padding: 6px 10px; }"
    ));

    filterRow->addWidget(m_searchEdit, 1);
    filterRow->addWidget(m_filterBox);
    layout->addLayout(filterRow);

    m_scrollArea = new QScrollArea;
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setStyleSheet(QStringLiteral(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollArea > QWidget > QWidget { background: transparent; }"
    ));
    auto *holder = new QWidget;
    m_listLayout = new QVBoxLayout(holder);
    m_listLayout->setContentsMargins(0, 0, 10, 0);
    m_listLayout->setSpacing(10);
    m_listLayout->setAlignment(Qt::AlignTop);
    m_scrollArea->setWidget(holder);
    layout->addWidget(m_scrollArea, 1);

    connect(refresh, &QPushButton::clicked, this, [this]() {
        reload(true);
    });
    connect(openFolder, &QPushButton::clicked, this, []() {
        QDir().mkpath(runtimeLogDirectory());
        QDesktopServices::openUrl(QUrl::fromLocalFile(runtimeLogDirectory()));
    });
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this]() {
        reload(false);
    });
    connect(m_filterBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [this]() {
        reload(false);
    });
    connect(m_scrollArea->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int value) {
        QScrollBar *bar = m_scrollArea ? m_scrollArea->verticalScrollBar() : nullptr;
        if (!bar || m_appending || m_visibleCount >= m_filteredLines.size()) {
            return;
        }
        if (value >= bar->maximum() - 24) {
            appendMoreLogLines(m_pagination.loadMoreCount);
        }
    });
}

void LogsPanel::setPaginationSnapshot(
    const LogPaginationSnapshot &pagination
)
{
    m_pagination = pagination;
}

QStringList LogsPanel::recentRuntimeLogLines(int maxLines) const
{
    QStringList result;
    const bool unlimited = maxLines <= 0;
    QDir dir(runtimeLogDirectory());
    const QFileInfoList files = dir.entryInfoList(QStringList() << QStringLiteral("*.log"), QDir::Files, QDir::Time);
    for (const QFileInfo &fileInfo : files) {
        QFile file(fileInfo.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }
        QTextStream stream(&file);
        stream.setEncoding(QStringConverter::Utf8);
        QStringList lines;
        while (!stream.atEnd()) {
            const QString line = stream.readLine().trimmed();
            if (!line.isEmpty()) {
                lines.append(line);
            }
        }
        for (int i = lines.size() - 1; i >= 0 && (unlimited || result.size() < maxLines); --i) {
            result.append(lines.at(i));
        }
        if (!unlimited && result.size() >= maxLines) {
            break;
        }
    }
    return result;
}

QWidget *LogsPanel::logLineCard(const QString &line)
{
    auto *frame = new HistoryRowFrame;
    frame->setObjectName(QStringLiteral("card"));
    frame->setStyleSheet(cardStyle());
    frame->setCursor(Qt::PointingHandCursor);
    frame->setClickCallback([this, line]() {
        showRuntimeLogDetail(line);
    });

    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(6);

    const QStringList parts = line.split(QStringLiteral(" | "));
    auto *title = new QLabel(parts.size() >= 3 ? (parts.value(1) + QStringLiteral(" / ") + parts.value(2)) : line.left(80));
    title->setFont(appFont(11, QFont::DemiBold));
    title->setWordWrap(true);
    layout->addWidget(title);

    auto *detail = new QLabel(line);
    detail->setWordWrap(true);
    detail->setTextInteractionFlags(Qt::TextSelectableByMouse);
    detail->setStyleSheet(QStringLiteral("color: #4b5563;"));
    layout->addWidget(detail);
    return frame;
}

void LogsPanel::showRuntimeLogDetail(const QString &line)
{
    AppDialog dialog(this);
    dialog.setWindowTitle(tr8("日志详情"));
    dialog.resize(620, 520);
    dialog.setStyleSheet(QStringLiteral("QDialog { background: #f6f7f9; } QLabel { color: #111827; }"));

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(22, 20, 22, 20);
    layout->setSpacing(12);

    auto *title = new QLabel(tr8("日志详情"));
    title->setFont(appFont(20, QFont::DemiBold));
    layout->addWidget(title);

    const QStringList parts = line.split(QStringLiteral(" | "));
    auto addField = [&](const QString &name, const QString &value) {
        auto *row = new QFrame;
        row->setObjectName(QStringLiteral("logDetailRow"));
        row->setStyleSheet(QStringLiteral(
            "QFrame#logDetailRow { background: #ffffff; border: 1px solid #e5e7eb; border-radius: 8px; }"
        ));
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(12, 8, 12, 8);
        rowLayout->setSpacing(12);

        auto *nameLabel = new QLabel(name);
        nameLabel->setMinimumWidth(78);
        nameLabel->setStyleSheet(QStringLiteral("color: #4b5563;"));

        auto *valueLabel = new QLabel(value.trimmed().isEmpty() ? tr8("无") : value);
        valueLabel->setWordWrap(true);
        valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

        rowLayout->addWidget(nameLabel);
        rowLayout->addWidget(valueLabel, 1);
        layout->addWidget(row);
    };

    addField(tr8("时间"), parts.value(0));
    addField(tr8("模块"), parts.value(1));
    addField(tr8("状态"), parts.value(2));
    if (parts.size() > 3 && parts.value(3).startsWith(tr8("耗时="))) {
        addField(tr8("耗时"), parts.value(3).mid(QStringLiteral("耗时=").size()));
    }
    if (parts.size() > 4) {
        addField(tr8("摘要"), parts.mid(4).join(QStringLiteral(" | ")));
    }

    auto *rawTitle = new QLabel(tr8("完整原文"));
    rawTitle->setFont(appFont(12, QFont::DemiBold));
    layout->addWidget(rawTitle);

    auto *raw = new QTextEdit;
    raw->setReadOnly(true);
    raw->setPlainText(line);
    raw->setMinimumHeight(120);
    raw->setStyleSheet(QStringLiteral(
        "QTextEdit { background: #ffffff; border: 1px solid #d0d5dd; border-radius: 8px; padding: 10px; }"
    ));
    layout->addWidget(raw, 1);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    auto *copy = new QPushButton(tr8("复制"));
    copy->setFixedHeight(36);
    copy->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
    auto *close = new QPushButton(tr8("关闭"));
    close->setFixedHeight(36);
    close->setStyleSheet(buttonStyle(QStringLiteral("#111827")));
    buttons->addWidget(copy);
    buttons->addWidget(close);
    layout->addLayout(buttons);

    connect(copy, &QPushButton::clicked, &dialog, [line]() {
        QApplication::clipboard()->setText(line);
    });
    connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);

    dialog.exec();
}

void LogsPanel::appendMoreLogLines(int count)
{
    if (!m_listLayout || m_appending || count <= 0) {
        return;
    }
    m_appending = true;

    if (m_loadHintLabel) {
        m_listLayout->removeWidget(m_loadHintLabel);
        m_loadHintLabel->deleteLater();
        m_loadHintLabel = nullptr;
    }

    const int end = qMin(m_visibleCount + count, m_filteredLines.size());
    for (int i = m_visibleCount; i < end; ++i) {
        m_listLayout->addWidget(logLineCard(m_filteredLines.at(i)));
    }
    m_visibleCount = end;

    if (m_visibleCount < m_filteredLines.size()) {
        m_loadHintLabel = new QLabel(
            tr8("已显示")
            + QString::number(m_visibleCount)
            + tr8(" / ")
            + QString::number(m_filteredLines.size())
            + tr8(" 条，向下滚动继续加载")
        );
        m_loadHintLabel->setAlignment(Qt::AlignCenter);
        m_loadHintLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: #667085; padding: 12px; }"
        ));
        m_listLayout->addWidget(m_loadHintLabel);
    }

    m_appending = false;
}

bool LogsPanel::logMatchesSelectedFilter(const QString &line) const
{
    const QString filter = m_filterBox ? m_filterBox->currentData().toString() : QStringLiteral("all");
    if (filter.isEmpty() || filter == QStringLiteral("all")) {
        return true;
    }
    if (filter == QStringLiteral("error")) {
        return line.contains(tr8("失败"), Qt::CaseInsensitive)
            || line.contains(tr8("错误"), Qt::CaseInsensitive)
            || line.contains(tr8("超时"), Qt::CaseInsensitive)
            || line.contains(tr8("异常"), Qt::CaseInsensitive)
            || line.contains(tr8("崩溃"), Qt::CaseInsensitive);
    }
    if (filter == QStringLiteral("hotkey")) {
        return line.contains(tr8("快捷键"), Qt::CaseInsensitive)
            || line.contains(QStringLiteral("RegisterHotKey"), Qt::CaseInsensitive);
    }
    if (filter == QStringLiteral("model")) {
        return line.contains(tr8("大模型"), Qt::CaseInsensitive)
            || line.contains(tr8("模型="), Qt::CaseInsensitive)
            || line.contains(QStringLiteral("DeepSeek"), Qt::CaseInsensitive)
            || line.contains(QStringLiteral("OpenAI"), Qt::CaseInsensitive)
            || line.contains(QStringLiteral("Claude"), Qt::CaseInsensitive);
    }
    if (filter == QStringLiteral("interface")) {
        return line.contains(tr8("接口"), Qt::CaseInsensitive)
            || line.contains(tr8("网络"), Qt::CaseInsensitive)
            || line.contains(tr8("语音识别"), Qt::CaseInsensitive)
            || line.contains(QStringLiteral("DNS"), Qt::CaseInsensitive)
            || line.contains(QStringLiteral("HTTP"), Qt::CaseInsensitive);
    }
    if (filter == QStringLiteral("slow")) {
        const QRegularExpression elapsedExpression(
            QStringLiteral("耗时=(\\d+)ms")
        );
        const QRegularExpressionMatch match = elapsedExpression.match(line);
        return match.hasMatch() && match.captured(1).toLongLong() >= 1000;
    }
    return true;
}

void LogsPanel::clearList()
{
    if (m_listLayout) {
        clearLayout(m_listLayout);
    }
}

void LogsPanel::reload(bool reloadFromDisk)
{
    if (!m_listLayout) {
        return;
    }
    if (reloadFromDisk || m_linesCache.isEmpty()) {
        m_linesCache = recentRuntimeLogLines();
    }

    clearList();
    m_emptyLabel = nullptr;
    m_loadHintLabel = nullptr;
    m_filteredLines.clear();
    m_visibleCount = 0;

    const QString keyword = m_searchEdit ? m_searchEdit->text().trimmed() : QString();
    for (const QString &line : m_linesCache) {
        if (!keyword.isEmpty() && !line.contains(keyword, Qt::CaseInsensitive)) {
            continue;
        }
        if (!logMatchesSelectedFilter(line)) {
            continue;
        }
        m_filteredLines.append(line);
    }

    if (m_filteredLines.isEmpty()) {
        m_emptyLabel = new QLabel(keyword.isEmpty() ? tr8("暂无日志。") : tr8("没有找到匹配的日志。"));
        m_emptyLabel->setAlignment(Qt::AlignCenter);
        m_emptyLabel->setWordWrap(true);
        m_emptyLabel->setStyleSheet(QStringLiteral(
            "QLabel { background: #f2f4f7; color: #667085; border-radius: 8px; padding: 18px; }"
        ));
        m_listLayout->addWidget(m_emptyLabel);
    } else {
        appendMoreLogLines(m_pagination.initialLoadCount);
    }

    if (m_scrollArea && m_scrollArea->verticalScrollBar()) {
        m_scrollArea->verticalScrollBar()->setValue(0);
    }
}
