#include "history_detail_dialog.h"

#include "../domain/history_text.h"
#include "../tasks/history_segment_retry_task.h"
#include "attention_message.h"
#include "history_detail_widgets.h"
#include "history_segments_widget.h"
#include "ui_style.h"

#include <QtConcurrent>
#include <QtWidgets>
#include <QDesktopServices>
#include <QPointer>

static QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

HistoryDetailDialog::HistoryDetailDialog(
    const HistoryEntry &entry,
    const Callbacks &callbacks,
    QWidget *parent
)
    : AppDialog(parent),
      m_entry(QSharedPointer<HistoryEntry>::create(entry)),
      m_callbacks(callbacks)
{
    buildUi();
}

void HistoryDetailDialog::buildUi()
{
    setWindowTitle(tr8("记录详情"));
    setMinimumSize(820, 620);
    setStyleSheet(QStringLiteral(
        "QDialog { background: #f6f7f9; }"
        "QLabel { color: #111827; }"
        "QTextEdit { background: #ffffff; border: 1px solid #dde2ea; border-radius: 8px; padding: 10px; }"
    ));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);

    auto *title = new QLabel(m_entry->mode + tr8("记录详情"));
    title->setFont(appFont(18, QFont::DemiBold));
    title->setMinimumHeight(34);
    layout->addWidget(title);

    HistoryDetailMetaTexts metaTexts;
    metaTexts.timeText = historyDisplayTimeText(m_entry->time);
    metaTexts.elapsedText = elapsedText(m_entry->elapsedMs);
    metaTexts.speechElapsedText = elapsedText(m_entry->speechElapsedMs);
    metaTexts.modelElapsedText = elapsedText(m_entry->modelElapsedMs);
    metaTexts.modelText = modelText();
    metaTexts.ocrElapsedText = elapsedText(m_entry->ocrElapsedMs);
    layout->addWidget(createHistoryDetailMetaCard(*m_entry, metaTexts));

    auto *contentWidget = new QWidget;
    auto *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(0, 0, 10, 0);
    contentLayout->setSpacing(12);

    addSegmentsCard(contentLayout);

    auto addTextSection = [contentLayout](const QString &sectionTitle, const QString &text, int minimumHeight) {
        contentLayout->addWidget(createHistoryDetailTextSection(sectionTitle, text, minimumHeight));
    };
    addTextSection(tr8("识别文本"), recognizedText(), 110);
    addTextSection(tr8("输入内容"), m_entry->input, 130);
    addTextSection(tr8("模型输出"), m_entry->output, 170);
    addTextSection(tr8("错误"), m_entry->error.trimmed().isEmpty() ? tr8("无") : m_entry->error, 90);
    addTextSection(tr8("录音文件"), m_entry->audio.trimmed().isEmpty() ? tr8("本次没有录音") : m_entry->audio, 80);
    contentLayout->addStretch();

    layout->addWidget(createHistoryDetailScrollArea(contentWidget), 1);

    const bool audioExists =
        !m_entry->audio.trimmed().isEmpty() && QFileInfo::exists(m_entry->audio);
    auto *buttons = new QHBoxLayout;
    auto *copyAll = new QPushButton(tr8("复制详情"));
    copyAll->setMinimumSize(92, 38);
    copyAll->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
    auto *copy = new QPushButton(tr8("复制输出"));
    copy->setMinimumSize(92, 38);
    copy->setStyleSheet(buttonStyle(QStringLiteral("#111827")));
    auto *openAudio = new QPushButton(tr8("播放录音"));
    openAudio->setMinimumSize(92, 38);
    openAudio->setEnabled(audioExists);
    openAudio->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
    auto *close = new QPushButton(tr8("关闭"));
    close->setMinimumSize(78, 38);
    close->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
    buttons->addStretch();
    buttons->addWidget(copyAll);
    buttons->addWidget(copy);
    buttons->addWidget(openAudio);
    buttons->addWidget(close);
    layout->addLayout(buttons);

    connect(copyAll, &QPushButton::clicked, this, [this]() {
        copyDetailText();
    });
    connect(copy, &QPushButton::clicked, this, [this]() {
        copyOutputText();
    });
    connect(openAudio, &QPushButton::clicked, this, [this]() {
        openAudioFile();
    });
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
}

