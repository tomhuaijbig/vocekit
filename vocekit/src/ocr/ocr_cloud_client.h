#ifndef VOCEKIT_OCR_CLOUD_CLIENT_H
#define VOCEKIT_OCR_CLOUD_CLIENT_H

#include "ocr_types.h"
#include "../tasks/cancellation_token.h"

#include <QString>

struct OcrCloudConfig
{
    QString url;
    QString apiKey;
    QString model;
    int timeoutMs = 45000;
    bool useSystemProxy = false;
    QString networkPolicy;
};

QString extractCloudOcrText(const QByteArray &payload, QString *error);

class OcrCloudClient
{
public:
    OcrResult recognize(
        const OcrCloudConfig &config,
        const OcrRequest &request,
        const CancellationToken &cancellation = CancellationToken()
    ) const;
};

#endif // VOCEKIT_OCR_CLOUD_CLIENT_H
