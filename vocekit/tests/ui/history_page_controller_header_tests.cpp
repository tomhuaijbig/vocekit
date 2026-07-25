#include <QtTest>

#include "../../src/ui/history_page_controller.h"

#include <type_traits>

class HistoryPageControllerHeaderTests : public QObject
{
    Q_OBJECT

private slots:
    void constructsFromAccessOnly();
};

void HistoryPageControllerHeaderTests::constructsFromAccessOnly()
{
    QVERIFY((std::is_constructible<
        HistoryPageController,
        QWidget *,
        const HistoryPageAccess &
    >::value));
}

QTEST_MAIN(HistoryPageControllerHeaderTests)

#include "history_page_controller_header_tests.moc"
