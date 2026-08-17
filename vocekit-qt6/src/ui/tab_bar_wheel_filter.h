#ifndef VOCEKIT_TAB_BAR_WHEEL_FILTER_H
#define VOCEKIT_TAB_BAR_WHEEL_FILTER_H

#include <QObject>

// 标签页滚轮过滤器：让设置、历史等横向标签栏可以用鼠标滚轮切换当前标签。
class TabBarWheelFilter : public QObject
{
public:
    explicit TabBarWheelFilter(QObject *parent = nullptr);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
};

#endif // VOCEKIT_TAB_BAR_WHEEL_FILTER_H
