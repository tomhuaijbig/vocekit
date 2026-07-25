#ifndef VOCEKIT_HISTORY_PAGE_ACCESS_FACTORY_H
#define VOCEKIT_HISTORY_PAGE_ACCESS_FACTORY_H

#include "history_page_controller.h"

#include <functional>

class HubSettingsState;

// 将类型化设置转换为历史页需要的最小访问对象。
HistoryPageAccess createHistoryPageAccess(
    HubSettingsState *settings,
    const std::function<void(const QStringList &, bool)> &historyChanged =
        std::function<void(const QStringList &, bool)>()
);

#endif // VOCEKIT_HISTORY_PAGE_ACCESS_FACTORY_H
