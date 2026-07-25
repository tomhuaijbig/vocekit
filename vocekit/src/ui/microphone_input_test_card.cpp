#include "microphone_input_test_card.h"

#include "attention_message.h"
#include "diagnostic_action_card.h"
#include "ui_style.h"
#include "../recording/segmented_recording.h"
#include "../storage/history_paths.h"
#include "../tasks/diagnostic_helpers.h"
#include "../tasks/microphone_diagnostic_task.h"

#include <QtWidgets>
#include <QtMultimedia>

static QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

#include "../recording/audio_recorder_legacy.h"

MicrophoneInputTestCard::MicrophoneInputTestCard(
    const std::function<QString()> &recordDirectoryPath,
    QWidget *parent
)
    : QFrame(parent),
      m_recordDirectoryPath(recordDirectoryPath),
      m_recorder(new AudioRecorder)
{
    setObjectName(QStringLiteral("card"));
    setProperty(
        "testSearchText",
        tr8("麦克风测试 录音 测试样本 保留样本 峰值 平均音量 削波")
    );
    setStyleSheet(cardStyle());

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);

    auto *top = new QHBoxLayout;
    auto *name = new QLabel(tr8("麦克风测试"));
    name->setFont(appFont(13, QFont::DemiBold));

    auto *keepLabel = new QLabel(tr8("保留测试样本"));
    keepLabel->setMinimumHeight(32);

    m_keepSampleBox = new QCheckBox;
    m_keepSampleBox->setChecked(false);

    m_button = new QPushButton(tr8("开始测试"));
    m_button->setMinimumSize(96, 36);
    m_button->setStyleSheet(buttonStyle(QStringLiteral("#111827")));
    connect(m_button, &QPushButton::clicked, this, [this]() {
        startTest();
    });

    top->addWidget(name, 1);
    top->addWidget(keepLabel);
    top->addWidget(m_keepSampleBox);
    top->addWidget(m_button);
    layout->addLayout(top);

    m_result = new QLabel;
    m_result->setWordWrap(true);
    m_result->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_result->setVisible(false);
    m_result->setStyleSheet(QStringLiteral(
        "QLabel { background: #f2f4f7; color: #344054; border-radius: 6px; padding: 10px; }"
    ));
    layout->addWidget(m_result);
}

MicrophoneInputTestCard::~MicrophoneInputTestCard()
{
    delete m_recorder;
}

void MicrophoneInputTestCard::startTest()
{
    if (!m_result || !m_button || m_testing || !m_recorder) {
        return;
    }

    const bool keepSample = m_keepSampleBox && m_keepSampleBox->isChecked();

    QString error;
    if (!m_recorder->startInDirectory(
            tr8("麦克风测试"),
            sampleDirectory(keepSample),
            &error)) {
        showDiagnosticResult(
            m_result,
            diagnosticStatusLine(
                tr8("麦克风测试"),
                tr8("失败"),
                compactDiagnosticError(error)
            )
        );
        showAttentionWarning(
            this,
            tr8("麦克风测试失败"),
            error.isEmpty() ? tr8("无法启动默认麦克风。") : error
        );
        return;
    }

    m_testing = true;
    m_button->setEnabled(false);
    showDiagnosticResult(
        m_result,
        tr8("正在录音 3 秒钟，请对着麦克风说一句话...")
    );

    QTimer::singleShot(3000, this, [this, keepSample]() {
        finishTest(keepSample);
    });
}

void MicrophoneInputTestCard::finishTest(bool keepSample)
{
    if (!m_recorder) {
        return;
    }

    const QByteArray pcm = m_recorder->stop();
    const QString samplePath = m_recorder->lastWavPath();
    if (!keepSample) {
        QFile::remove(samplePath);
    }

    m_testing = false;
    if (m_button) {
        m_button->setEnabled(true);
    }
    if (!m_result) {
        return;
    }

    MicrophoneDiagnosticRequest request;
    request.pcm = pcm;
    request.samplePath = samplePath;
    request.keepSample = keepSample;
    const MicrophoneDiagnosticResult result = runMicrophoneDiagnosticTask(request);
    showDiagnosticResult(m_result, result.displayText);
    if (result.showWarning) {
        showAttentionWarning(this, result.warningTitle, result.warningMessage);
    }
}

QString MicrophoneInputTestCard::sampleDirectory(bool keepSample) const
{
    if (!keepSample) {
        return QDir::tempPath();
    }
    const QString recordDirectory = m_recordDirectoryPath
        ? m_recordDirectoryPath()
        : QString();
    return QDir(historyRootPath(recordDirectory)).filePath(
        tr8("测试样本/麦克风")
            + QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"))
    );
}
