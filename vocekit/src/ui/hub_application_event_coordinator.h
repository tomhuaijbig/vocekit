#ifndef VOCEKIT_HUB_APPLICATION_EVENT_COORDINATOR_H
#define VOCEKIT_HUB_APPLICATION_EVENT_COORDINATOR_H

#include "../app/application_events.h"

#include <QPointer>

#include <functional>

// 主窗口对应用事件的响应动作。协调器负责连接信号和构造发布数据，
// 主窗口只保留具体页面刷新实现。
struct HubApplicationEventCoordinatorCallbacks
{
    std::function<void(const SettingsChangeSet &)> settingsChanged;
    std::function<void(const HistoryChangeSet &)> historyChanged;
    std::function<void(const VocabularyChangeSet &)> vocabularyChanged;
};

class HubApplicationEventCoordinator : public QObject
{
public:
    explicit HubApplicationEventCoordinator(
        ApplicationEvents *events,
        const HubApplicationEventCoordinatorCallbacks &callbacks,
        QObject *parent = nullptr
    );

    void dispatchSettingsChanged(
        const QStringList &keys,
        const QStringList &functionIds
    );
    void dispatchHistoryChanged(
        const QStringList &recordIds,
        bool resetRequired
    );
    void dispatchVocabularyChanged(
        const QStringList &entryIds,
        bool resetRequired
    );

private:
    QPointer<ApplicationEvents> m_events;
    HubApplicationEventCoordinatorCallbacks m_callbacks;
};

#endif // VOCEKIT_HUB_APPLICATION_EVENT_COORDINATOR_H
