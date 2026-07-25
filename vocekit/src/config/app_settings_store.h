#ifndef VOCEKIT_APP_SETTINGS_STORE_H
#define VOCEKIT_APP_SETTINGS_STORE_H

#include "app_settings_data.h"

#include "../domain/operation_error.h"

#include <QString>

// AppSettingsStore 是新的设置读写服务。
// 它只负责把 AppSettingsData 与 config/settings.json 互相转换，
// 不直接操作界面，也不弹出提示框。
class AppSettingsStore
{
public:
    explicit AppSettingsStore(const QString &path = QString());

    bool load(OperationError *error = nullptr);
    bool loadOrCreateDefaults(OperationError *error = nullptr);
    bool save(OperationError *error = nullptr) const;

    const AppSettingsData &snapshot() const;
    FunctionSettings function(const QString &id) const;
    bool updateFunction(const FunctionSettings &updatedFunction);
    void updateGlobal(const AppSettingsData &data);
    void replaceSnapshot(const AppSettingsData &data);
    bool replaceAndSave(
        const AppSettingsData &data,
        OperationError *error = nullptr
    );

    QString path() const;

private:
    QString m_path;
    AppSettingsData m_data;
};

#endif // VOCEKIT_APP_SETTINGS_STORE_H
