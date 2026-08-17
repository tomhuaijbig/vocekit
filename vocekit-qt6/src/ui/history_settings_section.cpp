#include "history_settings_section.h"

#include "history_directory_menu.h"
#include "ui_style.h"

#include <QtWidgets>

namespace {

QString hssTr8(const char *text)
{
    return QString::fromUtf8(text);
}

} // namespace

HistorySettingsSection::HistorySettingsSection(
    const Callbacks &callbacks,
    QWidget *parent
)
    : QWidget(parent),
      m_callbacks(callbacks)
{
    auto *pageLayout = new QVBoxLayout(this);
    pageLayout->setContentsMargins(0, 0, 0, 0);

    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto *holder = new QWidget;
    auto *layout = new QVBoxLayout(holder);
    layout->setContentsMargins(22, 22, 22, 22);
    layout->setSpacing(14);
    layout->addWidget(recordDirectoryRow());
    layout->addWidget(historyLoadCountRow());
    layout->addWidget(logLoadCountRow());
    layout->addStretch();

    scroll->setWidget(holder);
    pageLayout->addWidget(scroll);
}

void HistorySettingsSection::refreshFromSettings()
{
    const HistorySettingsSnapshot current = snapshot();
    if (m_recordDirectoryLabel) {
        m_recordDirectoryLabel->setText(current.recordDirectoryPath);
    }
    if (m_historyInitialLoadBox) {
        const QSignalBlocker blocker(m_historyInitialLoadBox);
        m_historyInitialLoadBox->setValue(current.historyInitialLoadCount);
    }
    if (m_historyLoadMoreBox) {
        const QSignalBlocker blocker(m_historyLoadMoreBox);
        m_historyLoadMoreBox->setValue(current.historyLoadMoreCount);
    }
    if (m_logInitialLoadBox) {
        const QSignalBlocker blocker(m_logInitialLoadBox);
        m_logInitialLoadBox->setValue(current.logInitialLoadCount);
    }
    if (m_logLoadMoreBox) {
        const QSignalBlocker blocker(m_logLoadMoreBox);
        m_logLoadMoreBox->setValue(current.logLoadMoreCount);
    }
}

