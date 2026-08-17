#ifndef VOCEKIT_OCR_BATCH_TEXT_H
#define VOCEKIT_OCR_BATCH_TEXT_H

#include "ocr_batch_queue.h"

#include <QString>

QString ocrBatchStatusText(const OcrBatchItem &item);
QString ocrBatchCompletionText(int failedCount);

#endif // VOCEKIT_OCR_BATCH_TEXT_H
