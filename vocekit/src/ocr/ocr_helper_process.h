#ifndef VOCEKIT_OCR_HELPER_PROCESS_H
#define VOCEKIT_OCR_HELPER_PROCESS_H

#include "ocr_types.h"
#include "../tasks/cancellation_token.h"

#include <QObject>
#include <QPointer>

class QProcess;

OcrResult parseOcrHelperResponse(const QByteArray &payload, OcrEngine engine);

class OcrHelperProcess : public QObject
{
public:
    explicit OcrHelperProcess(QObject *parent = nullptr);

    OcrResult recognize(
        const QString &program,
        const QStringList &arguments,
        const OcrRequest &request,
        int timeoutMs,
        const CancellationToken &cancellation = CancellationToken()
    );

    void stop();

private:
    QPointer<QProcess> m_process;
};

#endif // VOCEKIT_OCR_HELPER_PROCESS_H