bool HistorySettingsSection::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        auto *widget = qobject_cast<QWidget *>(watched);
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (widget && mouse->button() == Qt::LeftButton && widget->property("settingDetailEnabled").toBool()) {
            if (m_callbacks.showDetail) {
                m_callbacks.showDetail(
                    widget->property("settingDetailTitle").toString(),
                    widget->property("settingDetailText").toString()
                );
            }
            event->accept();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

QWidget *HistorySettingsSection::recordDirectoryRow()
{
    auto *frame = new QFrame;
    frame->setObjectName(QStringLiteral("card"));
    frame->setStyleSheet(cardStyle());
    auto *layout = new QHBoxLayout(frame);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(12);

    auto *labels = new QVBoxLayout;
    auto *name = new QLabel(hssTr8("历史记录保存位置"));
    name->setFont(appFont(11, QFont::DemiBold));

    m_recordDirectoryLabel = new QLabel(snapshot().recordDirectoryPath);
    m_recordDirectoryLabel->setWordWrap(true);
    m_recordDirectoryLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_recordDirectoryLabel->setStyleSheet(QStringLiteral("color: #047857; font-weight: 600;"));

    labels->addWidget(name);
    labels->addWidget(m_recordDirectoryLabel);

    auto *choose = new QPushButton(hssTr8("更改位置"));
    choose->setMinimumSize(92, 34);
    choose->setStyleSheet(buttonStyle(QStringLiteral("#111827")));
    connect(choose, &QPushButton::clicked, this, [this]() {
        if (!m_callbacks.setRecordDirectory) {
            return;
        }
        const QString dir = QFileDialog::getExistingDirectory(
            this,
            hssTr8("选择历史记录保存位置"),
            snapshot().recordDirectoryPath,
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
        );
        if (dir.trimmed().isEmpty()) {
            return;
        }
        m_callbacks.setRecordDirectory(dir);
        refreshFromSettings();
        saveAndRefresh();
    });

    auto *open = new QPushButton(hssTr8("打开目录"));
    open->setMinimumSize(92, 34);
    open->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
    open->setMenu(recordDirectoryOpenMenu(open, this, [this]() {
        return snapshot().recordDirectoryPath;
    }));

    auto *reset = new QPushButton(hssTr8("恢复默认"));
    reset->setMinimumSize(92, 34);
    reset->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
    connect(reset, &QPushButton::clicked, this, [this]() {
        if (!m_callbacks.resetRecordDirectory) {
            return;
        }
        m_callbacks.resetRecordDirectory();
        refreshFromSettings();
        saveAndRefresh();
    });

    layout->addLayout(labels, 1);
    layout->addWidget(choose);
    layout->addWidget(open);
    layout->addWidget(reset);
    attachSettingDetail(frame, hssTr8("历史记录保存位置"), recordDirectoryDetailText());
    return frame;
}

QWidget *HistorySettingsSection::historyLoadCountRow()
{
    auto *frame = new QFrame;
    frame->setObjectName(QStringLiteral("card"));
    frame->setStyleSheet(cardStyle());
    auto *layout = new QHBoxLayout(frame);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(12);

    auto *labels = new QVBoxLayout;
    auto *name = new QLabel(hssTr8("历史记录加载数量"));
    name->setFont(appFont(11, QFont::DemiBold));
    labels->addWidget(name);

    const HistorySettingsSnapshot current = snapshot();
    m_historyInitialLoadBox = new QSpinBox;
    auto *firstBox = m_historyInitialLoadBox;
    firstBox->setRange(5, 200);
    firstBox->setSingleStep(5);
    firstBox->setSuffix(hssTr8(" 条"));
    firstBox->setValue(current.historyInitialLoadCount);
    firstBox->setFixedWidth(110);
    firstBox->setFixedHeight(34);
    firstBox->setStyleSheet(QStringLiteral(
        "QSpinBox { background: #ffffff; border: 1px solid #d0d5dd; border-radius: 6px; padding: 4px 8px; }"
    ));

    m_historyLoadMoreBox = new QSpinBox;
    auto *moreBox = m_historyLoadMoreBox;
    moreBox->setRange(5, 200);
    moreBox->setSingleStep(5);
    moreBox->setSuffix(hssTr8(" 条"));
    moreBox->setValue(current.historyLoadMoreCount);
    moreBox->setFixedWidth(110);
    moreBox->setFixedHeight(34);
    moreBox->setStyleSheet(firstBox->styleSheet());

    auto *controls = new QGridLayout;
    controls->setHorizontalSpacing(10);
    controls->setVerticalSpacing(8);
    auto *firstLabel = new QLabel(hssTr8("首次加载"));
    firstLabel->setStyleSheet(QStringLiteral("color: #111827;"));
    auto *moreLabel = new QLabel(hssTr8("加载更多"));
    moreLabel->setStyleSheet(QStringLiteral("color: #111827;"));
    controls->addWidget(firstLabel, 0, 0);
    controls->addWidget(firstBox, 0, 1);
    controls->addWidget(moreLabel, 1, 0);
    controls->addWidget(moreBox, 1, 1);

    connect(firstBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [this](int value) {
        if (!m_callbacks.setHistoryInitialLoadCount) {
            return;
        }
        m_callbacks.setHistoryInitialLoadCount(value);
        saveAndRefresh();
    });
    connect(moreBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [this](int value) {
        if (!m_callbacks.setHistoryLoadMoreCount) {
            return;
        }
        m_callbacks.setHistoryLoadMoreCount(value);
        saveAndRefresh();
    });

    layout->addLayout(labels, 1);
    layout->addLayout(controls);
    attachSettingDetail(frame, hssTr8("历史记录加载数量"), historyLoadCountDetailText());
    return frame;
}

QWidget *HistorySettingsSection::logLoadCountRow()
{
    auto *frame = new QFrame;
    frame->setObjectName(QStringLiteral("card"));
    frame->setStyleSheet(cardStyle());
    auto *layout = new QHBoxLayout(frame);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(12);

    auto *labels = new QVBoxLayout;
    auto *name = new QLabel(hssTr8("日志加载数量"));
    name->setFont(appFont(11, QFont::DemiBold));
    labels->addWidget(name);

    const HistorySettingsSnapshot current = snapshot();
    m_logInitialLoadBox = new QSpinBox;
    auto *firstBox = m_logInitialLoadBox;
    firstBox->setRange(5, 500);
    firstBox->setSingleStep(5);
    firstBox->setSuffix(hssTr8(" 条"));
    firstBox->setValue(current.logInitialLoadCount);
    firstBox->setFixedSize(110, 34);
    firstBox->setStyleSheet(QStringLiteral(
        "QSpinBox { background: #ffffff; border: 1px solid #d0d5dd; border-radius: 6px; padding: 4px 8px; }"
    ));

    m_logLoadMoreBox = new QSpinBox;
    auto *moreBox = m_logLoadMoreBox;
    moreBox->setRange(5, 500);
    moreBox->setSingleStep(5);
    moreBox->setSuffix(hssTr8(" 条"));
    moreBox->setValue(current.logLoadMoreCount);
    moreBox->setFixedSize(110, 34);
    moreBox->setStyleSheet(firstBox->styleSheet());

    auto *controls = new QGridLayout;
    controls->setHorizontalSpacing(10);
    controls->setVerticalSpacing(8);
    controls->addWidget(new QLabel(hssTr8("首次加载")), 0, 0);
    controls->addWidget(firstBox, 0, 1);
    controls->addWidget(new QLabel(hssTr8("滚动加载")), 1, 0);
    controls->addWidget(moreBox, 1, 1);

    connect(firstBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [this](int value) {
        if (!m_callbacks.setLogInitialLoadCount) {
            return;
        }
        m_callbacks.setLogInitialLoadCount(value);
        saveAndRefresh();
    });
    connect(moreBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [this](int value) {
        if (!m_callbacks.setLogLoadMoreCount) {
            return;
        }
        m_callbacks.setLogLoadMoreCount(value);
        saveAndRefresh();
    });

    layout->addLayout(labels, 1);
    layout->addLayout(controls);
    attachSettingDetail(frame, hssTr8("日志加载数量"), logLoadCountDetailText());
    return frame;
}

void HistorySettingsSection::attachSettingDetail(QWidget *card, const QString &title, const QString &detail)
{
    installSettingDetailTarget(card, title, detail);
    const QList<QLabel *> labels = card
        ? card->findChildren<QLabel *>(QString(), Qt::FindDirectChildrenOnly)
        : QList<QLabel *>();
    for (QLabel *label : labels) {
        if (label && label->textInteractionFlags() == Qt::NoTextInteraction) {
            installSettingDetailTarget(label, title, detail);
        }
    }
}

void HistorySettingsSection::installSettingDetailTarget(QWidget *target, const QString &title, const QString &detail)
{
    if (!target || detail.trimmed().isEmpty()) {
        return;
    }
    target->setProperty("settingDetailEnabled", true);
    target->setProperty("settingDetailTitle", title);
    target->setProperty("settingDetailText", detail);
    target->setCursor(Qt::PointingHandCursor);
    target->installEventFilter(this);
}

void HistorySettingsSection::saveAndRefresh()
{
    refreshFromSettings();
    if (m_callbacks.saveAndRefresh) {
        m_callbacks.saveAndRefresh();
    }
}

HistorySettingsSnapshot HistorySettingsSection::snapshot() const
{
    return m_callbacks.snapshotProvider
        ? m_callbacks.snapshotProvider()
        : HistorySettingsSnapshot();
}

QString HistorySettingsSection::recordDirectoryDetailText()
{
    return hssTr8("这里决定历史记录根目录。软件会在这个目录下按功能、日期和类型保存文本记录、录音记录、详细 JSON 和备份文件。\n\n更改位置后，新的记录会保存到新目录；打开目录按钮可以选择打开当前保存目录、今天总录音目录、今天总文本目录、今天总详细记录目录或备份目录。");
}

QString HistorySettingsSection::historyLoadCountDetailText()
{
    return hssTr8("首次加载：进入历史记录页面时一次读取多少条记录。数值越大，第一次打开历史记录越慢。\n\n加载更多：点击加载更多时追加读取多少条。建议测试人员机器性能一般时保持较小数量。");
}

QString HistorySettingsSection::logLoadCountDetailText()
{
    return hssTr8("首次加载：进入日志页面或点击刷新时，先显示多少条最新日志。\n\n滚动加载：日志列表滚动到底部时，每次继续显示多少条。这个设置独立于历史记录。");
}
