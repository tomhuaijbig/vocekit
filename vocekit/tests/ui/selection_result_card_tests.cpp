#include <QtTest>

#include "../../src/ui/selection_result_card.h"

#include <QApplication>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollBar>
#include <QToolButton>

namespace {

SelectionResultCardState completedState(const QString &text)
{
    SelectionResultCardState state;
    state.actionId = QStringLiteral("explain");
    state.title = QString::fromUtf8("解释");
    state.committedText = text;
    state.statusText = QString::fromUtf8("已完成");
    return state;
}

QPushButton *pushButton(SelectionResultCard &card, const char *name)
{
    return card.findChild<QPushButton *>(QString::fromLatin1(name));
}

} // namespace

class SelectionResultCardTests : public QObject
{
    Q_OBJECT

private slots:
    void rendersCommittedAndProvisionalTextInBoundedScrollArea()
    {
        SelectionResultCard card;
        SelectionResultCardState state;
        state.committedText = QStringLiteral("committed");
        state.provisionalText = QStringLiteral("provisional");
        card.setState(state);
        QPlainTextEdit *committed = card.findChild<QPlainTextEdit *>(
            QStringLiteral("selectionResultCommittedText")
        );
        QPlainTextEdit *provisional = card.findChild<QPlainTextEdit *>(
            QStringLiteral("selectionResultProvisionalText")
        );
        QVERIFY(committed);
        QVERIFY(provisional);
        QTRY_COMPARE(committed->toPlainText(), QStringLiteral("committed"));
        QCOMPARE(provisional->toPlainText(), QStringLiteral("provisional"));
        QVERIFY(committed->verticalScrollBar());
    }

    void runningStateShowsCancelAndHidesReplace()
    {
        SelectionResultCard card;
        SelectionResultCardState state;
        state.running = true;
        state.replaceEnabled = true;
        card.setState(state);
        QVERIFY(pushButton(card, "selectionResultCancelButton")->isVisibleTo(&card));
        QVERIFY(!pushButton(card, "selectionResultRegenerateButton")->isVisibleTo(&card));
        QVERIFY(!pushButton(card, "selectionResultReplaceButton")->isEnabled());
    }

    void completedStateEnablesCopyAndConditionallyEnablesReplace()
    {
        SelectionResultCard card;
        SelectionResultCardState state = completedState(QStringLiteral("done"));
        state.replaceEnabled = false;
        card.setState(state);
        QVERIFY(!pushButton(card, "selectionResultCancelButton")->isVisibleTo(&card));
        QVERIFY(pushButton(card, "selectionResultRegenerateButton")->isVisibleTo(&card));
        QVERIFY(pushButton(card, "selectionResultCopyButton")->isEnabled());
        QVERIFY(!pushButton(card, "selectionResultReplaceButton")->isEnabled());
        state.replaceEnabled = true;
        card.setState(state);
        QVERIFY(pushButton(card, "selectionResultReplaceButton")->isEnabled());
    }

    void completedStateRegeneratesOnlyAfterExplicitClick()
    {
        SelectionResultCard card;
        int calls = 0;
        SelectionResultCardCallbacks callbacks;
        callbacks.regenerateRequested = [&calls]() { ++calls; };
        card.setCallbacks(callbacks);
        card.setState(completedState(QStringLiteral("done")));
        QCOMPARE(calls, 0);
        pushButton(card, "selectionResultRegenerateButton")->click();
        QCOMPARE(calls, 1);
    }

    void pinKeepsCardVisibleAcrossSelectionChanges()
    {
        SelectionResultCard card;
        card.setState(completedState(QStringLiteral("done")));
        card.showAt(QPoint(40, 40), QRect(0, 0, 900, 700));
        QToolButton *pin = card.findChild<QToolButton *>(
            QStringLiteral("selectionResultPinButton")
        );
        QVERIFY(pin);
        pin->click();
        QVERIFY(card.state().pinned);
        card.closeIfUnpinned();
        QVERIFY(card.isVisible());
        pin->click();
        card.closeIfUnpinned();
        QVERIFY(!card.isVisible());
    }

