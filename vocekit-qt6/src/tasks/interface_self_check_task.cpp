#include "interface_self_check_task.h"

#include "../config/app_settings_defaults.h"
#include "../ocr/ocr_cloud_client.h"
#include "../ocr/ocr_helper_process.h"
#include "../ocr/ocr_manager.h"
#include "../providers/built_in_provider_factory.h"
#include "../providers/windows_speech_helper_client.h"
#include "../providers/windows_speech_helper_protocol.h"
#include "diagnostic_helpers.h"

#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QTemporaryDir>
#include <QUuid>

namespace {

QString isTr8(const char *text)
{
    return QString::fromUtf8(text);
}

bool appendProviderCheck(
    QStringList *lines,
    const QString &title,
    const ProviderCheckResult &result,
    const CancellationToken &cancellation
)
{
    if (!lines || cancellation.isCancellationRequested()) {
        return false;
    }

    lines->append(diagnosticStatusLine(
        title,
        result.available ? isTr8("通过") : isTr8("失败"),
        result.available
            ? result.message
            : compactDiagnosticError(result.error.message)
    ));
    return true;
}

QString helperPath(const QString &applicationBasePath, const QString &relativePath)
{
    return QDir(applicationBasePath).filePath(relativePath);
}

OcrResult runOcrSelfCheck(
    const InterfaceSelfCheckRequest &taskRequest,
    const QString &normalizedEngine
)
{
    if (taskRequest.cancellation.isCancellationRequested()) {
        OcrResult cancelled;
        cancelled.errorCode = QStringLiteral("CANCELLED");
        cancelled.errorMessage = isTr8("OCR 测试已取消。");
        return cancelled;
    }

    QTemporaryDir directory;
    OcrResult ocrResult;
    if (!directory.isValid()) {
        ocrResult.errorCode = QStringLiteral("TEMP_DIRECTORY_FAILED");
        ocrResult.errorMessage = isTr8("无法创建 OCR 测试临时目录。");
        return ocrResult;
    }

    const QString imagePath = directory.filePath(QStringLiteral("ocr-test.png"));
    QImage image(720, 180, QImage::Format_ARGB32);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.setPen(Qt::black);
    painter.setFont(QFont(QStringLiteral("Microsoft YaHei"), 34));
    painter.drawText(image.rect(), Qt::AlignCenter, isTr8("vocekit 图片识别测试 123"));
    painter.end();

    if (!image.save(imagePath)) {
        ocrResult.errorCode = QStringLiteral("TEST_IMAGE_FAILED");
        ocrResult.errorMessage = isTr8("无法生成 OCR 测试图片。");
        return ocrResult;
    }

    OcrRequest request;
    request.requestId = QUuid::createUuid().toString();
    request.imagePath = QDir::toNativeSeparators(imagePath);
    request.languages = QStringList()
        << QStringLiteral("zh-Hans")
        << QStringLiteral("en");

    const QString rapidProgram = helperPath(
        taskRequest.applicationBasePath,
        QStringLiteral("ocr/rapidocr/vocekit-rapidocr.exe")
    );
    QString windowsProgram = helperPath(
        taskRequest.applicationBasePath,
        QStringLiteral("ocr/windows/vocekit-windows-ocr.exe")
    );
    if (!QFileInfo::exists(windowsProgram)) {
        windowsProgram = helperPath(
            taskRequest.applicationBasePath,
            QStringLiteral("helpers/bin/vocekit-windows-ocr.exe")
        );
    }

    if (normalizedEngine == ocrEngineCustomCloud()) {
        request.engine = OcrEngine::CustomCloud;
        OcrCloudConfig cloudConfig;
        cloudConfig.url = taskRequest.secrets.customOcrUrl;
        cloudConfig.apiKey = taskRequest.secrets.customOcrApiKey;
        cloudConfig.model = taskRequest.secrets.customOcrModel;
        cloudConfig.timeoutMs = taskRequest.ocrTimeoutMs;
        cloudConfig.useSystemProxy = taskRequest.useSystemProxy;
        OcrCloudClient cloudClient;
        return cloudClient.recognize(
            cloudConfig,
            request,
            taskRequest.cancellation
        );
    }

    if (normalizedEngine == ocrEngineWindows()) {
        request.engine = OcrEngine::WindowsOcr;
        OcrHelperProcess helper;
        return helper.recognize(
            windowsProgram,
            QStringList(),
            request,
            taskRequest.ocrTimeoutMs,
            taskRequest.cancellation
        );
    }

    if (normalizedEngine == ocrEngineRapid()) {
        request.engine = OcrEngine::RapidOcr;
        OcrHelperProcess helper;
        return helper.recognize(
            rapidProgram,
            QStringList(),
            request,
            taskRequest.ocrTimeoutMs,
            taskRequest.cancellation
        );
    }

    request.engine = OcrEngine::RapidOcr;
    OcrHelperProcess rapidHelper;
    const OcrResult rapidResult = rapidHelper.recognize(
        rapidProgram,
        QStringList(),
        request,
        taskRequest.ocrTimeoutMs,
        taskRequest.cancellation
    );
    if (rapidResult.ok || !shouldFallbackFromRapidOcr(rapidResult.errorCode)) {
        return rapidResult;
    }
    if (taskRequest.cancellation.isCancellationRequested()) {
        return rapidResult;
    }

    request.engine = OcrEngine::WindowsOcr;
    OcrHelperProcess windowsHelper;
    const OcrResult windowsResult = windowsHelper.recognize(
        windowsProgram,
        QStringList(),
        request,
        taskRequest.ocrTimeoutMs,
        taskRequest.cancellation
    );
    return combineAutomaticOcrResults(rapidResult, windowsResult);
}

void appendOcrSelfCheck(QStringList *lines, const InterfaceSelfCheckRequest &taskRequest)
{
    if (!lines) {
        return;
    }

    const QString normalizedEngine = normalizeOcrEngine(taskRequest.ocrEngine);
    if (normalizedEngine == ocrEngineCustomCloud()
        && taskRequest.secrets.customOcrUrl.trimmed().isEmpty()) {
        lines->append(diagnosticStatusLine(
            isTr8("图片识别接口"),
            isTr8("未填写，跳过")
        ));
        return;
    }

    const OcrResult ocrResult = runOcrSelfCheck(taskRequest, normalizedEngine);
    const QString engineName = ocrResult.engine == OcrEngine::WindowsOcr
        ? QStringLiteral("Windows OCR")
        : ocrResult.engine == OcrEngine::CustomCloud
            ? isTr8("自定义云 OCR")
            : QStringLiteral("RapidOCR");

    lines->append(diagnosticStatusLine(
        isTr8("图片识别接口"),
        ocrResult.ok ? isTr8("通过") : isTr8("失败"),
        ocrResult.ok
            ? engineName + isTr8("，识别到 ")
                + QString::number(ocrResult.text.size()) + isTr8(" 个字符")
            : compactDiagnosticError(
                ocrResult.errorCode + QStringLiteral("：") + ocrResult.errorMessage
            )
    ));
}

QStringList defaultWindowsSpeechProbe(
    const QString &programPath,
    const QString &language,
    const CancellationToken &cancellation
)
{
    WindowsSpeechHelperClient client(programPath);
    WindowsSpeechProbeRequest probe;
    probe.runId = QUuid::createUuid().toString();
    probe.language = normalizeWindowsSpeechLanguage(language);
    probe.cancellation = cancellation;
    const WindowsSpeechHelperResult result = client.probe(probe);
    if (!result.ok) {
        return QStringList()
            << result.errorCode
            << QStringLiteral("requestedLanguage=") + probe.language
            << result.errorMessage;
    }
    return QStringList()
        << QStringLiteral("OK")
        << QStringLiteral("resolvedLanguage=") + result.resolvedLanguage
        << QStringLiteral("installedLanguages=")
            + result.installedLanguages.join(QStringLiteral(","));
}

void appendWindowsSpeechSelfCheck(
    QStringList *lines,
    const InterfaceSelfCheckRequest &request
)
{
    if (!lines || request.cancellation.isCancellationRequested()) {
        return;
    }
    const QString language = normalizeWindowsSpeechLanguage(
        request.windowsSpeechLanguage
    );
    const QString programPath = windowsSpeechHelperPathForApplicationDir(
        request.applicationDirPath
    );
    const WindowsSpeechSelfCheckProbe probe = request.windowsSpeechProbe
        ? request.windowsSpeechProbe
        : WindowsSpeechSelfCheckProbe(defaultWindowsSpeechProbe);
    const QStringList probeLines = probe(
        programPath,
        language,
        request.cancellation
    );
    if (request.cancellation.isCancellationRequested()) {
        return;
    }
    const bool ok = probeLines.contains(QStringLiteral("OK"));
    lines->append(diagnosticStatusLine(
        QStringLiteral("Windows ") + isTr8("本地语音识别"),
        ok ? isTr8("通过") : isTr8("失败"),
        probeLines.join(QStringLiteral("；"))
    ));
}

} // namespace

