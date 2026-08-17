#pragma once

#include "secret_config.h"

#include <QString>

// SecretStore 只负责 config/secrets.json 的读取和保存，不参与界面展示和网络请求。
class SecretStore
{
public:
    explicit SecretStore(const QString &path = QString());

    QString path() const { return m_path; }
    SecretConfig load() const;
    bool save(const SecretConfig &secrets) const;

private:
    QString m_path;
};
