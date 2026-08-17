#include "history_segments_widget.h"

#include "ui_style.h"

#include <QtWidgets>

static QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

QFrame *createHistorySegmentsCard(
    const QVector<RecordingSegment> &segments,
    const HistorySegmentsCallbacks &callbacks,
    QWidget *context,
    QWidget *parent
)
{
    auto *segmentsFrame = new QFrame(parent);
    segmentsFrame->setObjectName(QStringLiteral("card"));
    segmentsFrame->setStyleSheet(cardStyle());
    auto *segmentsLayout = new QVBoxLayout(segmentsFrame);
    segmentsLayout->setContentsMargins(14, 12, 14, 14);
    segmentsLayout->setSpacing(8);

    auto *segmentsTitle = new QLabel(
        tr8("录音分段（%1）").arg(segments.size())
    );
    segmentsTitle->setFont(appFont(12, QFont::DemiBold));
    segmentsTitle->setMinimumHeight(30);
    segmentsLayout->addWidget(segmentsTitle);

    for (const RecordingSegment &segment : segments) {
        auto *segmentCard = new QFrame;
        segmentCard->setStyleSheet(QStringLiteral(
            "QFrame { background: #f8fafc; border: 1px solid #dde2ea; border-radius: 7px; }"
            "QLabel { border: none; background: transparent; }"
        ));
        auto *segmentLayout = new QVBoxLayout(segmentCard);
        segmentLayout->setContentsMargins(12, 10, 12, 10);
        segmentLayout->setSpacing(7);

        auto *header = new QHBoxLayout;
        const QString elapsed = callbacks.elapsedText
            ? callbacks.elapsedText(segment.recognitionElapsedMs)
            : QString();
        auto *segmentName = new QLabel(
            tr8("第 %1 段").arg(segment.index)
                + tr8(" · ")
                + elapsed
        );
        segmentName->setFont(appFont(10, QFont::DemiBold));
        segmentName->setMinimumHeight(32);

        auto *playSegment = new QPushButton(tr8("播放"));
        playSegment->setMinimumSize(72, 34);
        playSegment->setEnabled(
            callbacks.playSegment
                && !segment.wavPath.trimmed().isEmpty()
                && QFileInfo::exists(segment.wavPath)
        );
        playSegment->setStyleSheet(compactButtonStyle(QStringLiteral("#111827")));

        auto *copySegment = new QPushButton(tr8("复制文字"));
        copySegment->setMinimumSize(88, 34);
        copySegment->setEnabled(callbacks.copySegment && !segment.text.trimmed().isEmpty());
        copySegment->setStyleSheet(
            buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827"))
        );

        auto *retrySegment = new QPushButton(tr8("重试识别"));
        retrySegment->setMinimumSize(88, 34);
        retrySegment->setVisible(segment.text.trimmed().isEmpty());
        retrySegment->setEnabled(
            callbacks.retrySegment
                && segment.text.trimmed().isEmpty()
                && !segment.wavPath.trimmed().isEmpty()
                && QFileInfo::exists(segment.wavPath)
        );
        retrySegment->setStyleSheet(compactButtonStyle(QStringLiteral("#111827")));

        header->addWidget(segmentName, 1);
        header->addWidget(playSegment);
        header->addWidget(copySegment);
        header->addWidget(retrySegment);
        segmentLayout->addLayout(header);

        auto *segmentText = new QLabel(
            segment.text.trimmed().isEmpty()
                ? (segment.error.trimmed().isEmpty()
                    ? tr8("没有识别文字")
                    : tr8("识别失败：") + segment.error)
                : segment.text
        );
        segmentText->setWordWrap(true);
        segmentText->setTextInteractionFlags(Qt::TextSelectableByMouse);
        segmentText->setStyleSheet(
            segment.text.trimmed().isEmpty()
                ? QStringLiteral("color: #b42318;")
                : QStringLiteral("color: #344054;")
        );
        segmentText->setMinimumHeight(28);
        segmentLayout->addWidget(segmentText);
        segmentsLayout->addWidget(segmentCard);

        QObject::connect(playSegment, &QPushButton::clicked, context, [callbacks, segment]() {
            if (callbacks.playSegment) {
                callbacks.playSegment(segment);
            }
        });
        QObject::connect(copySegment, &QPushButton::clicked, context, [callbacks, segment]() {
            if (callbacks.copySegment) {
                callbacks.copySegment(segment);
            }
        });
        QObject::connect(retrySegment, &QPushButton::clicked, context, [callbacks, segment, retrySegment]() {
            if (callbacks.retrySegment) {
                callbacks.retrySegment(segment, retrySegment);
            }
        });
    }

    return segmentsFrame;
}