QStringList runInterfaceSelfCheckTask(const InterfaceSelfCheckRequest &request)
{
    QStringList lines;
    if (request.cancellation.isCancellationRequested()) {
        return lines;
    }

    ProviderRegistry registry;
    registerBuiltInProviders(&registry, request.useSystemProxy);
    const bool all = request.target.isEmpty() || request.target == QStringLiteral("all");

    if (all || request.target == QStringLiteral("windows_speech")) {
        appendWindowsSpeechSelfCheck(&lines, request);
        if (request.cancellation.isCancellationRequested()) {
            return QStringList();
        }
    }

    if (all || request.target == QStringLiteral("baidu")) {
        if (request.secrets.hasBaidu()) {
            if (!appendProviderCheck(
                &lines,
                isTr8("百度语音识别"),
                registry.checkSpeechProvider(
                    speechProviderBaidu(),
                    request.cancellation
                ),
                request.cancellation
            )) {
                return QStringList();
            }
        } else {
            lines << diagnosticStatusLine(isTr8("百度语音识别"), isTr8("未填写，跳过"));
        }
    }

    if (all || request.target == QStringLiteral("xfyun")) {
        if (request.secrets.hasXfyun()) {
            if (!appendProviderCheck(
                &lines,
                isTr8("讯飞语音听写"),
                registry.checkSpeechProvider(
                    speechProviderXfyun(),
                    request.cancellation
                ),
                request.cancellation
            )) {
                return QStringList();
            }
        } else {
            lines << diagnosticStatusLine(isTr8("讯飞语音听写"), isTr8("未填写，跳过"));
        }
    }

    if (all || request.target == QStringLiteral("custom_speech")) {
        if (request.secrets.hasCustomSpeech()) {
            if (!appendProviderCheck(
                &lines,
                isTr8("自定义语音接口"),
                registry.checkSpeechProvider(
                    speechProviderCustom(),
                    request.cancellation
                ),
                request.cancellation
            )) {
                return QStringList();
            }
        } else {
            lines << diagnosticStatusLine(isTr8("自定义语音接口"), isTr8("未填写，跳过"));
        }
    }

    if (all || request.target == QStringLiteral("ocr")) {
        appendOcrSelfCheck(&lines, request);
        if (request.cancellation.isCancellationRequested()) {
            return QStringList();
        }
    }

    if (all || request.target == QStringLiteral("deepseek")) {
        if (request.secrets.hasDeepSeek()) {
            if (!appendProviderCheck(
                &lines,
                QStringLiteral("DeepSeek"),
                registry.checkModelProvider(
                    QStringLiteral("deepseek"),
                    request.cancellation
                ),
                request.cancellation
            )) {
                return QStringList();
            }
        } else {
            lines << diagnosticStatusLine(QStringLiteral("DeepSeek"), isTr8("未填写，跳过"));
        }
    }

    if (all || request.target == QStringLiteral("openai")) {
        if (request.secrets.hasOpenAI()) {
            if (!appendProviderCheck(
                &lines,
                QStringLiteral("OpenAI"),
                registry.checkModelProvider(
                    QStringLiteral("openai"),
                    request.cancellation
                ),
                request.cancellation
            )) {
                return QStringList();
            }
        } else {
            lines << diagnosticStatusLine(QStringLiteral("OpenAI"), isTr8("未填写，跳过"));
        }
    }

    if (all || request.target == QStringLiteral("claude")) {
        if (request.secrets.hasAnthropic()) {
            if (!appendProviderCheck(
                &lines,
                QStringLiteral("Claude"),
                registry.checkModelProvider(
                    QStringLiteral("claude"),
                    request.cancellation
                ),
                request.cancellation
            )) {
                return QStringList();
            }
        } else {
            lines << diagnosticStatusLine(QStringLiteral("Claude"), isTr8("未填写，跳过"));
        }
    }

    const QVector<CustomModelProfile> profiles = request.secrets.effectiveCustomModels();
    for (const CustomModelProfile &profile : profiles) {
        if (request.cancellation.isCancellationRequested()) {
            return QStringList();
        }
        const QString provider = QStringLiteral("custom:")
            + normalizeCustomModelProfileId(profile.id);
        if (!all && request.target != provider) {
            continue;
        }
        if (!appendProviderCheck(
            &lines,
            profile.name.trimmed().isEmpty() ? isTr8("自定义大模型") : profile.name.trimmed(),
            registry.checkModelProvider(provider, request.cancellation),
            request.cancellation
        )) {
            return QStringList();
        }
    }
    if ((all || request.target.startsWith(QStringLiteral("custom:"))) && profiles.isEmpty()) {
        lines << diagnosticStatusLine(isTr8("自定义大模型"), isTr8("未填写，跳过"));
    }

    return lines;
}
