#include "windows_speech_provider.h"

#include "../config/app_settings_defaults.h"
#include "../runtime_log.h"
#include "windows_speech_helper_protocol.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QUuid>

namespace {

QString freshRunId()
{
    QString runId = QUuid::createUuid().toString();
    runId.remove(QLatin1Char('{'));
    runId.remove(QLatin1Char('}'));
    return runId;
}

QString helperPath()
{
    return windowsSpeechHelperPathForApplicationDir(
        QCoreApplication::applicationDirPath()
    );
}

OperationError operationError(
    const QString &code,
    const QString &message,
    const QString &detail = QString())
{
    OperationError error;
    error.code = code;
    error.message = message;
    error.detail = detail;
    return error;
}

OperationError cancellationError()
{
    return operationError(
        QStringLiteral("operation.cancelled"),
        QString::fromUtf8("Windows 本地语音识别已取消。")
    );
}

OperationError helperError(const WindowsSpeechHelperResult &helper)
{
    const QString code = windowsSpeechOperationErrorCode(helper.errorCode);
    QString message;
    if (code == QStringLiteral("speech.windows.program_missing")) {
        message = QString::fromUtf8("Windows 本地语音助手缺失，请修复软件安装。 ");
    } else if (code == QStringLiteral("speech.windows.recognizer_missing")) {
        message = QString::fromUtf8("所选 Windows 语音识别语言未安装。 ");
    } else if (code == QStringLiteral("speech.windows.runtime_missing")) {
        message = QString::fromUtf8("Windows 本地语音运行组件不可用。 ");
    } else if (code == QStringLiteral("speech.windows.grammar_load_failed")) {
        message = QString::fromUtf8("Windows 听写语法加载失败。 ");
    } else if (code == QStringLiteral("speech.empty_result")) {
        message = QString::fromUtf8("没有识别到语音。 ");
    } else if (code == QStringLiteral("operation.cancelled")) {
        message = QString::fromUtf8("Windows 本地语音识别已取消。 ");
    } else {
        message = QString::fromUtf8("Windows 本地语音识别失败。 ");
    }
    return operationError(
        code,
        message.trimmed(),
        helper.errorMessage
    );
}

QString languageSummary(const WindowsSpeechHelperResult &helper)
{
    return QString::fromUtf8("已解析语言：")
        + helper.resolvedLanguage
        + QString::fromUtf8("；已安装语言：")
        + helper.installedLanguages.join(QStringLiteral(", "));
}

} // namespace

qint64 windowsSpeechPcmDurationMs(qint64 pcmByteCount)
{
    return qMax<qint64>(0, pcmByteCount) * qint64(1000) / qint64(32000);
}

WindowsSpeechProvider::WindowsSpeechProvider()
    : WindowsSpeechProvider(
          [](const WindowsSpeechBatchRequest &request) {
              return WindowsSpeechHelperClient(helperPath()).recognize(request);
          },
          [](const WindowsSpeechProbeRequest &request) {
              return WindowsSpeechHelperClient(helperPath()).probe(request);
          }
      )
{
}

WindowsSpeechProvider::WindowsSpeechProvider(
    const BatchFunction &batch,
    const ProbeFunction &probe)
    : m_batch(batch),
      m_probe(probe)
{
}

QString WindowsSpeechProvider::id() const
{
    return speechProviderWindowsLocal();
}

