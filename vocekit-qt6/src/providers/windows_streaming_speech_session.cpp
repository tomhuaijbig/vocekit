#include "windows_streaming_speech_session.h"

#include "windows_speech_helper_protocol.h"

#include <QFileInfo>
#include <QPointer>

namespace {

const qint64 MaximumOutputBytes = 1024 * 1024;
const int MaximumLineBytes = 64 * 1024;

QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

QString displayMessage(
    const QString &helperCode,
    const QString &helperMessage)
{
    if (!helperMessage.trimmed().isEmpty()) {
        return helperMessage.trimmed();
    }
    if (helperCode == QStringLiteral("PROGRAM_MISSING")) {
        return tr8("Windows 本地语音组件不存在，请重新安装或修复软件。");
    }
    if (helperCode == QStringLiteral("RECOGNIZER_MISSING")) {
        return tr8("Windows 没有安装所选语言的语音识别器。");
    }
    if (helperCode == QStringLiteral("SYSTEM_SPEECH_UNAVAILABLE")) {
        return tr8("Windows 系统语音运行库不可用。");
    }
    if (helperCode == QStringLiteral("GRAMMAR_LOAD_FAILED")) {
        return tr8("Windows 本地语音识别器初始化失败。");
    }
    return tr8("Windows 本地语音识别发生错误。");
}

} // namespace

WindowsStreamingSpeechSession::WindowsStreamingSpeechSession(
    const QString &programPath,
    const QStringList &prependedArguments,
    const StreamingSpeechSessionRequest &request,
    const StreamingSpeechCallbacks &callbacks,
    const Timing &timing,
    QObject *parent)
    : QObject(parent),
      m_programPath(programPath),
      m_prependedArguments(prependedArguments),
      m_request(request),
      m_callbacks(callbacks),
      m_timing(timing)
{
    m_startupTimer.setSingleShot(true);
    m_finalTimer.setSingleShot(true);
    connect(&m_startupTimer, &QTimer::timeout, this, [this]() {
        fail(
            QStringLiteral("START_FAILED"),
            tr8("Windows 本地语音组件启动超时。")
        );
    });
    connect(&m_finalTimer, &QTimer::timeout, this, [this]() {
        fail(
            QStringLiteral("TIMEOUT"),
            tr8("Windows 本地语音识别等待最终结果超时。")
        );
    });
    connect(&m_process, &QProcess::started,
            this, &WindowsStreamingSpeechSession::onStarted);
    connect(&m_process, &QProcess::readyReadStandardOutput,
            this,
            &WindowsStreamingSpeechSession::onReadyReadStandardOutput);
    connect(&m_process, &QProcess::readyReadStandardError,
            this,
            &WindowsStreamingSpeechSession::onReadyReadStandardError);
    connect(&m_process, &QProcess::bytesWritten,
            this, &WindowsStreamingSpeechSession::onBytesWritten);
    connect(
        &m_process,
        static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(
            &QProcess::finished
        ),
        this,
        &WindowsStreamingSpeechSession::onFinished
    );
    connect(
        &m_process,
        static_cast<void (QProcess::*)(QProcess::ProcessError)>(
            &QProcess::errorOccurred
        ),
        this,
        &WindowsStreamingSpeechSession::onProcessError
    );
}

WindowsStreamingSpeechSession::~WindowsStreamingSpeechSession()
{
    cleanup(false);
}

bool WindowsStreamingSpeechSession::start(QString *error)
{
    if (m_state != StreamingSpeechState::Idle) {
        if (error) {
            *error = tr8("语音识别会话已经启动。");
        }
        return false;
    }
    if (m_request.runId.trimmed().isEmpty()
        || m_request.language.trimmed().isEmpty()
        || m_request.sampleRate <= 0
        || m_request.channelCount <= 0
        || m_request.sampleSizeBits <= 0) {
        if (error) {
            *error = tr8("Windows 本地语音识别请求无效。");
        }
        return false;
    }
    if (error) {
        error->clear();
    }

    m_state = StreamingSpeechState::Connecting;
    const QFileInfo programInfo(m_programPath);
    if (!programInfo.isAbsolute()
        || !programInfo.isFile()
        || programInfo.suffix().compare(
            QStringLiteral("exe"),
            Qt::CaseInsensitive
        ) != 0) {
        QTimer::singleShot(0, this, [this]() {
            fail(
                QStringLiteral("PROGRAM_MISSING"),
                displayMessage(QStringLiteral("PROGRAM_MISSING"), QString())
            );
        });
        return true;
    }

    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    const QStringList arguments = m_prependedArguments
        + windowsSpeechHelperArguments(
            QStringLiteral("stream"),
            m_request.runId.trimmed(),
            m_request.language.trimmed(),
            m_request.sampleRate,
            m_request.channelCount,
            m_request.sampleSizeBits
        );
    m_process.start(m_programPath, arguments, QIODevice::ReadWrite);
    m_startupTimer.start(qMax(1, m_timing.startupTimeoutMs));
    return true;
}

