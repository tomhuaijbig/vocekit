#include "current_status_panel.h"

#include "ui_style.h"

#include "../config/app_settings_defaults.h"

#include <QtWidgets>

namespace {

QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

QString enabledText(bool enabled)
{
    return enabled ? tr8("已开启") : tr8("已关闭");
}

} // namespace

CurrentStatusPanel::CurrentStatusPanel(
    const CurrentStatusSnapshot &snapshot,
    QWidget *parent
)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("card"));
    setStyleSheet(cardStyle());

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(12);

    auto *title = new QLabel(tr8("当前状态"));
    title->setFont(appFont(15, QFont::DemiBold));
    title->setStyleSheet(
        QStringLiteral("background: transparent; color: #111827;")
    );
    layout->addWidget(title);

    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setFocusPolicy(Qt::WheelFocus);
    scroll->setStyleSheet(QStringLiteral(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollArea > QWidget > QWidget { background: transparent; }"
    ));

    auto *holder = new QWidget;
    auto *items = new QVBoxLayout(holder);
    items->setContentsMargins(0, 0, 5, 0);
    items->setSpacing(10);

    items->addWidget(statusRow(
        QStringLiteral("trayResident"),
        tr8("托盘常驻"),
        enabledText(snapshot.trayResident)
    ));
    items->addWidget(statusRow(
        QStringLiteral("autoStart"),
        tr8("开机自启动"),
        enabledText(snapshot.autoStartEnabled)
    ));
    items->addWidget(statusRow(
        QStringLiteral("strongSelection"),
        tr8("强力选中"),
        enabledText(snapshot.strongSelectionEnabled)
    ));
    items->addWidget(statusRow(
        QStringLiteral("vocabularyEnabled"),
        tr8("词库"),
        enabledText(snapshot.vocabularyEnabled)
    ));
    items->addWidget(statusRow(
        QStringLiteral("vocabularyAddMode"),
        tr8("加入方式"),
        vocabularyAddModeTitle(snapshot.vocabularyAddMode)
    ));
    items->addWidget(statusRow(
        QStringLiteral("dictatePolish"),
        tr8("听写整理"),
        enabledText(snapshot.dictatePolishEnabled)
    ));
    items->addWidget(statusRow(
        QStringLiteral("speechProvider"),
        tr8("语音识别"),
        speechProviderTitle(snapshot.speechProvider)
    ));
    items->addWidget(statusRow(
        QStringLiteral("networkProxy"),
        tr8("网络代理"),
        snapshot.useSystemProxy ? tr8("系统代理") : tr8("直连")
    ));
    items->addWidget(statusRow(
        QStringLiteral("floatingBar"),
        tr8("浮动条"),
        snapshot.floatingBarEnabled
            ? tr8("语音时显示")
            : tr8("已关闭")
    ));
    items->addWidget(statusRow(
        QStringLiteral("recordDirectory"),
        tr8("历史保存"),
        snapshot.usesDefaultRecordDirectory
            ? tr8("默认位置")
            : tr8("自定义位置")
    ));
    addConditionalRows(items, snapshot);
    items->addStretch();

    scroll->setWidget(holder);
    layout->addWidget(scroll, 1);
}

QWidget *CurrentStatusPanel::statusRow(
    const QString &id,
    const QString &name,
    const QString &value
)
{
    auto *row = new QWidget;
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *left = new QLabel(name);
    auto *right = new QLabel(value);
    right->setStyleSheet(
        QStringLiteral("color: #047857; font-weight: 600;")
    );

    if (!id.isEmpty()) {
        m_statusRows.insert(id, row);
        m_statusValueLabels.insert(id, right);
    }
    layout->addWidget(left);
    layout->addStretch();
    layout->addWidget(right);
    return row;
}

void CurrentStatusPanel::setValue(
    const QString &id,
    const QString &value
)
{
    if (m_statusValueLabels.contains(id)) {
        m_statusValueLabels.value(id)->setText(value);
    }
}

void CurrentStatusPanel::setRowVisible(const QString &id, bool visible)
{
    if (m_statusRows.contains(id)) {
        m_statusRows.value(id)->setVisible(visible);
    }
}

void CurrentStatusPanel::addConditionalRows(
    QVBoxLayout *items,
    const CurrentStatusSnapshot &snapshot
)
{
    items->addWidget(statusRow(
        QStringLiteral("holdToTalk"),
        tr8("按住说话"),
        tr8("%1 个功能").arg(snapshot.holdToTalkFunctionCount)
    ));
    items->addWidget(statusRow(
        QStringLiteral("longRecording"),
        tr8("长录音"),
        tr8("%1 个功能").arg(snapshot.longRecordingFunctionCount)
    ));
    setRowVisible(
        QStringLiteral("holdToTalk"),
        snapshot.holdToTalkFunctionCount > 0
    );
    setRowVisible(
        QStringLiteral("longRecording"),
        snapshot.longRecordingFunctionCount > 0
    );
}

void CurrentStatusPanel::refresh(const CurrentStatusSnapshot &snapshot)
{
    setValue(QStringLiteral("trayResident"), enabledText(snapshot.trayResident));
    setValue(QStringLiteral("autoStart"), enabledText(snapshot.autoStartEnabled));
    setValue(QStringLiteral("strongSelection"), enabledText(snapshot.strongSelectionEnabled));
    setValue(QStringLiteral("vocabularyEnabled"), enabledText(snapshot.vocabularyEnabled));
    setValue(
        QStringLiteral("vocabularyAddMode"),
        vocabularyAddModeTitle(snapshot.vocabularyAddMode)
    );
    setValue(
        QStringLiteral("floatingBar"),
        snapshot.floatingBarEnabled
            ? tr8("语音时显示")
            : tr8("已关闭")
    );
    setValue(QStringLiteral("dictatePolish"), enabledText(snapshot.dictatePolishEnabled));
    setValue(
        QStringLiteral("networkProxy"),
        snapshot.useSystemProxy ? tr8("系统代理") : tr8("直连")
    );
    setValue(
        QStringLiteral("speechProvider"),
        speechProviderTitle(snapshot.speechProvider)
    );
    setValue(
        QStringLiteral("recordDirectory"),
        snapshot.usesDefaultRecordDirectory
            ? tr8("默认位置")
            : tr8("自定义位置")
    );
    setValue(
        QStringLiteral("holdToTalk"),
        tr8("%1 个功能").arg(snapshot.holdToTalkFunctionCount)
    );
    setRowVisible(
        QStringLiteral("holdToTalk"),
        snapshot.holdToTalkFunctionCount > 0
    );
    setValue(
        QStringLiteral("longRecording"),
        tr8("%1 个功能").arg(snapshot.longRecordingFunctionCount)
    );
    setRowVisible(
        QStringLiteral("longRecording"),
        snapshot.longRecordingFunctionCount > 0
    );
}
