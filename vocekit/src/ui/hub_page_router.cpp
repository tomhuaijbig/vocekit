#include "hub_page_router.h"

HubPageRouter::HubPageRouter(QWidget *parent)
    : QStackedWidget(parent)
{
}

bool HubPageRouter::registerPage(const HubPageRegistration &registration)
{
    const QString id = registration.id.trimmed();
    if (id.isEmpty() || !registration.page || m_pageIndexes.contains(id)) {
        return false;
    }

    const int index = addWidget(registration.page);
    m_pageIndexes.insert(id, index);
    m_activationCallbacks.insert(id, registration.activated);
    return true;
}

bool HubPageRouter::registerDeferredPage(
    const QString &id,
    const std::function<QWidget *()> &factory,
    const std::function<void(bool pageChanged)> &activated)
{
    const QString normalizedId = id.trimmed();
    if (normalizedId.isEmpty()
        || !factory
        || m_pageIndexes.contains(normalizedId)) {
        return false;
    }

    const int index = addWidget(new QWidget);
    m_pageIndexes.insert(normalizedId, index);
    m_pageFactories.insert(normalizedId, factory);
    m_activationCallbacks.insert(normalizedId, activated);
    return true;
}

bool HubPageRouter::selectPage(const QString &id)
{
    const auto index = m_pageIndexes.constFind(id);
    if (index == m_pageIndexes.constEnd()) {
        return false;
    }

    const auto factory = m_pageFactories.constFind(id);
    if (factory != m_pageFactories.constEnd()) {
        QWidget *page = factory.value()();
        if (!page) {
            return false;
        }
        QWidget *placeholder = widget(index.value());
        removeWidget(placeholder);
        delete placeholder;
        insertWidget(index.value(), page);
        m_pageFactories.remove(id);
    }

    const bool pageChanged = m_currentPageId != id;
    setCurrentIndex(index.value());
    m_currentPageId = id;

    const auto callback = m_activationCallbacks.constFind(id);
    if (callback != m_activationCallbacks.constEnd() && callback.value()) {
        callback.value()(pageChanged);
    }
    return true;
}

QString HubPageRouter::currentPageId() const
{
    return m_currentPageId;
}