bool WindowsStreamingSpeechSession::pushAudio(const QByteArray &pcm)
{
    if (pcm.isEmpty()) {
        return !isTerminal() && !m_finishRequested;
    }
    if (isTerminal() || m_finishRequested
        || (m_state != StreamingSpeechState::Connecting
            && m_state != StreamingSpeechState::Streaming)) {
        return false;
    }
    if (pcm.size() > m_timing.queueLimitBytes - m_audioQueue.size()) {
        fail(
            QStringLiteral("WRITE_FAILED"),
            tr8("Windows 本地语音输入积压过多，已停止本次实时识别。")
        );
        return false;
    }
    m_audioQueue.append(pcm);
    QPointer<WindowsStreamingSpeechSession> lifetimeGuard(this);
    pumpAudio();
    if (lifetimeGuard.isNull()) {
        return false;
    }
    return true;
}

void WindowsStreamingSpeechSession::finish()
{
    if (isTerminal() || m_finishRequested
        || (m_state != StreamingSpeechState::Connecting
            && m_state != StreamingSpeechState::Streaming)) {
        return;
    }
    m_finishRequested = true;
    m_state = StreamingSpeechState::Finalizing;
    QPointer<WindowsStreamingSpeechSession> lifetimeGuard(this);
    pumpAudio();
    if (lifetimeGuard.isNull()) {
        return;
    }
    closeInputIfDrained();
}

void WindowsStreamingSpeechSession::cancel()
{
    if (isTerminal()) {
        return;
    }
    m_state = StreamingSpeechState::Cancelled;
    m_terminalNotified = true;
    cleanup(true);
}

StreamingSpeechState WindowsStreamingSpeechSession::state() const
{
    return m_state;
}

void WindowsStreamingSpeechSession::onStarted()
{
    // A matching ready event, not merely process creation, ends startup.
}

void WindowsStreamingSpeechSession::onReadyReadStandardOutput()
{
    consumeOutput();
}

void WindowsStreamingSpeechSession::onReadyReadStandardError()
{
    consumeStandardError();
}

void WindowsStreamingSpeechSession::onBytesWritten(qint64 bytes)
{
    if (m_cleaningUp || isTerminal() || bytes <= 0) {
        return;
    }
    const qint64 confirmed = qMin(bytes, m_pendingWriteBytes);
    if (confirmed > 0) {
        m_audioQueue.remove(0, static_cast<int>(confirmed));
        m_pendingWriteBytes -= confirmed;
    }
    QPointer<WindowsStreamingSpeechSession> lifetimeGuard(this);
    pumpAudio();
    if (lifetimeGuard.isNull()) {
        return;
    }
    closeInputIfDrained();
}

void WindowsStreamingSpeechSession::onFinished(
    int exitCode,
    QProcess::ExitStatus exitStatus)
{
    if (m_cleaningUp || isTerminal()) {
        return;
    }
    QPointer<WindowsStreamingSpeechSession> lifetimeGuard(this);
    consumeOutput();
    if (lifetimeGuard.isNull()) {
        return;
    }
    if (isTerminal()) {
        return;
    }
    if (!m_stdoutBuffer.isEmpty()) {
        fail(
            QStringLiteral("INVALID_RESPONSE"),
            tr8("Windows 本地语音组件返回了不完整的数据。")
        );
        return;
    }
    const bool crashed = exitStatus == QProcess::CrashExit || exitCode != 0;
    fail(
        crashed ? QStringLiteral("PROCESS_CRASHED")
                : QStringLiteral("INVALID_RESPONSE"),
        crashed ? tr8("Windows 本地语音组件意外退出。")
                : tr8("Windows 本地语音组件没有返回最终结果。")
    );
}

void WindowsStreamingSpeechSession::onProcessError(
    QProcess::ProcessError error)
{
    if (m_cleaningUp || isTerminal()) {
        return;
    }
    if (error == QProcess::FailedToStart) {
        fail(
            QStringLiteral("START_FAILED"),
            tr8("Windows 本地语音组件无法启动。")
        );
    } else if (error == QProcess::Crashed) {
        fail(
            QStringLiteral("PROCESS_CRASHED"),
            tr8("Windows 本地语音组件意外退出。")
        );
    } else if (m_ready) {
        fail(
            QStringLiteral("WRITE_FAILED"),
            tr8("Windows 本地语音组件通信失败。")
        );
    } else {
        fail(
            QStringLiteral("START_FAILED"),
            tr8("Windows 本地语音组件启动失败。")
        );
    }
}

