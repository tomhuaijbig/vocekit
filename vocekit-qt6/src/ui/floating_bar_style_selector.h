#ifndef VOCEKIT_FLOATING_BAR_STYLE_SELECTOR_H
#define VOCEKIT_FLOATING_BAR_STYLE_SELECTOR_H

#include <QWidget>

#include <functional>

class QAbstractButton;
class QHBoxLayout;

class FloatingBarStyleSelector : public QWidget
{
    Q_OBJECT

public:
    struct Options
    {
        bool allowInherit = false;
    };

    explicit FloatingBarStyleSelector(
        const Options &options,
        QWidget *parent = nullptr
    );

    void setCurrentStyle(const QString &style);
    QString currentStyle() const;
    void setStyleChangedCallback(
        const std::function<void(const QString &)> &callback
    );

protected:
    void changeEvent(QEvent *event) override;

private:
    void addCard(const QString &style, const QString &accessibleText);
    void updateCards();
    QString normalizedStyle(const QString &style) const;

    Options m_options;
    QString m_currentStyle;
    QHBoxLayout *m_layout = nullptr;
    QList<QAbstractButton *> m_cards;
    std::function<void(const QString &)> m_styleChanged;
};

#endif // VOCEKIT_FLOATING_BAR_STYLE_SELECTOR_H
