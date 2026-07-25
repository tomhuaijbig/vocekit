#include "ocr_page_access_factory.h"

#include "hub_settings_state.h"

OcrPageAccess createOcrPageAccess(
    const OcrPageAccessFactoryDependencies &dependencies
)
{
    OcrPageAccess access;
    HubSettingsState *settings = dependencies.settings;
    access.settingsSnapshotProvider = [settings]() {
        return settings ? settings->toData() : AppSettingsData();
    };
    access.historyRecordSaved = dependencies.historyRecordSaved
        ? dependencies.historyRecordSaved
        : [](const QString &) {};
    return access;
}
