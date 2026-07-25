#ifndef VOCEKIT_HUB_PAGE_ROUTER_H
#define VOCEKIT_HUB_PAGE_ROUTER_H

#include <QMap>
#include <QStackedWidget>
#include <QString>

#include <functional>

struct HubPageRegistration
{
    QString id;
    QWidget *page = nullptr;
    std::function<void(bool pageChanged)> activated;
};

// 页面 ID、控件索引和激活动作统一保存在这里，调用方不依赖固定数字索引。
class HubPageRouter : public QStackedWidget
{
public:
    explicit HubPageRouter(QWidget *parent = nullptr);

    bool registerPage(const HubPageRegistration &registration);
    bool selectPage(const QString &id);
    QString currentPageId() const;

private:
    QMap<QString, int> m_pageIndexes;
    QMap<QString, std::function<void(bool)>> m_activationCallbacks;
    QString m_currentPageId;
};

#endif // VOCEKIT_HUB_PAGE_ROUTER_H