void WindowsStreamingSpeechSession::consumeOutput()
{
    if (m_cleaningUp || isTerminal()) {
        return;
    }
    QPointer<WindowsStreamingSpeechSession> lifetimeGuard(this);
    consumeStandardError();
    if (lifetimeGuard.isNull()) {
        return;
    }
    const QByteArray bytes = m_process.readAllStandardOutput();
    m_stdoutBytes += bytes.size();
    if (m_stdoutBytes > MaximumOutputBytes) {
        fail(
            QStringLiteral("INVALID_RESPONSE"),
            tr8("Windows 本地语音组件返回的数据过多。")
        );
        return;
    }
    m_stdoutBuffer.append(bytes);
    int newline = -1;
    while (!isTerminal()
           && (newline = m_stdoutBuffer.indexOf('\n')) >= 0) {
        QByteArray line = m_stdoutBuffer.left(newline);
        m_stdoutBuffer.remove(0, newline + 1);
        if (line.endsWith('\r')) {
            line.chop(1);
        }
        consumeLine(line);
        if (lifetimeGuard.isNull()) {
            return;
        }
    }
    if (!isTerminal() && m_stdoutBuffer.size() > MaximumLineBytes) {
        fail(
            QStringLiteral("INVALID_RESPONSE"),
            tr8("Windows 本地语音组件返回了无效数据。")
        );
    }
}

void WindowsStreamingSpeechSession::consumeStandardError()
{
    if (m_cleaningUp || isTerminal()) {
        return;
    }
    const QByteArray bytes = m_process.readAllStandardError();
    m_stderrBytes += bytes.size();
    if (m_stderrBytes > MaximumOutputBytes) {
        fail(
            QStringLiteral("INVALID_RESPONSE"),
            tr8("Windows 本地语音组件返回的数据过多。")
        );
    }
}

void WindowsStreamingSpeechSession::consumeLine(const QByteArray &line)
{
    const WindowsSpeechHelperEvent event =
        parseWindowsSpeechHelperEvent(line);
    if (!event.valid) {
        fail(
            QStringLiteral("INVALID_RESPONSE"),
            tr8("Windows 本地语音组件返回了无效数据。")
        );
        return;
    }
    if (event.runId != m_request.runId.trimmed()) {
        fail(
            QStringLiteral("RUN_ID_MISMATCH"),
            tr8("Windows 本地语音组件返回了其他任务的数据。")
        );
        return;
    }

    switch (event.type) {
    case WindowsSpeechHelperEventType::Ready: {
        if (m_ready) {
            fail(
                QStringLiteral("INVALID_RESPONSE"),
                tr8("Windows 本地语音组件重复报告就绪。")
            );
            return;
        }
        m_ready = true;
        m_startupTimer.stop();
        m_state = m_finishRequested ? StreamingSpeechState::Finalizing
                                    : StreamingSpeechState::Streaming;
        QPointer<WindowsStreamingSpeechSession> lifetimeGuard(this);
        pumpAudio();
        if (lifetimeGuard.isNull()) {
            return;
        }
        closeInputIfDrained();
        return;
    }
    case WindowsSpeechHelperEventType::Hypothesis:
        if (!m_ready) {
            fail(QStringLiteral("INVALID_RESPONSE"),
                 tr8("Windows 本地语音组件尚未就绪。"));
            return;
        }
        m_provisionalText = event.text;
        emitSnapshot();
        return;
    case WindowsSpeechHelperEventType::Recognized:
        if (!m_ready) {
            fail(QStringLiteral("INVALID_RESPONSE"),
                 tr8("Windows 本地语音组件尚未就绪。"));
            return;
        }
        m_committedText = appendWindowsSpeechRecognizedSegment(
            m_committedText, event.text
        );
        m_provisionalText.clear();
        emitSnapshot();
        return;
    case WindowsSpeechHelperEventType::Final:
        if (!m_ready) {
            fail(QStringLiteral("INVALID_RESPONSE"),
                 tr8("Windows 本地语音组件尚未就绪。"));
            return;
        }
        if (!m_finishRequested || !m_inputClosed
            || !event.inputStreamEnded) {
            fail(
                QStringLiteral("INVALID_RESPONSE"),
                tr8("Windows 本地语音组件在输入结束前返回了最终结果。")
            );
            return;
        }
        completeOnce(event.text.trimmed());
        return;
    case WindowsSpeechHelperEventType::Error:
        fail(event.errorCode,
             displayMessage(event.errorCode, event.errorMessage));
        return;
    case WindowsSpeechHelperEventType::Probe:
    case WindowsSpeechHelperEventType::SelfTest:
    case WindowsSpeechHelperEventType::Invalid:
        fail(QStringLiteral("INVALID_RESPONSE"),
             tr8("Windows 本地语音组件返回了无效事件。"));
        return;
    }
}

