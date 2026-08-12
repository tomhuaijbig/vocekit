#include "windows_speech_helper_client.h"

#include "windows_speech_helper_protocol.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QProcess>

namespace {

const int StartTimeoutMs = 5000;
const int PollIntervalMs = 50;
const int TerminateWaitMs = 250;
const int PcmChunkBytes = 32 * 1024;
const qint64 MaximumQueuedBytes = 64 * 1024;
const qint64 MaximumChannelBytes = 1024 * 1024;
const int MaximumLineBytes = 64 * 1024;

struct ExecutionRequest
{
    QString runId;
    QStringList arguments;
    QByteArray pcm;
    int timeoutMs = 0;
    CancellationToken cancellation;
    bool probe = false;
};

WindowsSpeechHelperResult failedResult(
    const QString &code,
    const QString &message,
    qint64 maximumBytesQueued = 0)
{
    WindowsSpeechHelperResult result;
    result.errorCode = code;
    result.errorMessage = message;
    result.maximumBytesQueued = maximumBytesQueued;
    return result;
}

int remainingMilliseconds(
    const QElapsedTimer &elapsed,
    int deadlineMs)
{
    const qint64 remaining = static_cast<qint64>(deadlineMs)
        - elapsed.elapsed();
    if (remaining <= 0) {
        return 0;
    }
    return static_cast<int>(qMin<qint64>(remaining, PollIntervalMs));
}

void stopProcess(QProcess *process)
{
    if (!process || process->state() == QProcess::NotRunning) {
        return;
    }
    process->terminate();
    if (!process->waitForFinished(TerminateWaitMs)) {
        process->kill();
        process->waitForFinished(TerminateWaitMs);
        if (process->state() != QProcess::NotRunning) {
            process->kill();
            process->waitForFinished(TerminateWaitMs);
        }
    }
}

class ProcessReaper
{
public:
    explicit ProcessReaper(QProcess *process)
        : m_process(process)
    {
    }

    ~ProcessReaper()
    {
        stopProcess(m_process);
    }

private:
    QProcess *m_process;
};

class OutputInterpreter
{
public:
    OutputInterpreter(const QString &runId, bool probe)
        : m_runId(runId),
          m_probe(probe)
    {
    }

    bool consume(QProcess *process)
    {
        const QByteArray standardOutput = process->readAllStandardOutput();
        const QByteArray standardError = process->readAllStandardError();
        m_stdoutBytes += standardOutput.size();
        m_stderrBytes += standardError.size();
        if (m_stdoutBytes > MaximumChannelBytes
            || m_stderrBytes > MaximumChannelBytes) {
            fail(
                QStringLiteral("OUTPUT_TOO_LARGE"),
                QStringLiteral("The speech helper produced too much output.")
            );
            return false;
        }

        m_stdoutBuffer.append(standardOutput);
        int newline = -1;
        while ((newline = m_stdoutBuffer.indexOf('\n')) >= 0) {
            QByteArray line = m_stdoutBuffer.left(newline);
            m_stdoutBuffer.remove(0, newline + 1);
            if (line.endsWith('\r')) {
                line.chop(1);
            }
            if (!consumeLine(line)) {
                return false;
            }
        }
        if (m_stdoutBuffer.size() > MaximumLineBytes) {
            fail(
                QStringLiteral("INVALID_RESPONSE"),
                QStringLiteral("The speech helper response was invalid.")
            );
            return false;
        }
        return true;
    }

    bool finish()
    {
        if (!m_stdoutBuffer.isEmpty()) {
            fail(
                QStringLiteral("INVALID_RESPONSE"),
                QStringLiteral("The speech helper response was incomplete.")
            );
            return false;
        }
        return !m_failed;
    }

    bool failed() const
    {
        return m_failed;
    }

    QString errorCode() const
    {
        return m_errorCode;
    }

    QString errorMessage() const
    {
        return m_errorMessage;
    }

    bool ready() const
    {
        return m_ready;
    }

    bool terminalSeen() const
    {
        return m_terminalSeen;
    }

