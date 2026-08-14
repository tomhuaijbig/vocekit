#include "selection_context_policy.h"

#include <QFileInfo>

namespace {

QString executableBasename(const QString &value)
{
    QString normalized = value.trimmed();
    normalized.replace(QLatin1Char('\\'), QLatin1Char('/'));
    return QFileInfo(normalized).fileName().trimmed().toLower();
}

bool anchorCoordinatesNear(
    const QRect &left,
    const QRect &right,
    int tolerance)
{
    return qAbs(left.left() - right.left()) <= tolerance
        && qAbs(left.top() - right.top()) <= tolerance
        && qAbs(left.right() - right.right()) <= tolerance
        && qAbs(left.bottom() - right.bottom()) <= tolerance;
}

} // namespace

SelectionContextEligibility selectionContextEligibility(
    const SelectionContextPolicyInput &input)
{
    switch (input.snapshot.sensitivity) {
    case SelectionSensitivity::Password:
    case SelectionSensitivity::Protected:
        return SelectionContextEligibility::Sensitive;
    case SelectionSensitivity::PermissionDenied:
        return SelectionContextEligibility::PermissionDenied;
    case SelectionSensitivity::SecureDesktop:
        return SelectionContextEligibility::SecureDesktop;
    case SelectionSensitivity::Normal:
        break;
    }

    const QString text = input.snapshot.text.trimmed();
    if (text.isEmpty()) {
        return SelectionContextEligibility::Empty;
    }
    if (text.size() < qMax(0, input.minimumTextLength)) {
        return SelectionContextEligibility::TooShort;
    }
    if (!input.snapshot.targetWindow || !input.targetWindowValid) {
        return SelectionContextEligibility::InvalidTargetWindow;
    }
    if (!input.targetWindowForeground) {
        return SelectionContextEligibility::StaleForeground;
    }
    if (input.currentProcessId != 0
        && input.snapshot.targetProcessId == input.currentProcessId) {
        return SelectionContextEligibility::OwnProcess;
    }

    const QString targetExecutable = executableBasename(
        input.snapshot.targetExecutable
    );
    if (!targetExecutable.isEmpty()) {
        for (const QString &blocked : input.blockedApplications) {
            if (targetExecutable == executableBasename(blocked)) {
                return SelectionContextEligibility::BlockedApplication;
            }
        }
    }
    return SelectionContextEligibility::Eligible;
}

bool selectionSnapshotsEquivalent(
    const SelectionSnapshot &left,
    const SelectionSnapshot &right,
    int anchorTolerancePixels)
{
    if (left.targetWindow != right.targetWindow
        || left.text != right.text) {
        return false;
    }
    if (!left.anchorRect.isValid() || !right.anchorRect.isValid()) {
        return !left.anchorRect.isValid() && !right.anchorRect.isValid();
    }
    return anchorCoordinatesNear(
        left.anchorRect,
        right.anchorRect,
        qMax(0, anchorTolerancePixels)
    );
}
