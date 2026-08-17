#include "log_pagination_snapshot.h"

#include <QtGlobal>

LogPaginationSnapshot buildLogPaginationSnapshot(
    const AppSettingsData &settings
)
{
    LogPaginationSnapshot snapshot;
    snapshot.initialLoadCount = qBound(
        5,
        settings.logInitialLoadCount,
        500
    );
    snapshot.loadMoreCount = qBound(
        5,
        settings.logLoadMoreCount,
        500
    );
    return snapshot;
}
