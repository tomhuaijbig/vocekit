#ifndef VOCEKIT_APPLICATION_EVENTS_H
#define VOCEKIT_APPLICATION_EVENTS_H

#include <QObject>
#include <QStringList>

// 设置变化只描述受影响的键和功能，页面自行决定是否局部刷新。
struct SettingsChangeSet
{
    QStringList keys;
    QStringList functionIds;
};

// 历史变化可以是指定记录变化，也可以要求页面重新加载索引。
struct HistoryChangeSet
{
    QStringList recordIds;
    bool resetRequired = false;
};

// 词库变化可以是指定词条变化，也可以要求页面重新加载全部词条。
struct VocabularyChangeSet
{
    QStringList entryIds;
    bool resetRequired = false;
};

Q_DECLARE_METATYPE(SettingsChangeSet)
Q_DECLARE_METATYPE(HistoryChangeSet)
Q_DECLARE_METATYPE(VocabularyChangeSet)

// 应用事件中心：负责在业务服务和页面之间传递明确的状态变化。
class ApplicationEvents : public QObject
{
    Q_OBJECT

public:
    explicit ApplicationEvents(QObject *parent = nullptr);

    void publishSettingsChanged(const SettingsChangeSet &change);
    void publishHistoryChanged(const HistoryChangeSet &change);
    void publishVocabularyChanged(const VocabularyChangeSet &change);

signals:
    void settingsChanged(SettingsChangeSet change);
    void historyChanged(HistoryChangeSet change);
    void vocabularyChanged(VocabularyChangeSet change);
};

#endif // VOCEKIT_APPLICATION_EVENTS_H
