#include "history_page_access_factory.h"

#include "hub_settings_state.h"

#include "../input/hotkey_definitions.h"

namespace {

HistoryPageSettingsSnapshot buildSnapshot(HubSettingsState *settings)
{
    HistoryPageSettingsSnapshot snapshot;
    if (!settings) {
        return snapshot;
    }

    snapshot.recordDirectoryPath = settings->recordDirectoryPath();
    snapshot.customFunctions = settings->customFunctions();
    snapshot.favoriteFolders = settings->favoriteFolders();
    snapshot.initialLoadCount = settings->historyInitialLoadCount();
    snapshot.loadMoreCount = settings->historyLoadMoreCount();
    snapshot.speechProvider = settings->speechProvider();
    snapshot.useSystemProxy = settings->useSystemProxy();
    for (const HotkeyDef &definition : coreFunctionDefs()) {
        snapshot.speechNetworkPolicies.insert(
            definition.id,
            settings->networkPoliciesFor(definition.id).speech
        );
    }
    for (const CustomFunctionDef &function : snapshot.customFunctions) {
        snapshot.speechNetworkPolicies.insert(
            function.id,
            settings->networkPoliciesFor(function.id).speech
        );
    }
    return snapshot;
}

} // namespace

HistoryPageAccess createHistoryPageAccess(
    HubSettingsState *settings,
    const std::function<void(const QStringList &, bool)> &historyChanged
)
{
    HistoryPageAccess access;
    access.snapshotProvider = [settings]() {
        return buildSnapshot(settings);
    };
    access.addFavoriteFolder = [settings](const QString &name) {
        return settings ? settings->addFavoriteFolder(name) : false;
    };
    access.saveSettings = [settings]() {
        return settings ? settings->save() : false;
    };
    access.historyChanged = historyChanged;
    return access;
}