void HistoryDetailDialog::addSegmentsCard(QVBoxLayout *contentLayout)
{
    if (!contentLayout || m_entry->segments.isEmpty()) {
        return;
    }

    HistorySegmentsCallbacks segmentCallbacks;
    segmentCallbacks.elapsedText = [this](qint64 elapsedMs) {
        return elapsedText(elapsedMs);
    };
    segmentCallbacks.playSegment = [](const RecordingSegment &segment) {
        if (!segment.wavPath.trimmed().isEmpty()
            && QFileInfo::exists(segment.wavPath)) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(segment.wavPath));
        }
    };
    segmentCallbacks.copySegment = [](const RecordingSegment &segment) {
        QApplication::clipboard()->setText(segment.text);
    };
    segmentCallbacks.retrySegment = [this](
        const RecordingSegment &segment,
        QPushButton *retryButton
    ) {
        retrySegment(segment, retryButton);
    };
    contentLayout->addWidget(
        createHistorySegmentsCard(
            m_entry->segments,
            segmentCallbacks,
            this,
            this
        )
    );
}

void HistoryDetailDialog::retrySegment(const RecordingSegment &segment, QPushButton *retryButton)
{
    HistorySegmentRetryTaskRequest retryRequest;
    retryRequest.segment = segment;
    retryRequest.speechProvider = speechProvider();
    retryRequest.useSystemProxy = useSystemProxy();
    retryRequest.networkPolicy = speechNetworkPolicy();

    const HistorySegmentRetryTaskPreflight preflight =
        prepareHistorySegmentRetryTask(retryRequest);
    if (!preflight.ok) {
        showAttentionWarning(this, tr8("分段识别失败"), preflight.error);
        return;
    }
    retryRequest.pcm = preflight.pcm;

    if (retryButton) {
        retryButton->setEnabled(false);
        retryButton->setText(tr8("识别中"));
    }

    auto *watcher = new QFutureWatcher<HistorySegmentRetryResult>(this);
    const QPointer<QPushButton> guardedButton(retryButton);
    connect(
        watcher,
        &QFutureWatcher<HistorySegmentRetryResult>::finished,
        this,
        [this, watcher, guardedButton]() {
            const HistorySegmentRetryResult result = watcher->result();
            watcher->deleteLater();

            if (guardedButton) {
                guardedButton->setText(tr8("重试识别"));
                guardedButton->setEnabled(true);
            }

            const bool updated = m_callbacks.updateRetriedSegment
                ? m_callbacks.updateRetriedSegment(m_entry.data(), result)
                : false;
            if (!updated) {
                showAttentionWarning(
                    this,
                    tr8("保存失败"),
                    tr8("分段识别已经结束，但无法更新历史记录文件。")
                );
                return;
            }

            if (m_callbacks.historyChanged) {
                m_callbacks.historyChanged();
            }

            if (result.text.trimmed().isEmpty()) {
                showAttentionWarning(
                    this,
                    tr8("分段识别失败"),
                    result.error.trimmed().isEmpty()
                        ? tr8("语音接口没有返回识别文字。")
                        : result.error
                );
                return;
            }

            showAttentionInformation(
                this,
                tr8("识别完成"),
                tr8("这一段的识别文字和历史记录已更新。")
            );
            accept();
        }
    );

    watcher->setFuture(QtConcurrent::run(
        [retryRequest]() {
            return runHistorySegmentRetryTask(retryRequest);
        }
    ));
}

void HistoryDetailDialog::copyDetailText() const
{
    QApplication::clipboard()->setText(detailPlainText());
}

void HistoryDetailDialog::copyOutputText() const
{
    QApplication::clipboard()->setText(m_entry->output.isEmpty() ? m_entry->input : m_entry->output);
}

void HistoryDetailDialog::openAudioFile() const
{
    if (!m_entry->audio.trimmed().isEmpty() && QFileInfo::exists(m_entry->audio)) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_entry->audio));
    }
}

QString HistoryDetailDialog::elapsedText(qint64 elapsedMs) const
{
    return m_callbacks.elapsedText
        ? m_callbacks.elapsedText(elapsedMs)
        : historyElapsedDurationText(elapsedMs);
}

QString HistoryDetailDialog::modelText() const
{
    return m_callbacks.modelText
        ? m_callbacks.modelText(*m_entry)
        : m_entry->model;
}

QString HistoryDetailDialog::recognizedText() const
{
    return m_callbacks.recognizedText
        ? m_callbacks.recognizedText(*m_entry)
        : historyEntryRecognizedText(*m_entry);
}

QString HistoryDetailDialog::detailPlainText() const
{
    return m_callbacks.detailPlainText
        ? m_callbacks.detailPlainText(*m_entry)
        : historyEntryDetailPlainText(*m_entry, modelText());
}

QString HistoryDetailDialog::speechProvider() const
{
    return m_callbacks.speechProvider ? m_callbacks.speechProvider() : QString();
}

QString HistoryDetailDialog::speechNetworkPolicy() const
{
    return m_callbacks.speechNetworkPolicy
        ? m_callbacks.speechNetworkPolicy(m_entry->modeId)
        : QStringLiteral("inherit");
}

bool HistoryDetailDialog::useSystemProxy() const
{
    return m_callbacks.useSystemProxy ? m_callbacks.useSystemProxy() : false;
}