ProviderCheckResult WindowsSpeechProvider::checkConfiguration(
    const CancellationToken &cancellation) const
{
    ProviderCheckResult result;
    CancellationSource ownedCancellation;
    const CancellationToken effectiveCancellation = cancellation.isValid()
        ? cancellation
        : ownedCancellation.token();
    if (effectiveCancellation.isCancellationRequested()) {
        result.error = cancellationError();
        return result;
    }
    if (!m_probe) {
        result.error = operationError(
            QStringLiteral("speech.windows.local"),
            QString::fromUtf8("Windows 本地语音自检未配置。")
        );
        return result;
    }

    WindowsSpeechProbeRequest request;
    request.runId = freshRunId();
    request.language = windowsSpeechLanguageFollowWindows();
    request.cancellation = effectiveCancellation;
    QElapsedTimer timer;
    timer.start();
    const WindowsSpeechHelperResult helper = m_probe(request);
    result.durationMs = timer.elapsed();

    if (effectiveCancellation.isCancellationRequested()) {
        result.error = cancellationError();
    } else if (!helper.ok) {
        result.error = helperError(helper);
    } else {
        result.available = true;
        result.message = languageSummary(helper);
    }
    return result;
}

SpeechRecognitionResult WindowsSpeechProvider::recognize(
    const SpeechRecognitionRequest &request,
    const CancellationToken &cancellation)
{
    SpeechRecognitionResult result;
    CancellationSource ownedCancellation;
    const CancellationToken effectiveCancellation = cancellation.isValid()
        ? cancellation
        : ownedCancellation.token();
    result.executionId = request.executionId.isValid()
        ? request.executionId
        : effectiveCancellation.executionId();
    if (effectiveCancellation.isCancellationRequested()) {
        result.error = cancellationError();
        return result;
    }
    if (request.audioData.isEmpty()
        || request.audioData.size() % 2 != 0
        || request.audioFormat.trimmed().compare(
               QStringLiteral("pcm"), Qt::CaseInsensitive
           ) != 0
        || request.sampleRate != 16000) {
        result.error = operationError(
            QStringLiteral("speech.invalid_audio"),
            QString::fromUtf8(
                "Windows 本地语音识别需要 16 kHz、单声道、16 位 PCM 音频。"
            )
        );
        return result;
    }
    if (!m_batch) {
        result.error = operationError(
            QStringLiteral("speech.windows.local"),
            QString::fromUtf8("Windows 本地语音识别未配置。")
        );
        return result;
    }

    WindowsSpeechBatchRequest batch;
    batch.runId = freshRunId();
    batch.language = normalizeWindowsSpeechLanguage(request.language);
    batch.pcm = request.audioData;
    batch.timeoutMs = windowsSpeechBatchTimeoutMs(batch.pcm.size());
    batch.cancellation = effectiveCancellation;

    QElapsedTimer timer;
    timer.start();
    logRuntimeEvent(
        QStringLiteral("Windows local speech"),
        QStringLiteral("start"),
        QStringLiteral("provider=") + id()
            + QStringLiteral(", language=") + batch.language
            + QStringLiteral(", durationMs=")
            + QString::number(windowsSpeechPcmDurationMs(batch.pcm.size()))
    );
    const WindowsSpeechHelperResult helper = m_batch(batch);
    result.durationMs = timer.elapsed();

    if (effectiveCancellation.isCancellationRequested()) {
        result.error = cancellationError();
    } else if (!helper.ok) {
        result.error = helperError(helper);
    } else {
        result.text = helper.text.trimmed();
        if (result.text.isEmpty()) {
            result.error = operationError(
                QStringLiteral("speech.empty_result"),
                QString::fromUtf8("Windows 本地语音识别没有返回文字。")
            );
        }
    }

    logRuntimeEvent(
        QStringLiteral("Windows local speech"),
        result.error.isEmpty() ? QStringLiteral("complete")
                               : QStringLiteral("failed"),
        QStringLiteral("provider=") + id()
            + QStringLiteral(", language=") + batch.language
            + QStringLiteral(", durationMs=")
            + QString::number(result.durationMs)
            + QStringLiteral(", textLength=")
            + QString::number(result.text.size()),
        result.durationMs
    );
    if (!result.error.isEmpty()) {
        result.text.clear();
    }
    return result;
}

QSharedPointer<ISpeechProvider> createWindowsSpeechProvider()
{
    return QSharedPointer<ISpeechProvider>(new WindowsSpeechProvider());
}
