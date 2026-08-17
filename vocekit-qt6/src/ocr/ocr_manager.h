#ifndef VOCEKIT_OCR_MANAGER_H
#define VOCEKIT_OCR_MANAGER_H

#include "ocr_cloud_client.h"
#include "ocr_types.h"
#include "../tasks/cancellation_token.h"

#include <QObject>

#include <functional>

template <typename T>
class QFutureWatcher;

struct OcrManagerConfig
{
    QString rapidOcrProgram;
    QStringList rapidOcrArguments;
    QString windowsOcrProgram;
    QStringList windowsOcrArguments;
    OcrCloudConfig customCloud;
    int timeoutMs = 45000;
};

bool shouldFallbackFromRapidOcr(const QString &errorCode);
OcrResult combineAutomaticOcrResults(
    const OcrResult &rapidResult,
    const OcrResult &windowsResult
);

class OcrManager : public QObject
{
public:
    explicit OcrManager(QObject *parent = nullptr);
    ~OcrManager() override;

    void setConfig(const OcrManagerConfig &config);
    OcrManagerConfig config() const;

    bool isBusy() const;
    void recognize(const OcrRequest &request);
    void cancel();

    std::function<void(const QString &)> statusCallback;
    std::function<void(const OcrResult &)> finishedCallback;

private:
    static OcrResult runRecognition(
        const OcrRequest &request,
        const OcrManagerConfig &config,
        const CancellationToken &cancellation
    );
    static OcrResult cancelledResult(OcrEngine engine);

    OcrManagerConfig m_config;
    bool m_busy = false;
    QFutureWatcher<OcrResult> *m_watcher = nullptr;
    CancellationSource m_cancellation;
};

#endif // VOCEKIT_OCR_MANAGER_H
