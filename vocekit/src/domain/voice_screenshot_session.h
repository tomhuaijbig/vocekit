#ifndef VOCEKIT_VOICE_SCREENSHOT_SESSION_H
#define VOCEKIT_VOICE_SCREENSHOT_SESSION_H

#include "voice_run_context.h"

#include <QImage>
#include <QRect>
#include <QString>

// A queued screenshot selection waiting for the current OCR request to stop.
struct VoiceScreenshotCapture
{
    QImage image;
    QRect rect;
};

// Owns the pure per-workflow screenshot state. UI objects and file cleanup stay
// in VoiceController; this class only controls lifecycle and one-shot context.
class VoiceScreenshotSession
{
public:
    void beginWorkflow();
    void beginCaptureAttempt();
    void reset(bool keepPendingContext = false);
    void deactivate();

    bool isActive() const;
    int generation() const;
    bool matchesGeneration(int expectedGeneration) const;

    void setCloudConsent(bool consent);
    bool cloudConsent() const;

    void setTemporaryPath(const QString &path);
    QString temporaryPath() const;
    QString takeTemporaryPath();

    void queuePendingCapture(const QImage &image, const QRect &rect);
    bool hasPendingCapture() const;
    VoiceScreenshotCapture takePendingCapture();

    void setCapture(const QImage &image, const QRect &rect);
    void setRecognitionResult(const OcrResult &result);
    bool hasRecognizedText() const;
    QString recognizedText() const;

    bool hasPendingContext() const;
    void markContextPending();
    bool applyPendingContext(VoiceRunContext *context);

private:
    void clearRecognitionState();

    bool m_active = false;
    bool m_contextPending = false;
    bool m_cloudConsent = false;
    int m_generation = 0;
    QString m_temporaryPath;
    VoiceScreenshotCapture m_pendingCapture;
    QImage m_image;
    QVector<OcrTextBlock> m_blocks;
    QString m_recognizedText;
    OcrEngine m_engine = OcrEngine::Automatic;
    qint64 m_elapsedMs = -1;
    bool m_usedFallback = false;
    QRect m_rect;
};

#endif // VOCEKIT_VOICE_SCREENSHOT_SESSION_H
