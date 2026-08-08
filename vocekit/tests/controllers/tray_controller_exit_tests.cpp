#include <QtTest>

#include "../../src/controllers/tray_controller.h"

#include <QAction>
#include <QFile>
#include <QWidget>

class TrayControllerExitTests : public QObject
{
    Q_OBJECT

private slots:
    void quitActionUsesTheApplicationExitCallbackExactlyOnce();
    void missingExitCallbackIsSafe();
    void trayControllerDoesNotQuitTheApplicationDirectly();
};

void TrayControllerExitTests::quitActionUsesTheApplicationExitCallbackExactlyOnce()
{
    QWidget hub;
    int quitRequests = 0;
    TrayController::Callbacks callbacks;
    callbacks.requestApplicationQuit = [&quitRequests]() {
        ++quitRequests;
    };
    TrayController controller(&hub, callbacks);

    QAction *quit = hub.findChild<QAction *>(
        QStringLiteral("trayQuitAction")
    );
    QVERIFY(quit);
    quit->trigger();

    QCOMPARE(quitRequests, 1);
}

void TrayControllerExitTests::missingExitCallbackIsSafe()
{
    QWidget hub;
    TrayController controller(&hub, TrayController::Callbacks());

    QAction *quit = hub.findChild<QAction *>(
        QStringLiteral("trayQuitAction")
    );
    QVERIFY(quit);
    quit->trigger();
}

void TrayControllerExitTests::trayControllerDoesNotQuitTheApplicationDirectly()
{
    const QString sourcePath = QFINDTESTDATA(
        "../../src/controllers/tray_controller.cpp"
    );
    QVERIFY2(!sourcePath.isEmpty(), "Cannot find tray controller source");
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();

    QVERIFY(contents.contains("requestApplicationQuit"));
    QVERIFY(!contents.contains("&QApplication::quit"));
    QVERIFY(!contents.contains("qApp->quit"));
}

QTEST_MAIN(TrayControllerExitTests)

#include "tray_controller_exit_tests.moc"