    WindowsSpeechHelperEvent terminal() const
    {
        return m_terminal;
    }

private:
    bool consumeLine(const QByteArray &line)
    {
        const WindowsSpeechHelperEvent event =
            parseWindowsSpeechHelperEvent(line);
        if (!event.valid) {
            fail(
                QStringLiteral("INVALID_RESPONSE"),
                QStringLiteral("The speech helper response was invalid.")
            );
            return false;
        }
        if (event.runId != m_runId) {
            fail(
                QStringLiteral("RUN_ID_MISMATCH"),
                QStringLiteral("The speech helper returned another run.")
            );
            return false;
        }
        if (m_terminalSeen) {
            fail(
                QStringLiteral("INVALID_RESPONSE"),
                QStringLiteral("The speech helper returned duplicate output.")
            );
            return false;
        }

        if (m_probe) {
            if (event.type != WindowsSpeechHelperEventType::Probe
                && event.type != WindowsSpeechHelperEventType::Error) {
                fail(
                    QStringLiteral("INVALID_RESPONSE"),
                    QStringLiteral("The speech helper response was invalid.")
                );
                return false;
            }
            setTerminal(event);
            return true;
        }

        switch (event.type) {
        case WindowsSpeechHelperEventType::Ready:
            if (m_ready) {
                fail(
                    QStringLiteral("INVALID_RESPONSE"),
                    QStringLiteral("The speech helper returned duplicate output.")
                );
                return false;
            }
            m_ready = true;
            return true;
        case WindowsSpeechHelperEventType::Hypothesis:
        case WindowsSpeechHelperEventType::Recognized:
            if (!m_ready) {
                fail(
                    QStringLiteral("INVALID_RESPONSE"),
                    QStringLiteral("The speech helper was not ready.")
                );
                return false;
            }
            return true;
        case WindowsSpeechHelperEventType::Final:
            if (!m_ready) {
                fail(
                    QStringLiteral("INVALID_RESPONSE"),
                    QStringLiteral("The speech helper was not ready.")
                );
                return false;
            }
            setTerminal(event);
            return true;
        case WindowsSpeechHelperEventType::Error:
            setTerminal(event);
            return true;
        case WindowsSpeechHelperEventType::Probe:
        case WindowsSpeechHelperEventType::SelfTest:
        case WindowsSpeechHelperEventType::Invalid:
            fail(
                QStringLiteral("INVALID_RESPONSE"),
                QStringLiteral("The speech helper response was invalid.")
            );
            return false;
        }
        return false;
    }

    void setTerminal(const WindowsSpeechHelperEvent &event)
    {
        m_terminalSeen = true;
        m_terminal = event;
    }

    void fail(const QString &code, const QString &message)
    {
        m_failed = true;
        m_errorCode = code;
        m_errorMessage = message;
    }

