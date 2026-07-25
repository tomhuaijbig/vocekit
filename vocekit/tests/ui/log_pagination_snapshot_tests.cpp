#include <QtTest>

#include "../../src/ui/log_pagination_snapshot.h"

class LogPaginationSnapshotTests : public QObject
{
    Q_OBJECT

private slots:
    void usesConfiguredCounts();
    void clampsCountsToSafeRange();
};

void LogPaginationSnapshotTests::usesConfiguredCounts()
{
    AppSettingsData settings;
    settings.logInitialLoadCount = 24;
    settings.logLoadMoreCount = 36;

    const LogPaginationSnapshot snapshot =
        buildLogPaginationSnapshot(settings);

    QCOMPARE(snapshot.initialLoadCount, 24);
    QCOMPARE(snapshot.loadMoreCount, 36);
}

void LogPaginationSnapshotTests::clampsCountsToSafeRange()
{
    AppSettingsData settings;
    settings.logInitialLoadCount = 1;
    settings.logLoadMoreCount = 900;

    const LogPaginationSnapshot snapshot =
        buildLogPaginationSnapshot(settings);

    QCOMPARE(snapshot.initialLoadCount, 5);
    QCOMPARE(snapshot.loadMoreCount, 500);
}

QTEST_APPLESS_MAIN(LogPaginationSnapshotTests)

#include "log_pagination_snapshot_tests.moc"