void WindowsStreamingSpeechSession::pumpAudio()
{
    if (!m_ready || isTerminal() || m_inputClosed
        || m_process.state() != QProcess::Running
        || m_pendingWriteBytes > 0 || m_audioQueue.isEmpty()) {
        return;
    }
    const int chunk = qMin(
        qMax(1, m_timing.writeChunkBytes), m_audioQueue.size()
    );
    const qint64 accepted = m_process.write(m_audioQueue.constData(), chunk);
    if (accepted <= 0) {
        fail(
            QStringLiteral("WRITE_FAILED"),
            tr8("无法把音频写入 Windows 本地语音组件。")
        );
        return;
    }
    m_pendingWriteBytes = accepted;
}

void WindowsStreamingSpeechSession::closeInputIfDrained()
{
    if (!m_ready || !m_finishRequested || m_inputClosed
        || !m_audioQueue.isEmpty() || m_pendingWriteBytes > 0
        || m_process.state() != QProcess::Running) {
        return;
    }
    m_process.closeWriteChannel();
    m_inputClosed = true;
    m_finalTimer.start(qMax(1, m_timing.finalTimeoutMs));
}

void WindowsStreamingSpeechSession::emitSnapshot()
{
    if (!m_callbacks.transcriptUpdated || isTerminal()) {
        return;
    }
    StreamingTranscriptSnapshot snapshot;
    snapshot.revision = ++m_revision;
    snapshot.committedText = m_committedText;
    snapshot.provisionalText = m_provisionalText;
    m_callbacks.transcriptUpdated(snapshot);
}

void WindowsStreamingSpeechSession::completeOnce(const QString &text)
{
    if (isTerminal()) {
        return;
    }
    m_terminalNotified = true;
    m_startupTimer.stop();
    m_finalTimer.stop();
    m_state = StreamingSpeechState::Completed;
    if (m_process.state() != QProcess::NotRunning) {
        m_process.closeWriteChannel();
        QTimer::singleShot(
            qMax(0, m_timing.killTimeoutMs),
            this,
            [this]() {
                if (m_process.state() != QProcess::NotRunning) {
                    m_process.kill();
                }
            }
        );
    }
    const std::function<void(const QString &)> completed =
        m_callbacks.completed;
    m_callbacks = StreamingSpeechCallbacks();
    if (completed) {
        completed(text.trimmed());
    }
}

void WindowsStreamingSpeechSession::fail(
    const QString &helperCode,
    const QString &message)
{
    if (isTerminal()) {
        return;
    }
    const bool startupFailure = !m_ready;
    m_terminalNotified = true;
    m_startupTimer.stop();
    m_finalTimer.stop();
    m_state = startupFailure ? StreamingSpeechState::Completed
                             : StreamingSpeechState::Degraded;
    const QString operationCode =
        windowsSpeechOperationErrorCode(helperCode);
    const std::function<void(const QString &, const QString &)> configured =
        m_callbacks.configurationFailed;
    const std::function<void(const QString &)> degraded =
        m_callbacks.degraded;
    m_callbacks = StreamingSpeechCallbacks();
    if (m_process.state() != QProcess::NotRunning) {
        m_process.terminate();
        QTimer::singleShot(
            qMax(0, m_timing.killTimeoutMs),
            this,
            [this]() {
                if (m_process.state() != QProcess::NotRunning) {
                    m_process.kill();
                }
            }
        );
    }
    if (startupFailure) {
        if (configured) {
            configured(operationCode, message);
        }
    } else if (degraded) {
        degraded(message);
    }
}

void WindowsStreamingSpeechSession::cleanup(bool cancelled)
{
    if (m_cleaningUp) {
        return;
    }
    m_cleaningUp = true;
    m_startupTimer.stop();
    m_finalTimer.stop();
    m_callbacks = StreamingSpeechCallbacks();
    m_stdoutBuffer.clear();
    m_audioQueue.clear();
    m_pendingWriteBytes = 0;
    disconnect(&m_process, nullptr, this, nullptr);
    if (m_process.state() != QProcess::NotRunning) {
        m_process.terminate();
        if (!m_process.waitForFinished(qMax(0, m_timing.killTimeoutMs))) {
            m_process.kill();
            m_process.waitForFinished(qMax(0, m_timing.killTimeoutMs));
        }
    }
    if (cancelled) {
        m_state = StreamingSpeechState::Cancelled;
    }
}

bool WindowsStreamingSpeechSession::isTerminal() const
{
    return m_terminalNotified
        || m_state == StreamingSpeechState::Completed
        || m_state == StreamingSpeechState::Degraded
        || m_state == StreamingSpeechState::Cancelled;
}
