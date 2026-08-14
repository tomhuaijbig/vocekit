#include <QtTest>

#include "../../src/controllers/selection_context_policy.h"

namespace {

SelectedTextNativeWindowHandle windowHandle(quintptr value)
{
    return reinterpret_cast<SelectedTextNativeWindowHandle>(value);
}

SelectionSnapshot usableSnapshot(
    const QString &text = QStringLiteral("hello"),
    SelectedTextNativeWindowHandle window = windowHandle(1),
    const QRect &anchor = QRect(100, 100, 80, 20))
{
    SelectionSnapshot snapshot;
    snapshot.text = text;
    snapshot.targetWindow = window;
    snapshot.targetProcessId = 7;
    snapshot.targetExecutable = QStringLiteral("notepad.exe");
    snapshot.anchorRect = anchor;
    snapshot.rectangles.append(anchor);
    snapshot.sensitivity = SelectionSensitivity::Normal;
    return snapshot;
}

SelectionContextPolicyInput eligibleInput()
{
    SelectionContextPolicyInput input;
    input.snapshot = usableSnapshot();
    input.targetWindowValid = true;
    input.targetWindowForeground = true;
    input.minimumTextLength = 2;
    input.currentProcessId = 42;
    return input;
}

} // namespace

class SelectionContextPolicyTests : public QObject
{
    Q_OBJECT

private slots:
    void emptyAndWhitespaceAreClosedOut()
    {
        SelectionContextPolicyInput input = eligibleInput();
        input.snapshot.text.clear();
        QCOMPARE(
            selectionContextEligibility(input),
            SelectionContextEligibility::Empty
        );
        input.snapshot.text = QStringLiteral("  \t\r\n ");
        QCOMPARE(
            selectionContextEligibility(input),
            SelectionContextEligibility::Empty
        );
    }

    void minimumLengthUsesTrimmedText()
    {
        SelectionContextPolicyInput input = eligibleInput();
        input.minimumTextLength = 4;
        input.snapshot.text = QStringLiteral("  abc  ");
        QCOMPARE(
            selectionContextEligibility(input),
            SelectionContextEligibility::TooShort
        );
        input.snapshot.text = QStringLiteral("  abcd  ");
        QCOMPARE(
            selectionContextEligibility(input),
            SelectionContextEligibility::Eligible
        );
    }

    void targetWindowMustExistAndRemainValid()
    {
        SelectionContextPolicyInput input = eligibleInput();
        input.snapshot.targetWindow = nullptr;
        QCOMPARE(
            selectionContextEligibility(input),
            SelectionContextEligibility::InvalidTargetWindow
        );
        input.snapshot.targetWindow = windowHandle(1);
        input.targetWindowValid = false;
        QCOMPARE(
            selectionContextEligibility(input),
            SelectionContextEligibility::InvalidTargetWindow
        );
    }

    void foregroundChangeDuringProbeIsStale()
    {
        SelectionContextPolicyInput input = eligibleInput();
        input.targetWindowForeground = false;
        QCOMPARE(
            selectionContextEligibility(input),
            SelectionContextEligibility::StaleForeground
        );
    }

    void sensitivityMapsToClosedEligibility_data()
    {
        QTest::addColumn<int>("sensitivity");
        QTest::addColumn<int>("eligibility");
        QTest::newRow("password")
            << int(SelectionSensitivity::Password)
            << int(SelectionContextEligibility::Sensitive);
        QTest::newRow("protected")
            << int(SelectionSensitivity::Protected)
            << int(SelectionContextEligibility::Sensitive);
        QTest::newRow("permission")
            << int(SelectionSensitivity::PermissionDenied)
            << int(SelectionContextEligibility::PermissionDenied);
        QTest::newRow("secure-desktop")
            << int(SelectionSensitivity::SecureDesktop)
            << int(SelectionContextEligibility::SecureDesktop);
    }

    void sensitivityMapsToClosedEligibility()
    {
        QFETCH(int, sensitivity);
        QFETCH(int, eligibility);
        SelectionContextPolicyInput input = eligibleInput();
        input.snapshot.sensitivity = SelectionSensitivity(sensitivity);
        input.snapshot.text.clear();
        QCOMPARE(
            int(selectionContextEligibility(input)),
            eligibility
        );
    }

    void ownProcessIsNeverEligible()
    {
        SelectionContextPolicyInput input = eligibleInput();
        input.snapshot.targetProcessId = 42;
        QCOMPARE(
            selectionContextEligibility(input),
            SelectionContextEligibility::OwnProcess
        );
        input.currentProcessId = 0;
        input.snapshot.targetProcessId = 0;
        QCOMPARE(
            selectionContextEligibility(input),
            SelectionContextEligibility::Eligible
        );
    }

    void blockedExecutableIsCaseInsensitiveAndBasenameOnly()
    {
        SelectionContextPolicyInput input = eligibleInput();
        input.snapshot.targetExecutable =
            QStringLiteral("C:\\Windows\\System32\\NOTEPAD.EXE");
        input.blockedApplications = QStringList()
            << QStringLiteral("  c:/tools/notepad.exe  ")
            << QStringLiteral("other.exe");
        QCOMPARE(
            selectionContextEligibility(input),
            SelectionContextEligibility::BlockedApplication
        );

        input.blockedApplications = QStringList()
            << QStringLiteral("wordpad.exe");
        QCOMPARE(
            selectionContextEligibility(input),
            SelectionContextEligibility::Eligible
        );
    }

    void allClearInputIsEligible()
    {
        QCOMPARE(
            selectionContextEligibility(eligibleInput()),
            SelectionContextEligibility::Eligible
        );
    }

    void duplicateRequiresSameWindowTextAndNearAnchor()
    {
        const SelectionSnapshot first = usableSnapshot();
        SelectionSnapshot near = first;
        near.anchorRect.translate(2, 2);
        QVERIFY(selectionSnapshotsEquivalent(first, near, 4));

        SelectionSnapshot moved = first;
        moved.anchorRect.translate(5, 0);
        QVERIFY(!selectionSnapshotsEquivalent(first, moved, 4));

        SelectionSnapshot resized = first;
        resized.anchorRect.setWidth(first.anchorRect.width() + 5);
        QVERIFY(!selectionSnapshotsEquivalent(first, resized, 4));

        SelectionSnapshot changedText = first;
        changedText.text = QStringLiteral("Hello");
        QVERIFY(!selectionSnapshotsEquivalent(first, changedText, 4));

        SelectionSnapshot changedWindow = first;
        changedWindow.targetWindow = windowHandle(2);
        QVERIFY(!selectionSnapshotsEquivalent(first, changedWindow, 4));
    }

    void invalidAnchorsOnlyMatchWhenBothAreInvalid()
    {
        SelectionSnapshot left = usableSnapshot();
        SelectionSnapshot right = left;
        left.anchorRect = QRect();
        right.anchorRect = QRect();
        QVERIFY(selectionSnapshotsEquivalent(left, right));
        right.anchorRect = QRect(1, 1, 2, 2);
        QVERIFY(!selectionSnapshotsEquivalent(left, right));
    }
};

QTEST_APPLESS_MAIN(SelectionContextPolicyTests)
#include "selection_context_policy_tests.moc"
