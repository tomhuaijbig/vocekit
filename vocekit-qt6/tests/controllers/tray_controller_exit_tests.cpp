#include <QtTest>

#include "../../src/controllers/tray_controller.h"
#include "../../src/config/app_settings_defaults.h"

#include <QAction>
#include <QActionGroup>
#include <QFile>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QWidget>

class TrayControllerExitTests : public QObject
{
    Q_OBJECT

private slots:
    void quitActionUsesTheApplicationExitCallbackExactlyOnce();
    void missingExitCallbackIsSafe();
    void trayControllerDoesNotQuitTheApplicationDirectly();
    void speechMenuUsesTheProviderCatalogAndRefreshesSelection();
};

void TrayControllerExitTests::
speechMenuUsesTheProviderCatalogAndRefreshesSelection()
{
    QWidget hub;
    QString currentProvider = speechProviderXfyun();
    QStringList selectedProviders;
    TrayController::Callbacks callbacks;
    callbacks.speechProvider = [&currentProvider]() {
        return currentProvider;
    };
    callbacks.setSpeechProvider = [&selectedProviders](const QString &id) {
        selectedProviders.append(id);
    };
    TrayController controller(&hub, callbacks);

    QActionGroup *group = hub.findChild<QActionGroup *>();
    QVERIFY(group);
    QMenu *speechMenu = qobject_cast<QMenu *>(group->parent());
    QVERIFY(speechMenu);

    QMap<QString, QAction *> providerActions;
    for (QAction *action : speechMenu->actions()) {
        const QString providerId = action->data().toString();
        if (!providerId.isEmpty()) {
            providerActions.insert(providerId, action);
        }
    }
    QStringList actionIds;
    for (const QString &providerId : providerActions.keys()) {
        actionIds.append(providerId);
    }
    QStringList expectedIds = supportedSpeechProviderIds();
    actionIds.sort();
    expectedIds.sort();
    QCOMPARE(actionIds, expectedIds);
    QVERIFY(group->isExclusive());
    QCOMPARE(group->actions().size(), supportedSpeechProviderIds().size());
    for (const QString &providerId : supportedSpeechProviderIds()) {
        QAction *action = providerActions.value(providerId);
        QVERIFY(action);
        QVERIFY(action->isCheckable());
        QCOMPARE(action->actionGroup(), group);
    }

    QSystemTrayIcon *tray = controller.findChild<QSystemTrayIcon *>();
    QVERIFY(tray);
    QVERIFY(tray->contextMenu());
    QVERIFY(QMetaObject::invokeMethod(
        tray->contextMenu(),
        "aboutToShow",
        Qt::DirectConnection
    ));
    QVERIFY(providerActions.value(speechProviderXfyun())->isChecked());
    QCOMPARE(group->checkedAction(), providerActions.value(speechProviderXfyun()));

    currentProvider = QStringLiteral(" custom ");
    QVERIFY(QMetaObject::invokeMethod(
        tray->contextMenu(),
        "aboutToShow",
        Qt::DirectConnection
    ));
    QCOMPARE(group->checkedAction(), providerActions.value(speechProviderCustom()));

    providerActions.value(speechProviderWindowsLocal())->trigger();
    QCOMPARE(selectedProviders, QStringList() << speechProviderWindowsLocal());
}

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
