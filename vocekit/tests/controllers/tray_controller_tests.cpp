#include <QtTest>

#include "../../src/controllers/tray_controller.h"

#include <QAction>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QWidget>

namespace {

QAction *actionById(QMenu *menu, const QString &id)
{
    const QList<QAction *> actions = menu->findChildren<QAction *>();
    for (QAction *action : actions) {
        if (action && action->data().toString() == id) {
            return action;
        }
    }
    return nullptr;
}

} // namespace

class TrayControllerTests : public QObject
{
    Q_OBJECT

private slots:
    void showsEnablePauseThirtyMinutesAndResumeActions();
    void refreshesSelectionContextStateBeforeEveryShow();
};

void TrayControllerTests::showsEnablePauseThirtyMinutesAndResumeActions()
{
    QWidget hub;
    int enableCalls = 0;
    int pauseCalls = 0;
    int resumeCalls = 0;
    TrayController::Callbacks callbacks;
    callbacks.selectionContextEnabled = []() { return true; };
    callbacks.selectionContextPaused = []() { return false; };
    callbacks.setSelectionContextEnabled = [&](bool enabled) {
        QCOMPARE(enabled, false);
        ++enableCalls;
    };
    callbacks.pauseSelectionContextThirtyMinutes = [&]() { ++pauseCalls; };
    callbacks.resumeSelectionContext = [&]() { ++resumeCalls; };
    TrayController controller(&hub, callbacks);

    QSystemTrayIcon *tray = controller.findChild<QSystemTrayIcon *>();
    QVERIFY(tray);
    QMenu *menu = tray->contextMenu();
    QVERIFY(menu);
    QAction *enable = actionById(menu, QStringLiteral("selection-context-enable"));
    QAction *pause = actionById(menu, QStringLiteral("selection-context-pause-30"));
    QAction *resume = actionById(menu, QStringLiteral("selection-context-resume"));
    QVERIFY(enable);
    QVERIFY(pause);
    QVERIFY(resume);
    QVERIFY(enable->isCheckable());
    QVERIFY(pause->isCheckable());
    QVERIFY(resume->isCheckable());
    QVERIFY(pause->actionGroup());
    QCOMPARE(pause->actionGroup(), resume->actionGroup());
    QVERIFY(pause->actionGroup()->isExclusive());

    QVERIFY(QMetaObject::invokeMethod(menu, "aboutToShow", Qt::DirectConnection));
    enable->trigger();
    pause->trigger();
    resume->trigger();
    QCOMPARE(enableCalls, 1);
    QCOMPARE(pauseCalls, 1);
    QCOMPARE(resumeCalls, 1);
}

void TrayControllerTests::refreshesSelectionContextStateBeforeEveryShow()
{
    QWidget hub;
    bool enabledValue = true;
    bool pausedValue = false;
    TrayController::Callbacks callbacks;
    callbacks.selectionContextEnabled = [&]() { return enabledValue; };
    callbacks.selectionContextPaused = [&]() { return pausedValue; };
    TrayController controller(&hub, callbacks);

    QSystemTrayIcon *tray = controller.findChild<QSystemTrayIcon *>();
    QVERIFY(tray);
    QMenu *menu = tray->contextMenu();
    QAction *enable = actionById(menu, QStringLiteral("selection-context-enable"));
    QAction *pause = actionById(menu, QStringLiteral("selection-context-pause-30"));
    QAction *resume = actionById(menu, QStringLiteral("selection-context-resume"));
    QVERIFY(enable);
    QVERIFY(pause);
    QVERIFY(resume);

    QVERIFY(QMetaObject::invokeMethod(menu, "aboutToShow", Qt::DirectConnection));
    QVERIFY(enable->isChecked());
    QVERIFY(resume->isChecked());

    enabledValue = false;
    pausedValue = true;
    QVERIFY(QMetaObject::invokeMethod(menu, "aboutToShow", Qt::DirectConnection));
    QVERIFY(!enable->isChecked());
    QVERIFY(pause->isChecked());
}

QTEST_MAIN(TrayControllerTests)

#include "tray_controller_tests.moc"
