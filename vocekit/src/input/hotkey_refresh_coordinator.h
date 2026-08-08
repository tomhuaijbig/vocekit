#ifndef VOCEKIT_HOTKEY_REFRESH_COORDINATOR_H
#define VOCEKIT_HOTKEY_REFRESH_COORDINATOR_H

#include "global_hotkeys.h"

#include <QSet>

#include <functional>

using RegisteredHotkeyPressCallback =
    std::function<void(const QString &)>;

void dispatchRegisteredHotkeyPress(
    const QString &functionId,
    const QSet<QString> &activeHoldFunctions,
    const RegisteredHotkeyPressCallback &trackHold,
    const RegisteredHotkeyPressCallback &freezeOwner,
    const RegisteredHotkeyPressCallback &start
);

// 热键刷新门：物理按住期间只保留最新快照，最后一次松开后再应用。
class HotkeyRefreshCoordinator
{
public:
    using ApplyCallback = std::function<void(
        const GlobalHotkeySettingsSnapshot &
    )>;

    explicit HotkeyRefreshCoordinator(
        const ApplyCallback &applyCallback = ApplyCallback()
    );

    void requestRefresh(
        const GlobalHotkeySettingsSnapshot &snapshot,
        bool hasPhysicalHold
    );
    void holdPressed(const QString &functionId);
    void holdReleased(
        const QString &functionId,
        bool hasPhysicalHold
    );

private:
    void apply(const GlobalHotkeySettingsSnapshot &snapshot);
    void applyPendingIfReady(bool hasPhysicalHold);

    ApplyCallback m_applyCallback;
    QSet<QString> m_pressedFunctionIds;
    GlobalHotkeySettingsSnapshot m_pendingSnapshot;
    bool m_hasPendingSnapshot = false;
};

#endif // VOCEKIT_HOTKEY_REFRESH_COORDINATOR_H