    void followUpActivatesOnlyAfterExplicitClick()
    {
        SelectionResultCard card;
        QString question;
        SelectionResultCardCallbacks callbacks;
        callbacks.followUpRequested = [&question](const QString &value) {
            question = value;
        };
        card.setCallbacks(callbacks);
        card.setState(completedState(QStringLiteral("done")));
        card.showAt(QPoint(40, 40), QRect(0, 0, 900, 700));
        QVERIFY(card.windowFlags() & Qt::WindowDoesNotAcceptFocus);
        QLineEdit *input = card.findChild<QLineEdit *>(
            QStringLiteral("selectionResultFollowUpInput")
        );
        QVERIFY(input);
        QTest::mouseClick(input, Qt::LeftButton);
        QTRY_VERIFY(!(card.windowFlags() & Qt::WindowDoesNotAcceptFocus));
        input->setText(QString::fromUtf8("继续解释"));
        pushButton(card, "selectionResultFollowUpButton")->click();
        QCOMPARE(question, QString::fromUtf8("继续解释"));
    }

    void outsideCloseDoesNotClosePinnedCard()
    {
        SelectionResultCard card;
        SelectionResultCardState state = completedState(QStringLiteral("done"));
        state.pinned = true;
        card.setState(state);
        card.showAt(QPoint(20, 20), QRect(0, 0, 800, 600));
        card.closeIfUnpinned();
        QVERIFY(card.isVisible());
    }

    void nextShowAfterFollowUpRestoresPassiveWindowMode()
    {
        SelectionResultCard card;
        card.setState(completedState(QStringLiteral("done")));
        const QRect screen(0, 0, 900, 700);
        card.showAt(QPoint(40, 40), screen);
        QLineEdit *input = card.findChild<QLineEdit *>(
            QStringLiteral("selectionResultFollowUpInput")
        );
        QTest::mouseClick(input, Qt::LeftButton);
        QTRY_VERIFY(!(card.windowFlags() & Qt::WindowDoesNotAcceptFocus));
        card.closeIfUnpinned();
        card.showAt(QPoint(60, 60), screen);
        QVERIFY(card.windowFlags() & Qt::WindowDoesNotAcceptFocus);
        QVERIFY(card.testAttribute(Qt::WA_ShowWithoutActivating));
    }

    void callbacksInstalledAfterRunningStateRefreshCancelAvailability()
    {
        SelectionResultCard card;
        SelectionResultCardState running;
        running.running = true;
        card.setState(running);
        QVERIFY(!pushButton(card, "selectionResultCancelButton")->isEnabled());
        SelectionResultCardCallbacks callbacks;
        callbacks.cancelRequested = []() {};
        card.setCallbacks(callbacks);
        QVERIFY(pushButton(card, "selectionResultCancelButton")->isEnabled());
    }

    void terminalAndActionCallbacksAreExactlyOnce()
    {
        SelectionResultCard card;
        int cancel = 0;
        int copy = 0;
        SelectionResultCardCallbacks callbacks;
        callbacks.cancelRequested = [&cancel]() { ++cancel; };
        callbacks.copyRequested = [&copy]() { ++copy; };
        card.setCallbacks(callbacks);
        SelectionResultCardState running;
        running.running = true;
        card.setState(running);
        pushButton(card, "selectionResultCancelButton")->click();
        pushButton(card, "selectionResultCancelButton")->click();
        QCOMPARE(cancel, 1);
        card.setState(completedState(QStringLiteral("done")));
        pushButton(card, "selectionResultCopyButton")->click();
        QCOMPARE(copy, 1);
    }

    void callbackMayDeleteCardSynchronously()
    {
        SelectionResultCard *card = new SelectionResultCard;
        QPointer<SelectionResultCard> guard(card);
        SelectionResultCardCallbacks callbacks;
        callbacks.copyRequested = [card]() { delete card; };
        card->setCallbacks(callbacks);
        card->setState(completedState(QStringLiteral("done")));
        pushButton(*card, "selectionResultCopyButton")->click();
        QVERIFY(!guard);
    }

