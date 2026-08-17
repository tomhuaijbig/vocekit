#ifndef VOCEKIT_SELECTION_CONTEXT_POLICY_H
#define VOCEKIT_SELECTION_CONTEXT_POLICY_H

#include "../input/selection_snapshot.h"

#include <QStringList>

enum class SelectionContextEligibility
{
    Eligible,
    Empty,
    TooShort,
    InvalidTargetWindow,
    StaleForeground,
    Sensitive,
    PermissionDenied,
    SecureDesktop,
    OwnProcess,
    BlockedApplication
};

struct SelectionContextPolicyInput
{
    SelectionSnapshot snapshot;
    bool targetWindowValid = false;
    bool targetWindowForeground = false;
    int minimumTextLength = 2;
    quint32 currentProcessId = 0;
    QStringList blockedApplications;
};

SelectionContextEligibility selectionContextEligibility(
    const SelectionContextPolicyInput &input
);
bool selectionSnapshotsEquivalent(
    const SelectionSnapshot &left,
    const SelectionSnapshot &right,
    int anchorTolerancePixels = 4
);

#endif // VOCEKIT_SELECTION_CONTEXT_POLICY_H
