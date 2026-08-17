#ifndef VOCEKIT_LOG_PAGINATION_SNAPSHOT_H
#define VOCEKIT_LOG_PAGINATION_SNAPSHOT_H

#include "../config/app_settings_data.h"

// 日志页面只需要分页数量，不应持有整份可变设置。
struct LogPaginationSnapshot
{
    int initialLoadCount = 20;
    int loadMoreCount = 30;
};

LogPaginationSnapshot buildLogPaginationSnapshot(
    const AppSettingsData &settings
);

#endif // VOCEKIT_LOG_PAGINATION_SNAPSHOT_H
