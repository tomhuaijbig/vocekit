#include "floating_bar_style_selector.h"

#include "../config/app_settings_defaults.h"

#include <QAbstractButton>
#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QPainter>
#include <QStyleOptionButton>

namespace {

class FloatingBarStyleCard : public QAbstractButton
{
public:
    FloatingBarStyleCard(
        const QString &style,
        const QString &accessibleText,
        QWidget *parent)
        : QAbstractButton(parent), m_style(style)
    {
        setCheckable(true);
        setFocusPolicy(Qt::StrongFocus);
        setAccessibleName(accessibleText);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
        updateMinimumHeight();
    }

protected:
    void changeEvent(QEvent *event) override
    {
        QAbstractButton::changeEvent(event);
        if (event->type() == QEvent::FontChange) {
            updateMinimumHeight();
        }
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const bool selected = property("selected").toBool();
        const QRectF outer = rect().adjusted(1, 1, -1, -1);
        painter.setPen(QPen(
            selected ? QColor("#2f6fed") : QColor("#cfd7e6"),
            selected ? 2 : 1
        ));
        painter.setBrush(QColor("#ffffff"));
        painter.drawRoundedRect(outer, 8, 8);

        painter.setPen(QColor("#101827"));
        QFont titleFont = font();
        titleFont.setBold(true);
        painter.setFont(titleFont);
        painter.drawText(
            QRect(12, 8, width() - 24, fontMetrics().height()),
            Qt::AlignLeft | Qt::AlignVCenter,
            floatingBarStyleTitle(m_style, true)
        );

        const int previewTop = 12 + fontMetrics().height();
        if (m_style == floatingBarStyleInherit()) {
            painter.setPen(QColor("#63708a"));
            painter.setFont(font());
            painter.drawText(
                QRect(12, previewTop, width() - 24,
                      height() - previewTop - 8),
                Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap,
                QString::fromUtf8("使用全局漂浮窗样式")
            );
            return;
        }

        QRectF preview(12, previewTop + 4, width() - 24,
                       height() - previewTop - 12);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#111111"));
        painter.drawRoundedRect(preview, 14, 14);
        painter.setFont(font());
        if (m_style == floatingBarStyleStatusPill()) {
            painter.setPen(QColor("#ffffff"));
            painter.drawText(
                preview.adjusted(10, 0, -10, 0),
                Qt::AlignVCenter,
                QString::fromUtf8("×  ▂▅▃▆  正在录音  ✓")
            );
            return;
        }
        painter.setPen(QColor("#ffffff"));
        painter.drawText(
            preview.adjusted(10, 4, -10, -4),
            Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
            QString::fromUtf8("正在转录\n已确认文字  临时文字")
        );
    }

private:
    void updateMinimumHeight()
    {
        setMinimumHeight(QFontMetrics(font()).height() * 3 + 28);
    }

    QString m_style;
};

} // namespace

FloatingBarStyleSelector::FloatingBarStyleSelector(
    const Options &options,
    QWidget *parent)
    : QWidget(parent),
      m_options(options),
      m_currentStyle(options.allowInherit
          ? floatingBarStyleInherit()
          : floatingBarStyleStatusPill())
{
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(10);
    if (m_options.allowInherit) {
        addCard(floatingBarStyleInherit(), QString::fromUtf8("跟随全局"));
    }
    addCard(floatingBarStyleStatusPill(), QString::fromUtf8("状态胶囊"));
    addCard(
        floatingBarStyleLiveTranscriptCard(),
        QString::fromUtf8("实时文字卡片")
    );
    updateCards();
}

void FloatingBarStyleSelector::setCurrentStyle(const QString &style)
{
    m_currentStyle = normalizedStyle(style);
    updateCards();
}

QString FloatingBarStyleSelector::currentStyle() const
{
    return m_currentStyle;
}

void FloatingBarStyleSelector::setStyleChangedCallback(
    const std::function<void(const QString &)> &callback)
{
    m_styleChanged = callback;
}

void FloatingBarStyleSelector::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::FontChange) {
        for (QAbstractButton *card : m_cards) {
            card->setFont(font());
        }
    }
}

void FloatingBarStyleSelector::addCard(
    const QString &style,
    const QString &accessibleText)
{
    QAbstractButton *card = new FloatingBarStyleCard(
        style,
        accessibleText,
        this
    );
    card->setObjectName(
        QStringLiteral("floatingBarStyleCard_") + style
    );
    card->setProperty("styleId", style);
    connect(card, &QAbstractButton::clicked, this, [this, style]() {
        if (m_currentStyle == style) {
            return;
        }
        m_currentStyle = style;
        updateCards();
        if (m_styleChanged) {
            m_styleChanged(m_currentStyle);
        }
    });
    m_cards.append(card);
    m_layout->addWidget(card);
}

void FloatingBarStyleSelector::updateCards()
{
    for (QAbstractButton *card : m_cards) {
        const bool selected =
            card->property("styleId").toString() == m_currentStyle;
        card->setChecked(selected);
        card->setProperty("selected", selected);
        card->update();
    }
}

QString FloatingBarStyleSelector::normalizedStyle(
    const QString &style) const
{
    return m_options.allowInherit
        ? normalizeFunctionFloatingBarStyle(style)
        : normalizeGlobalFloatingBarStyle(style);
}
