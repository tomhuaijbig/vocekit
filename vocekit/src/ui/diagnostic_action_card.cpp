#include "diagnostic_action_card.h"

#include "ui_style.h"

#include <QtWidgets>

QWidget *diagnosticActionCard(
    const QString &title,
    const QString &hint,
    const QString &buttonText,
    QPushButton **buttonOut,
    QLabel **resultOut,
    const std::function<void()> &onClick
)
{
    auto *frame = new QFrame;
    frame->setObjectName(QStringLiteral("card"));
    frame->setProperty(
        "testSearchText",
        (QStringList() << title << hint << buttonText).join(QStringLiteral("\n"))
    );
    frame->setStyleSheet(cardStyle());

    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);

    auto *top = new QHBoxLayout;
    auto *labels = new QVBoxLayout;
    auto *name = new QLabel(title);
    name->setFont(appFont(13, QFont::DemiBold));
    labels->addWidget(name);

    auto *button = new QPushButton(buttonText);
    button->setFixedHeight(36);
    button->setStyleSheet(buttonStyle(QStringLiteral("#111827")));
    QObject::connect(button, &QPushButton::clicked, button, [onClick]() {
        if (onClick) {
            onClick();
        }
    });
    if (buttonOut) {
        *buttonOut = button;
    }

    top->addLayout(labels, 1);
    top->addWidget(button);
    layout->addLayout(top);

    if (resultOut) {
        auto *result = new QLabel;
        result->setWordWrap(true);
        result->setTextInteractionFlags(Qt::TextSelectableByMouse);
        result->setStyleSheet(QStringLiteral(
            "QLabel {"
            "  background: #f2f4f7;"
            "  color: #344054;"
            "  border-radius: 6px;"
            "  padding: 10px;"
            "}"
        ));
        result->setVisible(false);
        *resultOut = result;
        layout->addWidget(result);
    }

    return frame;
}

void showDiagnosticResult(QLabel *result, const QString &text)
{
    if (!result) {
        return;
    }
    result->setText(text);
    result->setVisible(true);
}
