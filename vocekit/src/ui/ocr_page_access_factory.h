#ifndef VOCEKIT_OCR_PAGE_ACCESS_FACTORY_H
#define VOCEKIT_OCR_PAGE_ACCESS_FACTORY_H

#include "ocr_page_controller.h"

class HubSettingsState;

// 图片识别页的装配输入。工厂负责生成设置快照并补齐安全回调。
struct OcrPageAccessFactoryDependencies
{
    HubSettingsState *settings = nullptr;
    std::function<void(const QString &)> historyRecordSaved;
};

OcrPageAccess createOcrPageAccess(
    const OcrPageAccessFactoryDependencies &dependencies
);

#endif // VOCEKIT_OCR_PAGE_ACCESS_FACTORY_H