    void longTextConfirmationOffersFullTextOrCancelWithoutTruncation()
    {
        SelectionResultCard card;
        int processFull = 0;
        SelectionResultCardCallbacks callbacks;
        callbacks.processFullTextRequested = [&processFull]() {
            ++processFull;
        };
        card.setCallbacks(callbacks);
        SelectionResultCardState state = completedState(
            QString(450, QChar(0x6d4b))
        );
        state.requiresLongTextConfirmation = true;
        card.setState(state);
        QCOMPARE(card.state().committedText.size(), 450);
        QPushButton *process = pushButton(card, "selectionLongTextProcessButton");
        QVERIFY(process);
        QVERIFY(process->isVisibleTo(&card));
        process->click();
        QCOMPARE(processFull, 1);
        QCOMPARE(card.state().committedText.size(), 450);
    }

    void rapidDeltasCoalesceLayoutWithoutDroppingText()
    {
        SelectionResultCard card;
        for (int i = 1; i <= 80; ++i) {
            SelectionResultCardState state;
            state.running = true;
            state.committedText = QString(i, QLatin1Char('x'));
            card.setState(state);
        }
        QCOMPARE(card.state().committedText.size(), 80);
        QPlainTextEdit *editor = card.findChild<QPlainTextEdit *>(
            QStringLiteral("selectionResultCommittedText")
        );
        QTRY_COMPARE(editor->toPlainText().size(), 80);
        QVERIFY(card.property("selectionRenderCount").toInt() < 10);
    }

    void longChineseTextIsScrollableAtTwoHundredPercentFont()
    {
        SelectionResultCard card;
        QFont font = card.font();
        font.setPointSizeF(qMax(18.0, font.pointSizeF() * 2.0));
        card.setFont(font);
        card.setState(completedState(QString(800, QChar(0x4e2d))));
        const QRect screen(0, 0, 760, 520);
        card.showAt(QPoint(700, 470), screen);
        QPlainTextEdit *editor = card.findChild<QPlainTextEdit *>(
            QStringLiteral("selectionResultCommittedText")
        );
        QTRY_VERIFY(editor->verticalScrollBar()->maximum() > 0);
        QVERIFY(screen.contains(card.geometry()));
        QCOMPARE(card.state().committedText.size(), 800);
    }

    void streamingGrowthRemainsInsideStoredAvailableGeometry()
    {
        SelectionResultCard card;
        const QRect screen(-900, 20, 700, 480);
        card.setState(completedState(QStringLiteral("short")));
        card.showAt(QPoint(-250, 450), screen);
        SelectionResultCardState growing;
        growing.running = true;
        growing.committedText = QString(1200, QChar(0x6d41));
        growing.provisionalText = QString(300, QChar(0x5f0f));
        card.setState(growing);
        QTest::qWait(100);
        QVERIFY(screen.contains(card.geometry()));
        QCOMPARE(card.state().committedText.size(), 1200);
    }

    void narrowScreenWrapsActionButtonsWithoutDuplication()
    {
        SelectionResultCard card;
        card.setState(completedState(QStringLiteral("done")));
        const QRect screen(0, 0, 340, 540);
        card.showAt(QPoint(20, 20), screen);
        const QStringList names = QStringList()
            << QStringLiteral("selectionResultRegenerateButton")
            << QStringLiteral("selectionResultCopyButton")
            << QStringLiteral("selectionResultReplaceButton")
            << QStringLiteral("selectionResultPinButton")
            << QStringLiteral("selectionResultCloseButton");
        for (const QString &name : names) {
            const QList<QWidget *> matches = card.findChildren<QWidget *>(name);
            QCOMPARE(matches.size(), 1);
            QWidget *button = matches.constFirst();
            if (button->isVisible()) {
                QVERIFY(button->width() >= button->sizeHint().width());
                QVERIFY(card.rect().contains(
                    button->mapTo(&card, button->rect().center())
                ));
            }
        }
        QVERIFY(screen.contains(card.geometry()));
    }
};

QTEST_MAIN(SelectionResultCardTests)
#include "selection_result_card_tests.moc"
