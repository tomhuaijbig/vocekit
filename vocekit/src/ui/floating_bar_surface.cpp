#include "floating_bar_surface.h"

#include "../config/app_settings_defaults.h"
#include "ui_style.h"

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include <cmath>

namespace {

QString defaultTitle(FloatingBarStage stage)
{
    switch (stage) {
    case FloatingBarStage::Preparing:
        return QString::fromUtf8("准备录音");
    case FloatingBarStage::Recording:
    case FloatingBarStage::Streaming:
        return QString::fromUtf8("正在聆听");
    case FloatingBarStage::Recognizing:
        return QString::fromUtf8("正在转录");
    case FloatingBarStage::ModelProcessing:
        return QString::fromUtf8("AI 处理中");
    case FloatingBarStage::Writing:
        return QString::fromUtf8("正在写入");
    case FloatingBarStage::Completed:
        return QString::fromUtf8("处理完成");
    case FloatingBarStage::Failed:
        return QString::fromUtf8("处理失败");
    case FloatingBarStage::StreamingFinalizing:
        return QString::fromUtf8("正在完成识别");
    case FloatingBarStage::StreamingFallback:
        return QString::fromUtf8("已切换整段识别");
    }
    return QString();
}

bool showsRecordingActions(FloatingBarStage stage)
{
    return stage == FloatingBarStage::Preparing
        || stage == FloatingBarStage::Recording
        || stage == FloatingBarStage::Streaming;
}

QIcon actionIcon(bool confirm)
{
    QImage image(40, 40, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(Qt::white, 3.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    if (confirm) {
        QPainterPath path;
        path.moveTo(9, 21);
        path.lineTo(17, 29);
        path.lineTo(32, 12);
        painter.drawPath(path);
    } else {
        painter.drawLine(QPointF(11, 11), QPointF(29, 29));
        painter.drawLine(QPointF(29, 11), QPointF(11, 29));
    }
    return QIcon(QPixmap::fromImage(image));
}

class CompactWaveform : public QWidget
{
public:
    explicit CompactWaveform(QWidget *parent = nullptr) : QWidget(parent)
    {
        setObjectName(QStringLiteral("floatingWaveform"));
        setMinimumSize(86, 30);
        setMaximumHeight(38);
    }

    void setPeak(int peak)
    {
        m_peak = qBound(0, peak, 3200);
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        const qreal ratio = std::sqrt(m_peak / 3200.0);
        const int count = 11;
        const qreal gap = 3.0;
        const qreal barWidth = qMax(2.0, (width() - gap * (count - 1)) / count);
        for (int i = 0; i < count; ++i) {
            const qreal phase = 0.35 + 0.65 * ((i % 5) + 1) / 5.0;
            const qreal h = qMax(4.0, (height() - 6) * ratio * phase);
            painter.setBrush(i > count - 4 ? QColor("#ffffff") : QColor("#9ca3af"));
            painter.drawRoundedRect(
                QRectF(i * (barWidth + gap), (height() - h) / 2,
                       barWidth, h),
                2,
                2
            );
        }
    }

private:
    int m_peak = 0;
};

class BaseSurface : public FloatingBarSurface
{
public:
    BaseSurface(bool transcript, QWidget *parent)
        : FloatingBarSurface(parent), m_hasTranscript(transcript)
    {
        setObjectName(transcript
            ? QStringLiteral("liveTranscriptFloatingBarSurface")
            : QStringLiteral("statusPillFloatingBarSurface"));
        setStyleSheet(QStringLiteral(
            "QWidget#statusPillFloatingBarSurface,"
            "QWidget#liveTranscriptFloatingBarSurface {"
            " background:#111111; border:1px solid #333333;"
            " border-radius:18px; }"
            "QLabel { color:#ffffff; background:transparent; }"
            "QPushButton { background:#2c2c2c; color:#ffffff; border:none;"
            " border-radius:17px; min-width:34px; min-height:34px; }"
            "QPushButton:hover { background:#454545; }"
            "QPushButton:disabled { color:#777777; }"
        ));
        QVBoxLayout *outer = new QVBoxLayout(this);
        outer->setContentsMargins(10, 8, 10, 8);
        outer->setSpacing(5);
        QHBoxLayout *row = new QHBoxLayout;
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(8);
        m_cancel = new QPushButton(this);
        m_cancel->setObjectName(QStringLiteral("floatingCancelButton"));
        m_cancel->setCursor(Qt::PointingHandCursor);
        m_cancel->setIcon(actionIcon(false));
        m_waveform = new CompactWaveform(this);
        m_title = new QLabel(this);
        m_title->setObjectName(QStringLiteral("floatingBarTitle"));
        m_title->setFont(appFont(10, QFont::DemiBold));
        m_title->setAlignment(Qt::AlignCenter);
        m_confirm = new QPushButton(this);
        m_confirm->setObjectName(QStringLiteral("floatingConfirmButton"));
        m_confirm->setCursor(Qt::PointingHandCursor);
        m_confirm->setIcon(actionIcon(true));
        const int actionSize = qMax(
            34,
            QFontMetrics(m_cancel->font()).height() + 16
        );
        m_cancel->setMinimumSize(actionSize, actionSize);
        m_confirm->setMinimumSize(actionSize, actionSize);
        m_cancel->setIconSize(QSize(actionSize - 14, actionSize - 14));
        m_confirm->setIconSize(QSize(actionSize - 14, actionSize - 14));
        row->addWidget(m_cancel);
        row->addWidget(m_waveform, 1);
        row->addWidget(m_title, transcript ? 0 : 1);
        row->addWidget(m_confirm);
        outer->addLayout(row);

        m_detail = new QLabel(this);
        m_detail->setObjectName(QStringLiteral("floatingBarSubtitle"));
        m_detail->setFont(appFont(9));
        m_detail->setAlignment(Qt::AlignCenter);
        m_detail->setWordWrap(true);
        m_detail->setStyleSheet(QStringLiteral("color:#aeb7c5;"));
        outer->addWidget(m_detail);

        if (transcript) {
            QScrollArea *transcriptArea = new QScrollArea(this);
            transcriptArea->setObjectName(
                QStringLiteral("streamingTranscriptScrollArea"));
            transcriptArea->setWidgetResizable(true);
            transcriptArea->setHorizontalScrollBarPolicy(
                Qt::ScrollBarAlwaysOff);
            transcriptArea->setFrameShape(QFrame::NoFrame);
            transcriptArea->setStyleSheet(QStringLiteral(
                "QScrollArea#streamingTranscriptScrollArea {"
                " background:transparent; border:none; }"
                "QScrollArea#streamingTranscriptScrollArea > QWidget > QWidget {"
                " background:transparent; }"
            ));
            QWidget *transcriptContents = new QWidget(transcriptArea);
            transcriptContents->setObjectName(
                QStringLiteral("streamingTranscriptContents"));
            QVBoxLayout *transcriptLayout =
                new QVBoxLayout(transcriptContents);
            transcriptLayout->setContentsMargins(0, 0, 0, 0);
            transcriptLayout->setSpacing(4);
            transcriptLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);
            m_committed = new QLabel(this);
            m_committed->setObjectName(QStringLiteral("streamingCommittedText"));
            m_committed->setFont(appFont(9));
            m_committed->setWordWrap(true);
            m_committed->setSizePolicy(
                QSizePolicy::Preferred, QSizePolicy::Minimum);
            m_committed->setTextInteractionFlags(Qt::NoTextInteraction);
            m_provisional = new QLabel(this);
            m_provisional->setObjectName(QStringLiteral("streamingProvisionalText"));
            m_provisional->setFont(appFont(9));
            m_provisional->setWordWrap(true);
            m_provisional->setSizePolicy(
                QSizePolicy::Preferred, QSizePolicy::Minimum);
            m_provisional->setTextInteractionFlags(Qt::NoTextInteraction);
            m_provisional->setStyleSheet(QStringLiteral("color:#60a5fa;"));
            transcriptLayout->addWidget(m_committed);
            transcriptArea->setWidget(transcriptContents);
            const int transcriptLine =
                QFontMetrics(m_committed->font()).lineSpacing();
            transcriptArea->setMaximumHeight(transcriptLine * 4 + 12);
            outer->addWidget(transcriptArea);
            outer->addWidget(m_provisional);
        }

        connect(m_cancel, &QPushButton::clicked, this, [this]() {
            if (m_actions.cancel) m_actions.cancel();
        });
        connect(m_confirm, &QPushButton::clicked, this, [this]() {
            if (m_actions.confirm) m_actions.confirm();
        });
    }

    void render(
        const FloatingBarViewState &state,
        const FloatingBarActions &actions) override
    {
        m_actions = actions;
        const int actionSize = qMax(
            34,
            QFontMetrics(m_cancel->font()).height() + 16
        );
        m_cancel->setMinimumSize(actionSize, actionSize);
        m_confirm->setMinimumSize(actionSize, actionSize);
        m_cancel->setIconSize(QSize(actionSize - 14, actionSize - 14));
        m_confirm->setIconSize(QSize(actionSize - 14, actionSize - 14));
        const bool showActions = showsRecordingActions(state.stage);
        m_cancel->setVisible(showActions);
        m_confirm->setVisible(showActions);
        m_cancel->setEnabled(state.cancelEnabled && bool(actions.cancel));
        m_confirm->setEnabled(state.confirmEnabled && bool(actions.confirm));
        m_waveform->setVisible(showActions && state.waveformVisible);
        m_waveform->setPeak(state.waveformPeak);
        m_title->setText(state.title.isEmpty()
            ? defaultTitle(state.stage) : state.title);
        m_title->setMinimumHeight(m_title->sizeHint().height());
        m_detail->setText(state.detail);
        m_detail->setVisible(!state.detail.isEmpty());
        const int detailHeight = state.detail.isEmpty()
            ? 0
            : qMax(
                  m_detail->sizeHint().height(),
                  QFontMetrics(m_detail->font()).boundingRect(
                      QRect(0, 0, 340, QWIDGETSIZE_MAX),
                      Qt::TextWordWrap | Qt::AlignCenter,
                      state.detail
                  ).height() + 2
              );
        m_detail->setMinimumHeight(detailHeight);
        m_detail->setFixedHeight(detailHeight);
        if (m_hasTranscript) {
            m_committed->setText(state.committedText);
            m_provisional->setText(state.provisionalText);
            m_committed->setMaximumHeight(QWIDGETSIZE_MAX);
            m_provisional->setMaximumHeight(QWIDGETSIZE_MAX);
            m_committed->setVisible(!state.committedText.isEmpty());
            m_provisional->setVisible(!state.provisionalText.isEmpty());
            m_committed->setMinimumHeight(m_committed->sizeHint().height());
            m_provisional->setMinimumHeight(
                m_provisional->sizeHint().height());
        }
        setFixedWidth(m_hasTranscript ? 440 : 360);
        setMaximumHeight(m_hasTranscript ? 240 : 180);
        adjustSize();
    }

private:
    bool m_hasTranscript = false;
    FloatingBarActions m_actions;
    QPushButton *m_cancel = nullptr;
    QPushButton *m_confirm = nullptr;
    CompactWaveform *m_waveform = nullptr;
    QLabel *m_title = nullptr;
    QLabel *m_detail = nullptr;
    QLabel *m_committed = nullptr;
    QLabel *m_provisional = nullptr;
};

} // namespace

FloatingBarSurface *createFloatingBarSurface(
    const QString &style,
    QWidget *parent)
{
    return new BaseSurface(
        normalizeGlobalFloatingBarStyle(style)
            == floatingBarStyleLiveTranscriptCard(),
        parent
    );
}
