#include <QtTest>
#include <QLineEdit>
#include <QVBoxLayout>

#include "../../src/ui/reorderable_card_column.h"

class ReorderableCardColumnTests : public QObject
{
    Q_OBJECT

private slots:
    void movesCardsAndReportsTheNewOrder();
    void rejectsInvalidMoves();
    void keepsTheDragCursorOutOfTheCardBody();
};

void ReorderableCardColumnTests::movesCardsAndReportsTheNewOrder()
{
    ReorderableCardColumn column;
    column.addCard(QStringLiteral("voice"), new QWidget);
    column.addCard(QStringLiteral("selection"), new QWidget);
    column.addCard(QStringLiteral("screenshot"), new QWidget);

    QStringList reported;
    column.setOrderChangedCallback(
        [&reported](const QStringList &order) {
            reported = order;
        }
    );

    QVERIFY(column.moveCard(0, 1));
    QCOMPARE(
        column.order(),
        QStringList()
            << QStringLiteral("selection")
            << QStringLiteral("voice")
            << QStringLiteral("screenshot")
    );
    QCOMPARE(reported, column.order());
}

void ReorderableCardColumnTests::rejectsInvalidMoves()
{
    ReorderableCardColumn column;
    column.addCard(QStringLiteral("voice"), new QWidget);

    QVERIFY(!column.moveCard(-1, 0));
    QVERIFY(!column.moveCard(0, 1));
    QVERIFY(!column.moveCard(0, 0));
}

void ReorderableCardColumnTests::keepsTheDragCursorOutOfTheCardBody()
{
    ReorderableCardColumn column;
    auto *card = new QWidget;
    auto *layout = new QVBoxLayout(card);
    auto *header = new QWidget;
    auto *body = new QLineEdit;
    header->setFixedHeight(42);
    body->setFixedHeight(36);
    layout->addWidget(header);
    layout->addWidget(body);
    const Qt::CursorShape originalBodyCursor = body->cursor().shape();
    column.addCard(QStringLiteral("voice"), card, header);
    column.resize(360, 120);
    column.show();
    QVERIFY(QTest::qWaitForWindowExposed(&column));

    const QPoint pressPosition = header->rect().center();
    QMouseEvent pressEvent(
        QEvent::MouseButtonPress,
        pressPosition,
        header->mapToGlobal(pressPosition),
        Qt::LeftButton,
        Qt::LeftButton,
        Qt::NoModifier
    );
    QApplication::sendEvent(header, &pressEvent);
    QTest::qWait(90);

    const QPoint movePosition =
        pressPosition + QPoint(QApplication::startDragDistance() + 8, 0);
    QMouseEvent moveEvent(
        QEvent::MouseMove,
        movePosition,
        header->mapToGlobal(movePosition),
        Qt::NoButton,
        Qt::LeftButton,
        Qt::NoModifier
    );
    QApplication::sendEvent(header, &moveEvent);

    QMouseEvent releaseEvent(
        QEvent::MouseButtonRelease,
        movePosition,
        header->mapToGlobal(movePosition),
        Qt::LeftButton,
        Qt::NoButton,
        Qt::NoModifier
    );
    QApplication::sendEvent(header, &releaseEvent);

    QCOMPARE(header->cursor().shape(), Qt::OpenHandCursor);
    QCOMPARE(body->cursor().shape(), originalBodyCursor);
}

QTEST_MAIN(ReorderableCardColumnTests)

#include "reorderable_card_column_tests.moc"
