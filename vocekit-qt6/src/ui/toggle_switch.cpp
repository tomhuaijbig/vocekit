#include "toggle_switch.h"

#include <QtWidgets>

namespace {

class SwitchTextLabel : public QLabel
{
public:
    SwitchTextLabel(const QString &text, QCheckBox *target)
        : QLabel(text), m_target(target)
    {
        QFont font;
        font.setFamily(QStringLiteral("Microsoft YaHei UI"));
        font.setPointSize(10);
        font.setWeight(QFont::DemiBold);
        setFont(font);
        setCursor(Qt::PointingHandCursor);
    }

protected:
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && rect().contains(event->pos()) && m_target) {
            m_target->toggle();
            event->accept();
            return;
        }
        QLabel::mouseReleaseEvent(event);
    }

private:
    QPointer<QCheckBox> m_target;
};

} // namespace

QWidget *labeledSwitch(QCheckBox *box, const QString &text)
{
    auto *wrap = new QWidget;
    auto *layout = new QHBoxLayout(wrap);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto *label = new SwitchTextLabel(text, box);
    box->setText(QString());
    box->setFixedWidth(52);
    box->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    layout->addWidget(label);
    layout->addWidget(box);
    return wrap;
}

ToggleSwitchStyle::ToggleSwitchStyle(QStyle *baseStyle)
    : QProxyStyle(baseStyle)
{
}

int ToggleSwitchStyle::pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const
{
    if (qobject_cast<const QCheckBox *>(widget)) {
        if (metric == PM_IndicatorWidth) {
            return 46;
        }
        if (metric == PM_IndicatorHeight) {
            return 24;
        }
        if (metric == PM_CheckBoxLabelSpacing) {
            return 10;
        }
    }
    return QProxyStyle::pixelMetric(metric, option, widget);
}

void ToggleSwitchStyle::polish(QWidget *widget)
{
    QProxyStyle::polish(widget);
    auto *box = qobject_cast<QCheckBox *>(widget);
    if (!box) {
        return;
    }

    box->setCursor(Qt::PointingHandCursor);
    box->installEventFilter(this);
    if (!box->property("switchProgress").isValid()) {
        box->setProperty("switchProgress", box->isChecked() ? 1.0 : 0.0);
    }
    if (!box->property("switchStyleConnected").toBool()) {
        box->setProperty("switchStyleConnected", true);
        QObject::connect(box, &QCheckBox::toggled, this, [this, box](bool checked) {
            animateSwitch(box, checked ? 1.0 : 0.0);
        });
        QObject::connect(box, &QObject::destroyed, this, [this](QObject *object) {
            if (m_animations.contains(object)) {
                m_animations.take(object)->deleteLater();
            }
        });
    }
}

void ToggleSwitchStyle::unpolish(QWidget *widget)
{
    auto *box = qobject_cast<QCheckBox *>(widget);
    if (box) {
        box->removeEventFilter(this);
        QObject::disconnect(box, nullptr, this, nullptr);
        if (m_animations.contains(box)) {
            m_animations.take(box)->deleteLater();
        }
        box->setProperty("switchStyleConnected", false);
    }
    QProxyStyle::unpolish(widget);
}

void ToggleSwitchStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    if (element == PE_IndicatorCheckBox && qobject_cast<const QCheckBox *>(widget)) {
        const bool enabled = option->state & State_Enabled;
        const bool checked = option->state & State_On;
        const bool partial = option->state & State_NoChange;
        const bool hover = option->state & State_MouseOver;
        qreal progress = widget->property("switchProgress").isValid()
            ? widget->property("switchProgress").toDouble()
            : (checked ? 1.0 : 0.0);
        progress = qBound(0.0, progress, 1.0);
        if (partial) {
            progress = 0.5;
        }

        QRectF track = option->rect;
        track = track.adjusted(1.0, 2.0, -1.0, -2.0);
        const qreal radius = track.height() / 2.0;

        QColor trackColor;
        if (!enabled) {
            trackColor = mixColor(QColor(QStringLiteral("#e5e7eb")), QColor(QStringLiteral("#bcd2ff")), progress);
        } else if (partial) {
            trackColor = QColor(QStringLiteral("#7c3aed"));
        } else {
            const QColor offColor = hover ? QColor(QStringLiteral("#cbd5e1")) : QColor(QStringLiteral("#d0d5dd"));
            const QColor onColor = hover ? QColor(QStringLiteral("#1d4ed8")) : QColor(QStringLiteral("#2563eb"));
            trackColor = mixColor(offColor, onColor, progress);
        }

        const qreal knobSize = track.height() - 6.0;
        const qreal knobStart = track.left() + 3.0;
        const qreal knobEnd = track.right() - knobSize - 3.0;
        const qreal knobX = knobStart + (knobEnd - knobStart) * progress;
        QRectF knob(knobX, track.top() + 3.0, knobSize, knobSize);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(Qt::NoPen);
        painter->setBrush(trackColor);
        painter->drawRoundedRect(track, radius, radius);

        painter->setBrush(enabled ? QColor(QStringLiteral("#ffffff")) : QColor(QStringLiteral("#f8fafc")));
        painter->drawEllipse(knob);
        painter->restore();
        return;
    }

    QProxyStyle::drawPrimitive(element, option, painter, widget);
}

QColor ToggleSwitchStyle::mixColor(const QColor &from, const QColor &to, qreal progress)
{
    progress = qBound(0.0, progress, 1.0);
    return QColor(
        from.red() + qRound((to.red() - from.red()) * progress),
        from.green() + qRound((to.green() - from.green()) * progress),
        from.blue() + qRound((to.blue() - from.blue()) * progress),
        from.alpha() + qRound((to.alpha() - from.alpha()) * progress)
    );
}

void ToggleSwitchStyle::animateSwitch(QWidget *widget, qreal target)
{
    if (!widget) {
        return;
    }
    const qreal start = widget->property("switchProgress").isValid()
        ? widget->property("switchProgress").toDouble()
        : target;
    if (qAbs(start - target) < 0.01) {
        widget->setProperty("switchProgress", target);
        widget->update();
        return;
    }

    if (m_animations.contains(widget)) {
        m_animations.take(widget)->deleteLater();
    }
    auto *animation = new QVariantAnimation(this);
    m_animations.insert(widget, animation);
    QPointer<QWidget> targetWidget(widget);
    animation->setStartValue(start);
    animation->setEndValue(target);
    animation->setDuration(150);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    QObject::connect(animation, &QVariantAnimation::valueChanged, this, [targetWidget](const QVariant &value) {
        if (!targetWidget) {
            return;
        }
        targetWidget->setProperty("switchProgress", value.toDouble());
        targetWidget->update();
    });
    QObject::connect(animation, &QVariantAnimation::finished, this, [this, widget, target]() {
        if (m_animations.value(widget) == sender()) {
            m_animations.remove(widget);
        }
        if (widget) {
            widget->setProperty("switchProgress", target);
            widget->update();
        }
        sender()->deleteLater();
    });
    animation->start();
}
