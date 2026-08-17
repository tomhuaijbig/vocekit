#include "floating_bar_test_card.h"

#include "attention_message.h"
#include "diagnostic_action_card.h"
#include "floating_bar.h"
#include "ui_style.h"
#include "../tasks/diagnostic_helpers.h"

#include <QtWidgets>

#include <cmath>

namespace {

QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

QString stageTitle(FloatingBarStage stage)
{
    switch (stage) {
    case FloatingBarStage::Preparing:
        return tr8("准备录音");
    case FloatingBarStage::Recording:
        return tr8("正在录音");
    case FloatingBarStage::Recognizing:
        return tr8("正在识别");
    case FloatingBarStage::ModelProcessing:
        return tr8("模型处理中");
    case FloatingBarStage::Writing:
        return tr8("正在写入");
    case FloatingBarStage::Completed:
        return tr8("处理完成");
    case FloatingBarStage::Failed:
        return tr8("处理失败");
    case FloatingBarStage::Streaming:
        return tr8("实时识别中");
    case FloatingBarStage::StreamingFinalizing:
        return tr8("正在完成识别");
    case FloatingBarStage::StreamingFallback:
        return tr8("实时识别已切换");
    }
    return tr8("浮动条测试");
}

QString stageSubtitle(FloatingBarStage stage)
{
    switch (stage) {
    case FloatingBarStage::Preparing:
        return tr8("倒计时和提示音完成后开始录音");
    case FloatingBarStage::Recording:
        return tr8("模拟声音输入，波形会持续变化");
    case FloatingBarStage::Recognizing:
        return tr8("语音识别处理中 · 0.0 秒");
    case FloatingBarStage::ModelProcessing:
        return tr8("正在生成结果 · 0.0 秒");
    case FloatingBarStage::Writing:
        return tr8("正在写入当前输入位置");
    case FloatingBarStage::Completed:
        return tr8("浮动条完成状态显示正常");
    case FloatingBarStage::Failed:
        return tr8("这是浮动条失败状态测试");
    case FloatingBarStage::Streaming:
        return tr8("文字会在录音时持续更新");
    case FloatingBarStage::StreamingFinalizing:
        return tr8("正在等待最终文字");
    case FloatingBarStage::StreamingFallback:
        return tr8("将使用录音结束后的整段识别");
    }
    return QString();
}

QString timedSubtitle(FloatingBarStage stage, const QString &elapsed)
{
    if (stage == FloatingBarStage::Recognizing) {
        return tr8("语音识别处理中 · ") + elapsed + tr8(" 秒");
    }
    if (stage == FloatingBarStage::ModelProcessing) {
        return tr8("正在生成结果 · ") + elapsed + tr8(" 秒");
    }
    return tr8("正在写入当前输入位置 · ") + elapsed + tr8(" 秒");
}

bool needsTimedPreview(FloatingBarStage stage)
{
    return stage == FloatingBarStage::Recognizing
        || stage == FloatingBarStage::ModelProcessing
        || stage == FloatingBarStage::Writing;
}

} // namespace

FloatingBarTestCard::FloatingBarTestCard(
    const std::function<bool()> &floatingBarEnabled,
    const std::function<int()> &dictateFloatingBarSeconds,
    FloatingBar *floatingBar,
    QWidget *parent
)
    : QFrame(parent),
      m_floatingBarEnabled(floatingBarEnabled),
      m_dictateFloatingBarSeconds(dictateFloatingBarSeconds),
      m_floatingBar(floatingBar)
{
    setObjectName(QStringLiteral("card"));
    setProperty(
        "testSearchText",
        tr8("浮动条测试 准备 录音 识别 模型处理中 写入 完成 失败 波形 状态")
    );
    setStyleSheet(cardStyle());

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);

    auto *top = new QHBoxLayout;
    auto *name = new QLabel(tr8("浮动条测试"));
    name->setFont(appFont(13, QFont::DemiBold));

    m_stageBox = new QComboBox;
    m_stageBox->setMinimumSize(170, 36);
    m_stageBox->addItem(tr8("准备录音"), int(FloatingBarStage::Preparing));
    m_stageBox->addItem(tr8("录音中（波形）"), int(FloatingBarStage::Recording));
    m_stageBox->addItem(tr8("识别中"), int(FloatingBarStage::Recognizing));
    m_stageBox->addItem(tr8("模型处理中"), int(FloatingBarStage::ModelProcessing));
    m_stageBox->addItem(tr8("写入中"), int(FloatingBarStage::Writing));
    m_stageBox->addItem(tr8("已完成"), int(FloatingBarStage::Completed));
    m_stageBox->addItem(tr8("失败"), int(FloatingBarStage::Failed));
    m_stageBox->addItem(tr8("实时识别中"), int(FloatingBarStage::Streaming));
    m_stageBox->addItem(
        tr8("等待实时最终结果"),
        int(FloatingBarStage::StreamingFinalizing)
    );
    m_stageBox->addItem(
        tr8("实时识别已降级"),
        int(FloatingBarStage::StreamingFallback)
    );

    auto *button = new QPushButton(tr8("开始测试"));
    button->setMinimumSize(96, 36);
    button->setStyleSheet(buttonStyle(QStringLiteral("#111827")));
    connect(button, &QPushButton::clicked, this, [this]() {
        runTest();
    });

    top->addWidget(name, 1);
    top->addWidget(m_stageBox);
    top->addWidget(button);
    layout->addLayout(top);

    m_result = new QLabel;
    m_result->setWordWrap(true);
    m_result->setVisible(false);
    m_result->setStyleSheet(QStringLiteral(
        "QLabel { background: #f2f4f7; color: #344054; border-radius: 6px; padding: 10px; }"
    ));
    layout->addWidget(m_result);
}

