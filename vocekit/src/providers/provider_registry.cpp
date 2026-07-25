#include "provider_registry.h"

#include <QReadLocker>
#include <QWriteLocker>

bool ProviderRegistry::addSpeechProvider(
    const QSharedPointer<ISpeechProvider> &provider)
{
    if (provider.isNull() || provider->id().trimmed().isEmpty()) {
        return false;
    }
    QWriteLocker locker(&m_lock);
    m_speechProviders.insert(provider->id().trimmed(), provider);
    return true;
}

bool ProviderRegistry::addModelProvider(
    const QSharedPointer<IModelProvider> &provider)
{
    if (provider.isNull() || provider->id().trimmed().isEmpty()) {
        return false;
    }
    QWriteLocker locker(&m_lock);
    m_modelProviders.insert(provider->id().trimmed(), provider);
    return true;
}

QSharedPointer<ISpeechProvider> ProviderRegistry::speechProvider(
    const QString &id) const
{
    QReadLocker locker(&m_lock);
    return m_speechProviders.value(id.trimmed());
}

QSharedPointer<IModelProvider> ProviderRegistry::modelProvider(
    const QString &id) const
{
    QReadLocker locker(&m_lock);
    return m_modelProviders.value(id.trimmed());
}

QStringList ProviderRegistry::speechProviderIds() const
{
    QReadLocker locker(&m_lock);
    return m_speechProviders.keys();
}

QStringList ProviderRegistry::modelProviderIds() const
{
    QReadLocker locker(&m_lock);
    return m_modelProviders.keys();
}

ProviderCheckResult ProviderRegistry::checkSpeechProvider(
    const QString &id,
    const CancellationToken &cancellation) const
{
    const QSharedPointer<ISpeechProvider> provider = speechProvider(id);
    return provider.isNull()
        ? missingProvider(id)
        : provider->checkConfiguration(cancellation);
}

ProviderCheckResult ProviderRegistry::checkModelProvider(
    const QString &id,
    const CancellationToken &cancellation) const
{
    const QSharedPointer<IModelProvider> provider = modelProvider(id);
    return provider.isNull()
        ? missingProvider(id)
        : provider->checkConfiguration(cancellation);
}

void ProviderRegistry::refreshConfiguration()
{
    QList<QSharedPointer<ISpeechProvider>> speech;
    QList<QSharedPointer<IModelProvider>> models;
    {
        QReadLocker locker(&m_lock);
        speech = m_speechProviders.values();
        models = m_modelProviders.values();
    }
    for (const QSharedPointer<ISpeechProvider> &provider : speech) {
        provider->refreshConfiguration();
    }
    for (const QSharedPointer<IModelProvider> &provider : models) {
        provider->refreshConfiguration();
    }
}

ProviderCheckResult ProviderRegistry::missingProvider(const QString &id)
{
    ProviderCheckResult result;
    result.available = false;
    result.error.code = QStringLiteral("provider.not_found");
    result.error.message = QStringLiteral("未找到接口提供商：")
        + id.trimmed();
    return result;
}