    QString m_runId;
    bool m_probe = false;
    QByteArray m_stdoutBuffer;
    qint64 m_stdoutBytes = 0;
    qint64 m_stderrBytes = 0;
    bool m_ready = false;
    bool m_terminalSeen = false;
    WindowsSpeechHelperEvent m_terminal;
    bool m_failed = false;
    QString m_errorCode;
    QString m_errorMessage;
};

WindowsSpeechHelperResult execute(
    const QString &programPath,
    const ExecutionRequest &request)
{
    if (request.cancellation.isCancellationRequested()) {
        return failedResult(
            QStringLiteral("CANCELLED"),
            QStringLiteral("Windows speech recognition was cancelled.")
        );
    }
    if (request.runId.trimmed().isEmpty()) {
        return failedResult(
            QStringLiteral("INVALID_RESPONSE"),
            QStringLiteral("The speech helper run identifier is missing.")
        );
    }
    const QFileInfo program(programPath);
    if (!program.exists() || !program.isFile()) {
        return failedResult(
            QStringLiteral("PROGRAM_MISSING"),
            QStringLiteral("The Windows speech helper is missing.")
        );
    }

    const int deadlineMs = qMax(1, request.timeoutMs);
    QElapsedTimer elapsed;
    elapsed.start();

    QProcess process;
    ProcessReaper reaper(&process);
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start(programPath, request.arguments, QIODevice::ReadWrite);

    while (process.state() == QProcess::Starting) {
        if (request.cancellation.isCancellationRequested()) {
            return failedResult(
                QStringLiteral("CANCELLED"),
                QStringLiteral("Windows speech recognition was cancelled.")
            );
        }
        const int sharedRemaining = remainingMilliseconds(
            elapsed, deadlineMs
        );
        const int startRemaining = StartTimeoutMs
            - static_cast<int>(elapsed.elapsed());
        if (sharedRemaining <= 0) {
            return failedResult(
                QStringLiteral("TIMEOUT"),
                QStringLiteral("The Windows speech helper timed out.")
            );
        }
        if (startRemaining <= 0) {
            return failedResult(
                QStringLiteral("START_FAILED"),
                QStringLiteral("The Windows speech helper could not start.")
            );
        }
        process.waitForStarted(qMin(sharedRemaining, startRemaining));
    }
    if (process.state() != QProcess::Running) {
        return failedResult(
            QStringLiteral("START_FAILED"),
            QStringLiteral("The Windows speech helper could not start.")
        );
    }

    OutputInterpreter output(request.runId, request.probe);
    qint64 maximumBytesQueued = 0;
    int pcmOffset = 0;
    bool writeChannelClosed = false;
    if (request.probe) {
        process.closeWriteChannel();
        writeChannelClosed = true;
    }

    while (true) {
        if (!output.consume(&process)) {
            return failedResult(
                output.errorCode(),
                output.errorMessage(),
                maximumBytesQueued
            );
        }
        if (request.cancellation.isCancellationRequested()) {
            return failedResult(
                QStringLiteral("CANCELLED"),
                QStringLiteral("Windows speech recognition was cancelled."),
                maximumBytesQueued
            );
        }
        const int waitMs = remainingMilliseconds(elapsed, deadlineMs);
        if (waitMs <= 0) {
            return failedResult(
                QStringLiteral("TIMEOUT"),
                QStringLiteral("The Windows speech helper timed out."),
                maximumBytesQueued
            );
        }

        if (process.state() == QProcess::NotRunning) {
            if (!output.consume(&process) || !output.finish()) {
                return failedResult(
                    output.errorCode(),
                    output.errorMessage(),
                    maximumBytesQueued
                );
            }
            if (!output.terminalSeen()
                && (process.exitStatus() == QProcess::CrashExit
                    || process.error() == QProcess::Crashed
                    || process.exitCode() != 0)) {
                return failedResult(
                    QStringLiteral("PROCESS_CRASHED"),
                    QStringLiteral("The Windows speech helper crashed."),
                    maximumBytesQueued
                );
            }
            break;
        }

        if (!request.probe && output.ready() && !writeChannelClosed) {
            const qint64 queued = process.bytesToWrite();
            maximumBytesQueued = qMax(maximumBytesQueued, queued);
            if (queued > 0) {
                process.waitForBytesWritten(waitMs);
                maximumBytesQueued = qMax(
                    maximumBytesQueued, process.bytesToWrite()
                );
                continue;
            }

            if (pcmOffset < request.pcm.size()) {
                const int chunkSize = qMin(
                    PcmChunkBytes, request.pcm.size() - pcmOffset
                );
                if (process.bytesToWrite() + chunkSize
                    >= MaximumQueuedBytes) {
                    process.waitForBytesWritten(waitMs);
                    continue;
                }
                const qint64 written = process.write(
                    request.pcm.constData() + pcmOffset,
                    chunkSize
                );
                if (written <= 0) {
                    process.waitForFinished(0);
                    if (process.exitStatus() == QProcess::CrashExit
                        || process.error() == QProcess::Crashed) {
                        return failedResult(
                            QStringLiteral("PROCESS_CRASHED"),
                            QStringLiteral("The Windows speech helper crashed."),
                            maximumBytesQueued
                        );
                    }
                    return failedResult(
                        QStringLiteral("WRITE_FAILED"),
                        QStringLiteral("PCM could not be written to the speech helper."),
                        maximumBytesQueued
                    );
                }
                pcmOffset += static_cast<int>(written);
                maximumBytesQueued = qMax(
                    maximumBytesQueued, process.bytesToWrite()
                );
                continue;
            }

            process.closeWriteChannel();
            writeChannelClosed = true;
        }

        if (process.bytesToWrite() > 0) {
            process.waitForBytesWritten(waitMs);
        } else {
            process.waitForReadyRead(waitMs);
        }
    }

    if (process.exitStatus() == QProcess::CrashExit
        || process.error() == QProcess::Crashed) {
        return failedResult(
            QStringLiteral("PROCESS_CRASHED"),
            QStringLiteral("The Windows speech helper crashed."),
            maximumBytesQueued
        );
    }
    if (!output.terminalSeen()) {
        return failedResult(
            QStringLiteral("INVALID_RESPONSE"),
            QStringLiteral("The speech helper returned no final response."),
            maximumBytesQueued
        );
    }

    const WindowsSpeechHelperEvent terminal = output.terminal();
    if (terminal.type == WindowsSpeechHelperEventType::Error) {
        return failedResult(
            terminal.errorCode,
            terminal.errorMessage,
            maximumBytesQueued
        );
    }
    if (!request.probe
        && terminal.type == WindowsSpeechHelperEventType::Final
        && terminal.text.trimmed().isEmpty()) {
        return failedResult(
            QStringLiteral("EMPTY_TEXT"),
            QStringLiteral("Windows speech recognition returned no text."),
            maximumBytesQueued
        );
    }

    WindowsSpeechHelperResult result;
    result.ok = true;
    result.text = terminal.text;
    result.resolvedLanguage = terminal.resolvedLanguage;
    result.installedLanguages = terminal.installedLanguages;
    result.pcmBytesObserved = terminal.pcmBytesObserved;
    result.maximumBytesQueued = maximumBytesQueued;
    return result;
}

} // namespace

