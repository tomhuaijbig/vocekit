#include <QtTest>

#define private public
#include "../../src/ui/result_choice_popup.h"
#undef private

#include <QComboBox>
#include <QDialog>
#include <QPushButton>
#include <QTimer>

#include <type_traits>

namespace {

QPushButton *buttonWithText(ResultChoicePopup *popup, const QString &text)
{
    const QList<QPushButton *> buttons = popup->findChildren<QPushButton *>();
    for (QPushButton *button : buttons) {
        if (button->text() == text) {
            return button;
        }
    }
    return nullptr;
}

} // namespace

class ResultChoicePopupHeaderTests : public QObject
{
    Q_OBJECT

private slots:
    void exposesWidgetType();
    void acceptsWindowPreferenceSnapshot();
    void appliesConfiguredActionVisibility();
    void clampsConfiguredOpacity();
    void reportsTheResolvedActionOnce();
    void closingBusyPopupRequestsCancellation();
    void retryDialogSelectsTheMigratedCurrentModel();
};

void ResultChoicePopupHeaderTests::exposesWidgetType()
{
    QVERIFY((std::is_base_of<QWidget, ResultChoicePopup>::value));
}

void ResultChoicePopupHeaderTests::acceptsWindowPreferenceSnapshot()
{
    QVERIFY((std::is_constructible<
        ResultChoicePopup,
        const ResultPopupWindowPreferences &,
        const QString &,
        const QString &,
        ClipboardWindowHandle,
        bool,
        int,
        QWidget *
    >::value));
}

void ResultChoicePopupHeaderTests::appliesConfiguredActionVisibility()
{
    ResultPopupWindowPreferences preferences;
    ResultChoicePopup popup(
        preferences,
        QStringLiteral("test"),
        QStringLiteral("result"),
        nullptr,
        true,
        0
    );
    popup.setAttribute(Qt::WA_DeleteOnClose, false);

    popup.setActionOrder(
        QStringList()
            << QStringLiteral("write")
            << QStringLiteral("copy")
            << QStringLiteral("write")
            << QStringLiteral("unsupported")
    );

    QPushButton *write = buttonWithText(&popup, QString::fromUtf8("写入"));
    QPushButton *copy = buttonWithText(&popup, QString::fromUtf8("复制"));
    QPushButton *replace = buttonWithText(&popup, QString::fromUtf8("替换选中"));
    QPushButton *close = buttonWithText(&popup, QString::fromUtf8("关闭"));
    QVERIFY(write);
    QVERIFY(copy);
    QVERIFY(replace);
    QVERIFY(close);
    QVERIFY(!write->isHidden());
    QVERIFY(!copy->isHidden());
    QVERIFY(replace->isHidden());
    QVERIFY(!close->isHidden());
}

void ResultChoicePopupHeaderTests::clampsConfiguredOpacity()
{
    ResultPopupWindowPreferences preferences;
    ResultChoicePopup popup(
        preferences,
        QStringLiteral("test"),
        QStringLiteral("result"),
        nullptr,
        false,
        0
    );
    popup.setAttribute(Qt::WA_DeleteOnClose, false);

    popup.setOpacityPercent(20);
    QVERIFY(qAbs(popup.windowOpacity() - 0.60) < 0.02);
    popup.setOpacityPercent(95);
    QVERIFY(qAbs(popup.windowOpacity() - 0.95) < 0.02);
    popup.setOpacityPercent(120);
    QVERIFY(qAbs(popup.windowOpacity() - 1.00) < 0.02);
}

void ResultChoicePopupHeaderTests::reportsTheResolvedActionOnce()
{
    ResultPopupWindowPreferences preferences;
    ResultChoicePopup popup(
        preferences,
        QStringLiteral("test"),
        QStringLiteral("result"),
        nullptr,
        false,
        0
    );
    popup.setAttribute(Qt::WA_DeleteOnClose, false);

    QStringList resolvedActions;
    popup.setResolvedCallback([&resolvedActions](const QString &action) {
        resolvedActions.append(action);
    });

    QPushButton *close = buttonWithText(&popup, QString::fromUtf8("关闭"));
    QVERIFY(close);
    close->click();
    popup.close();

    QCOMPARE(resolvedActions, QStringList() << QStringLiteral("close"));
}

void ResultChoicePopupHeaderTests::closingBusyPopupRequestsCancellation()
{
    ResultPopupWindowPreferences preferences;
    ResultChoicePopup popup(
        preferences,
        QStringLiteral("test"),
        QStringLiteral("result"),
        nullptr,
        false,
        0
    );
    popup.setAttribute(Qt::WA_DeleteOnClose, false);

    int cancellationCount = 0;
    popup.setCancellationCallback([&cancellationCount]() {
        ++cancellationCount;
    });
    popup.setBusy(true);

    QVERIFY(popup.close());
    QCOMPARE(cancellationCount, 1);
}

void ResultChoicePopupHeaderTests::
retryDialogSelectsTheMigratedCurrentModel()
{
    ResultPopupWindowPreferences preferences;
    ResultChoicePopup popup(
        preferences,
        QStringLiteral("test"),
        QStringLiteral("result"),
        nullptr,
        false,
        0
    );
    popup.setAttribute(Qt::WA_DeleteOnClose, false);
    popup.setActionCallbacks(
        std::function<void()>(),
        [](const QString &) {},
        std::function<void(const QString &)>()
    );
    popup.setCurrentModel(QStringLiteral("openai:gpt-5.4"));

    QString selectedModel;
    int oldModelIndex = -2;
    bool dialogFound = false;
    QTimer::singleShot(0, [&]() {
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            QDialog *dialog = qobject_cast<QDialog *>(widget);
            if (!dialog || !dialog->isVisible()) {
                continue;
            }
            dialogFound = true;
            QComboBox *models = dialog->findChild<QComboBox *>();
            if (models) {
                selectedModel = models->currentData().toString();
                oldModelIndex = models->findData(
                    QStringLiteral("openai:gpt-5.4")
                );
            }
            dialog->reject();
            break;
        }
    });

    popup.chooseModelAndRetry();
    QVERIFY(dialogFound);
    QCOMPARE(selectedModel, QStringLiteral("openai:gpt-5.6-terra"));
    QVERIFY(selectedModel != QStringLiteral("deepseek-v4-flash"));
    QCOMPARE(oldModelIndex, -1);
}

QTEST_MAIN(ResultChoicePopupHeaderTests)

#include "result_choice_popup_header_tests.moc"
