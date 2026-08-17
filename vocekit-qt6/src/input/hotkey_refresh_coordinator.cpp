#include "hotkey_refresh_coordinator.h"

void dispatchRegisteredHotkeyPress(
    const QString &functionId,
    const QSet<QString> &activeHoldFunctions,
    const RegisteredHotkeyPressCallback &trackHold,
    const RegisteredHotkeyPressCallback &freezeOwner,
    const RegisteredHotkeyPressCallback &start)
{
    const QString id = functionId.trimmed();
    if (id.isEmpty()) {
        return;
    }
    if (activeHoldFunctions.contains(id)) {
        if (trackHold) {
            trackHold(id);
        }
        if (freezeOwner) {
            freezeOwner(id);
        }
    }
    if (start) {
        start(id);
    }
}

HotkeyRefreshCoordinator::HotkeyRefreshCoordinator(
    const ApplyCallback &applyCallback)
    : m_applyCallback(applyCallback)
{
}

void HotkeyRefreshCoordinator::requestRefresh(
    const GlobalHotkeySettingsSnapshot &snapshot,
    bool hasPhysicalHold)
{
    if (hasPhysicalHold || !m_pressedFunctionIds.isEmpty()) {
        m_pendingSnapshot = snapshot;
        m_hasPendingSnapshot = true;
        return;
    }
    m_hasPendingSnapshot = false;
    apply(snapshot);
}

void HotkeyRefreshCoordinator::holdPressed(
    const QString &functionId)
{
    const QString id = functionId.trimmed();
    if (!id.isEmpty()) {
        m_pressedFunctionIds.insert(id);
    }
}

void HotkeyRefreshCoordinator::holdReleased(
    const QString &functionId,
    bool hasPhysicalHold)
{
    m_pressedFunctionIds.remove(functionId.trimmed());
    applyPendingIfReady(hasPhysicalHold);
}

void HotkeyRefreshCoordinator::apply(
    const GlobalHotkeySettingsSnapshot &snapshot)
{
    if (m_applyCallback) {
        m_applyCallback(snapshot);
    }
}

void HotkeyRefreshCoordinator::applyPendingIfReady(
    bool hasPhysicalHold)
{
    if (!m_hasPendingSnapshot
        || hasPhysicalHold
        || !m_pressedFunctionIds.isEmpty()) {
        return;
    }
    const GlobalHotkeySettingsSnapshot latest =
        m_pendingSnapshot;
    m_hasPendingSnapshot = false;
    apply(latest);
}
