#ifndef TOGGLE_SWITCH_H
#define TOGGLE_SWITCH_H

#include <QHash>
#include <QProxyStyle>
#include <QVariantAnimation>

class QCheckBox;
class QColor;
class QPainter;
class QString;
class QWidget;

// 全局复选框开关样式：把 QCheckBox 绘制成滑动开关，同时保留原来的 checked 逻辑。
class ToggleSwitchStyle : public QProxyStyle
{
public:
    explicit ToggleSwitchStyle(QStyle *baseStyle = nullptr);

    int pixelMetric(PixelMetric metric, const QStyleOption *option = nullptr, const QWidget *widget = nullptr) const override;
    void polish(QWidget *widget) override;
    void unpolish(QWidget *widget) override;
    void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget = nullptr) const override;

private:
    static QColor mixColor(const QColor &from, const QColor &to, qreal progress);
    void animateSwitch(QWidget *widget, qreal target);

    QHash<QObject *, QVariantAnimation *> m_animations;
};

// 创建“文字在左，滑动开关在右”的组合控件。点击文字也会切换开关。
QWidget *labeledSwitch(QCheckBox *box, const QString &text);

#endif
