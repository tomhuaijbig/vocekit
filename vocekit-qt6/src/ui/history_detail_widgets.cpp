#include "history_detail_widgets.h"

#include "../domain/history_text.h"
#include "ui_style.h"

#include <QtWidgets>

static QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

QFrame *createHistoryDetailMetaCard(
    const HistoryEntry &entry,
    const HistoryDetailMetaTexts &texts,
    QWidget *parent
)
{
    auto *metaCard = new QFrame(parent);
    metaCard->setObjectName(QStringLiteral("card"));
    metaCard->setStyleSheet(cardStyle());
    auto *meta = new QGridLayout(metaCard);
    meta->setContentsMargins(14, 12, 14, 12);
    meta->setHorizontalSpacing(18);
    meta->setVerticalSpacing(10);

    auto addMeta = [meta](int row, int column, const QString &name, const QString &value) {
        auto *nameLabel = new QLabel(name);
        nameLabel->setFont(appFont(9, QFont::DemiBold));
        nameLabel->setStyleSheet(QStringLiteral("color: #667085;"));
        auto *valueLabel = new QLabel(value.trimmed().isEmpty() ? tr8("无") : value);
        valueLabel->setWordWrap(true);
        valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        valueLabel->setFont(appFont(10, QFont::DemiBold));
        valueLabel->setMinimumHeight(26);
        meta->addWidget(nameLabel, row, column * 2);
        meta->addWidget(valueLabel, row, column * 2 + 1);
    };

    const bool audioExists = !entry.audio.trimmed().isEmpty() && QFileInfo::exists(entry.audio);
    const QString audioState = entry.audio.trimmed().isEmpty()
        ? tr8("本次没有录音")
        : (audioExists ? tr8("文件存在") : tr8("文件不存在或已被删除"));

    addMeta(0, 0, tr8("功能"), historyEntryModeText(entry));
    addMeta(0, 1, tr8("时间"), texts.timeText);
    addMeta(1, 0, tr8("总耗时"), texts.elapsedText);
    addMeta(1, 1, tr8("语音识别耗时"), texts.speechElapsedText);
    addMeta(2, 0, tr8("模型耗时"), texts.modelElapsedText);
    addMeta(2, 1, tr8("使用模型"), texts.modelText);

    int nextMetaRow = 3;
    if (entry.modeId == QStringLiteral("ocr")) {
        addMeta(nextMetaRow, 0, tr8("图片识别引擎"), entry.ocrEngine.trimmed().isEmpty() ? tr8("未记录") : entry.ocrEngine);
        addMeta(nextMetaRow, 1, tr8("图片识别耗时"), texts.ocrElapsedText);
        ++nextMetaRow;
        addMeta(nextMetaRow, 0, tr8("识别语言"), entry.ocrLanguages.isEmpty() ? tr8("未记录") : entry.ocrLanguages.join(QStringLiteral("、")));
        addMeta(nextMetaRow, 1, tr8("自动回退"), entry.ocrUsedFallback ? tr8("是") : tr8("否"));
        ++nextMetaRow;
        addMeta(nextMetaRow, 0, tr8("图片文件名"), entry.imageFileName.trimmed().isEmpty() ? tr8("未记录") : entry.imageFileName);
        ++nextMetaRow;
    }

    if (entry.longRecording || !entry.segments.isEmpty()) {
        int failedSegments = 0;
        for (const RecordingSegment &segment : entry.segments) {
            if (segment.text.trimmed().isEmpty()) {
                ++failedSegments;
            }
        }
        const QString triggerMode = entry.recordingTriggerMode == QStringLiteral("hold")
            ? tr8("按住说话")
            : tr8("切换开始和结束");
        addMeta(nextMetaRow, 0, tr8("录音方式"), triggerMode);
        addMeta(
            nextMetaRow,
            1,
            tr8("录音分段"),
            tr8("%1 段，%2 段失败")
                .arg(entry.segments.size())
                .arg(failedSegments)
        );
        ++nextMetaRow;
    }

    if (!entry.flowRunId.trimmed().isEmpty()) {
        addMeta(
            nextMetaRow,
            0,
            tr8("流程运行 ID"),
            entry.flowRunId
        );
        addMeta(
            nextMetaRow,
            1,
            tr8("触发入口"),
            entry.flowTrigger
        );
        ++nextMetaRow;
        addMeta(
            nextMetaRow,
            0,
            tr8("发布版本"),
            QString::number(entry.flowPublishedRevision)
        );
        addMeta(
            nextMetaRow,
            1,
            tr8("节点轨迹"),
            tr8("%1 个节点").arg(entry.flowNodeTraces.size())
        );
        ++nextMetaRow;
        addMeta(
            nextMetaRow,
            0,
            tr8("发布哈希"),
            entry.flowPublishedHash
        );
        addMeta(
            nextMetaRow,
            1,
            tr8("流程状态"),
            entry.flowCancelled
                ? tr8("已取消")
                : (entry.flowFailedNodeId.trimmed().isEmpty()
                    ? tr8("完成")
                    : tr8("失败"))
        );
        ++nextMetaRow;
    }

    addMeta(nextMetaRow, 0, tr8("提示词版本"), entry.promptVersion.trimmed().isEmpty() ? tr8("未记录") : entry.promptVersion);
    addMeta(nextMetaRow, 1, tr8("状态"), entry.draft ? tr8("草稿") : (entry.error.trimmed().isEmpty() ? tr8("完成") : tr8("有错误")));
    addMeta(nextMetaRow + 1, 0, tr8("录音"), audioState);
    return metaCard;
}

QFrame *createHistoryDetailTextSection(
    const QString &sectionTitle,
    const QString &text,
    int minimumHeight,
    QWidget *parent
)
{
    auto *frame = new QFrame(parent);
    frame->setObjectName(QStringLiteral("card"));
    frame->setStyleSheet(cardStyle());
    auto *frameLayout = new QVBoxLayout(frame);
    frameLayout->setContentsMargins(14, 12, 14, 14);
    frameLayout->setSpacing(8);

    auto *label = new QLabel(sectionTitle);
    label->setFont(appFont(12, QFont::DemiBold));
    label->setMinimumHeight(28);
    frameLayout->addWidget(label);

    auto *editor = new QTextEdit;
    editor->setReadOnly(true);
    editor->setLineWrapMode(QTextEdit::WidgetWidth);
    editor->setPlainText(text.trimmed().isEmpty() ? tr8("无") : text);
    editor->setMinimumHeight(minimumHeight);
    frameLayout->addWidget(editor);
    return frame;
}

QScrollArea *createHistoryDetailScrollArea(QWidget *contentWidget)
{
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QStringLiteral(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollArea > QWidget > QWidget { background: transparent; }"
    ));
    scroll->setWidget(contentWidget);
    return scroll;
}
