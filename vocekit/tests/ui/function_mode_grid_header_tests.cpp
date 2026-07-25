#include <QtTest>

#include "../../src/ui/function_mode_grid.h"

#include <type_traits>

class FunctionModeGridHeaderTests : public QObject
{
    Q_OBJECT

private slots:
    void constructsFromAccessOnly();
};

void FunctionModeGridHeaderTests::constructsFromAccessOnly()
{
    QVERIFY((std::is_constructible<
        FunctionModeGrid,
        const FunctionModeGridAccess &,
        QWidget *
    >::value));
}

QTEST_MAIN(FunctionModeGridHeaderTests)

#include "function_mode_grid_header_tests.moc"