int windowsSpeechBatchTimeoutMs(qint64 pcmByteCount)
{
    const qint64 safeBytes = qMax<qint64>(0, pcmByteCount);
    const qint64 audioDurationMs = safeBytes * 1000 / 32000;
    const qint64 derived = audioDurationMs * 3 / 2 + 10000;
    return static_cast<int>(
        qMin<qint64>(2100000, qMax<qint64>(15000, derived))
    );
}

WindowsSpeechHelperClient::WindowsSpeechHelperClient(
    const QString &programPath,
    const QStringList &prependedArguments)
    : m_programPath(programPath),
      m_prependedArguments(prependedArguments)
{
}

WindowsSpeechHelperResult WindowsSpeechHelperClient::recognize(
    const WindowsSpeechBatchRequest &request) const
{
    ExecutionRequest execution;
    execution.runId = request.runId;
    execution.arguments = m_prependedArguments
        + windowsSpeechHelperArguments(
            QStringLiteral("batch"),
            request.runId,
            request.language,
            16000,
            1,
            16
        );
    execution.pcm = request.pcm;
    execution.timeoutMs = request.timeoutMs > 0
        ? request.timeoutMs
        : windowsSpeechBatchTimeoutMs(request.pcm.size());
    execution.cancellation = request.cancellation;
    return execute(m_programPath, execution);
}

WindowsSpeechHelperResult WindowsSpeechHelperClient::probe(
    const WindowsSpeechProbeRequest &request) const
{
    ExecutionRequest execution;
    execution.runId = request.runId;
    execution.arguments = m_prependedArguments;
    execution.arguments
        << QStringLiteral("--probe")
        << request.language
        << QStringLiteral("--run-id")
        << request.runId;
    execution.timeoutMs = request.timeoutMs > 0
        ? request.timeoutMs
        : StartTimeoutMs;
    execution.cancellation = request.cancellation;
    execution.probe = true;
    return execute(m_programPath, execution);
}
