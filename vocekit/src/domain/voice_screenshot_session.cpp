#include "voice_screenshot_session.h"

void VoiceScreenshotSession::beginWorkflow()
{
    ++m_generation;
    m_active = true;
    m_contextPending = false;
    m_cloudConsent = false;
    m_pendingCapture = VoiceScreenshotCapture();
    clearRecognitionState();
}

void VoiceScreenshotSession::beginCaptureAttempt()
{
    ++m_generation;
    m_contextPending = false;
    clearRecognitionState();
}

void VoiceScreenshotSession::reset(bool keepPendingContext)
{
    ++m_generation;
    m_active = false;
    m_cloudConsent = false;
    m_pendingCapture = VoiceScreenshotCapture();
    if (!keepPendingContext) {
        m_contextPending = false;
        clearRecognitionState();
    }
}

void VoiceScreenshotSession::deactivate()
{
    m_active = false;
}

bool VoiceScreenshotSession::isActive() const
{
    return m_active;
}

int VoiceScreenshotSession::generation() const
{
    return m_generation;
}

bool VoiceScreenshotSession::matchesGeneration(int expectedGeneration) const
{
    return expectedGeneration == m_generation;
}

void VoiceScreenshotSession::setCloudConsent(bool consent)
{
    m_cloudConsent = consent;
}

bool VoiceScreenshotSession::cloudConsent() const
{
    return m_cloudConsent;
}

void VoiceScreenshotSession::setTemporaryPath(const QString &path)
{
    m_temporaryPath = path;
}

QString VoiceScreenshotSession::temporaryPath() const
{
    return m_temporaryPath;
}

QString VoiceScreenshotSession::takeTemporaryPath()
{
    const QString path = m_temporaryPath;
    m_temporaryPath.clear();
    return path;
}

void VoiceScreenshotSession::queuePendingCapture(
    const QImage &image,
    const QRect &rect)
{
    m_pendingCapture.image = image;
    m_pendingCapture.rect = rect;
}

bool VoiceScreenshotSession::hasPendingCapture() const
{
    return !m_pendingCapture.image.isNull();
}

VoiceScreenshotCapture VoiceScreenshotSession::takePendingCapture()
{
    const VoiceScreenshotCapture capture = m_pendingCapture;
    m_pendingCapture = VoiceScreenshotCapture();
    return capture;
}

void VoiceScreenshotSession::setCapture(
    const QImage &image,
    const QRect &rect)
{
    m_image = image;
    m_rect = rect;
}

void VoiceScreenshotSession::setRecognitionResult(const OcrResult &result)
{
    m_recognizedText = result.text.trimmed();
    m_blocks = result.blocks;
    m_engine = result.engine;
    m_elapsedMs = result.elapsedMs;
    m_usedFallback = result.usedFallback;
    m_contextPending = !m_recognizedText.isEmpty();
}

bool VoiceScreenshotSession::hasRecognizedText() const
{
    return !m_recognizedText.trimmed().isEmpty();
}

QString VoiceScreenshotSession::recognizedText() const
{
    return m_recognizedText;
}

bool VoiceScreenshotSession::hasPendingContext() const
{
    return m_contextPending;
}

void VoiceScreenshotSession::markContextPending()
{
    m_contextPending = hasRecognizedText();
}

bool VoiceScreenshotSession::applyPendingContext(VoiceRunContext *context)
{
    if (!context || !m_contextPending) {
        return false;
    }

    context->screenshotInput = true;
    context->screenshotImage = m_image;
    context->screenshotBlocks = m_blocks;
    context->screenshotRecognizedText = m_recognizedText;
    context->screenshotOcrEngine = m_engine;
    context->screenshotOcrElapsedMs = m_elapsedMs;
    context->screenshotOcrUsedFallback = m_usedFallback;
    context->screenshotRect = m_rect;
    m_contextPending = false;
    return true;
}

void VoiceScreenshotSession::clearRecognitionState()
{
    m_image = QImage();
    m_blocks.clear();
    m_recognizedText.clear();
    m_engine = OcrEngine::Automatic;
    m_elapsedMs = -1;
    m_usedFallback = false;
    m_rect = QRect();
}
