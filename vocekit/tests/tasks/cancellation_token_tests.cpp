#include <QtTest>

#include "../../src/tasks/cancellation_token.h"

class CancellationTokenTests : public QObject
{
    Q_OBJECT

private slots:
    void sharesCancellationState()
    {
        CancellationSource source;
        const CancellationToken first = source.token();
        const CancellationToken second = source.token();

        QVERIFY(!first.isCancellationRequested());
        QVERIFY(!second.isCancellationRequested());
        source.cancel();
        QVERIFY(first.isCancellationRequested());
        QVERIFY(second.isCancellationRequested());
    }

    void keepsExecutionIdsSeparate()
    {
        const CancellationSource first;
        const CancellationSource second;

        QVERIFY(first.executionId().isValid());
        QVERIFY(second.executionId().isValid());
        QVERIFY(first.executionId() != second.executionId());
        QCOMPARE(first.token().executionId(), first.executionId());
    }
};

QTEST_MAIN(CancellationTokenTests)
#include "cancellation_token_tests.moc"