void FloatingBarTestCard::runTest()
{
    if (m_floatingBarEnabled && !m_floatingBarEnabled()) {
        showAttentionInformation(
            this,
            tr8("浮动条已关闭"),
            tr8("请在设置的“常用设置”页勾选“启用浮动条”。")
        );
        return;
    }
    if (!m_floatingBar) {
        showAttentionWarning(
            this,
            tr8("浮动条测试失败"),
            tr8("没有连接到浮动条对象，请重启软件后再试。")
        );
        return;
    }
    if (m_dictateFloatingBarSeconds && m_dictateFloatingBarSeconds() <= 0) {
        showDiagnosticResult(
            m_result,
            diagnosticStatusLine(
                tr8("浮动条测试"),
                tr8("未启用显示"),
                tr8("当前听写功能的浮动条显示时间为 0 秒。")
            )
        );
        return;
    }

    const FloatingBarStage stage = m_stageBox
        ? FloatingBarStage(m_stageBox->currentData().toInt())
        : FloatingBarStage::Recording;
    const QString title = stageTitle(stage);

    m_floatingBar->setSuppressed(false);
    m_floatingBar->setWaveformVisible(false);
    m_floatingBar->clearStreamingTranscript();
    m_floatingBar->setStatus(title, stageSubtitle(stage));
    if (stage == FloatingBarStage::Streaming) {
        m_floatingBar->setStreamingTranscript(
            tr8("这是已经确认的实时识别文字，"),
            tr8("这是正在变化的临时文字")
        );
    } else if (stage == FloatingBarStage::StreamingFinalizing) {
        m_floatingBar->setStreamingTranscript(
            tr8("这是已经确认的实时识别文字。"),
            QString()
        );
        m_floatingBar->setStreamingFinalizing();
    } else if (stage == FloatingBarStage::StreamingFallback) {
        m_floatingBar->setStreamingFallback();
    }
    showDiagnosticResult(
        m_result,
        diagnosticStatusLine(tr8("浮动条测试"), tr8("已显示"), title)
    );

    stopPreviewTimer();
    if (stage == FloatingBarStage::Recording) {
        startRecordingPreview();
    } else if (needsTimedPreview(stage)) {
        startTimedPreview(title, int(stage));
    } else {
        m_floatingBar->hideLater(5000);
    }
}

void FloatingBarTestCard::stopPreviewTimer()
{
    if (!m_previewTimer) {
        return;
    }
    m_previewTimer->stop();
    m_previewTimer->deleteLater();
    m_previewTimer = nullptr;
}

void FloatingBarTestCard::startRecordingPreview()
{
    if (!m_floatingBar) {
        return;
    }
    m_floatingBar->setWaveformVisible(true);
    m_previewTimer = new QTimer(this);
    m_previewTimer->setInterval(80);
    connect(m_previewTimer, &QTimer::timeout, this, [this]() {
        ++m_previewTick;
        const double phase = m_previewTick / 4.0;
        const int peak = 180
            + qRound((std::sin(phase) + 1.0) * 900.0)
            + (m_previewTick % 5) * 80;
        if (m_floatingBar) {
            m_floatingBar->setWaveformLevel(peak);
        }
        if (m_previewTick >= 125) {
            stopPreviewTimer();
            if (m_floatingBar) {
                m_floatingBar->setWaveformVisible(false);
                m_floatingBar->hide();
            }
        }
    });
    m_previewTick = 0;
    m_previewTimer->start();
}

void FloatingBarTestCard::startTimedPreview(const QString &title, int stageValue)
{
    const FloatingBarStage stage = FloatingBarStage(stageValue);
    m_previewTimer = new QTimer(this);
    m_previewTimer->setInterval(100);
    connect(m_previewTimer, &QTimer::timeout, this, [this, stage, title]() {
        ++m_previewTick;
        const QString elapsed = QString::number(m_previewTick / 10.0, 'f', 1);
        if (m_floatingBar) {
            m_floatingBar->setStatus(title, timedSubtitle(stage, elapsed));
        }
        if (m_previewTick >= 50) {
            stopPreviewTimer();
            if (m_floatingBar) {
                m_floatingBar->hide();
            }
        }
    });
    m_previewTick = 0;
    m_previewTimer->start();
}
