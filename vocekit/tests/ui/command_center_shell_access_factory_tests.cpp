#include <QtTest>

#include "../../src/ui/command_center_shell_access_factory.h"
#include "../../src/ui/hub_settings_state.h"

#include <QFile>

class CommandCenterShellAccessFactoryTests : public QObject
{
    Q_OBJECT

private slots:
    void buildsCoreAndCustomFunctionItems();
    void forwardsNavigationActions();
    void handlesMissingDependencies();
    void hubWindowUsesIndependentFactory();
};

void CommandCenterShellAccessFactoryTests::buildsCoreAndCustomFunctionItems()
{
    AppSettingsData source;

    FunctionSettings dictate;
    dictate.id = QStringLiteral("dictate");
    dictate.name = QStringLiteral("Dictate");
    dictate.builtIn = true;
    dictate.shortcut = QStringLiteral("Alt+X");
    source.functions.append(dictate);

    FunctionSettings custom;
    custom.id = QStringLiteral("custom_1");
    custom.name = QStringLiteral("Custom action");
    custom.builtIn = false;
    custom.shortcut = QStringLiteral("Ctrl+Alt+1");
    source.functions.append(custom);

    HubWindowAccess stateAccess;
    stateAccess.settingsSnapshotProvider = [source]() { return source; };
    HubSettingsState settings(stateAccess);

    CommandCenterShellAccessFactoryDependencies dependencies;
    dependencies.settings = &settings;
    const CommandCenterShellAccess access =
        createCommandCenterShellAccess(dependencies);

    QVERIFY(access.functionsProvider);
    const QVector<CommandCenterFunctionItem> items = access.functionsProvider();
    QCOMPARE(items.size(), 4);
    QCOMPARE(items.at(0).id, QStringLiteral("dictate"));
    QCOMPARE(items.at(0).shortcut, QStringLiteral("Alt + X"));
    QCOMPARE(items.last().id, QStringLiteral("custom_1"));
    QCOMPARE(items.last().title, QStringLiteral("Custom action"));
    QCOMPARE(items.last().shortcut, QStringLiteral("Ctrl + Alt + 1"));
}

void CommandCenterShellAccessFactoryTests::forwardsNavigationActions()
{
    QString openedFunction;
    QString openedTool;
    QString missedKeyword;
    bool addRequested = false;

    CommandCenterShellAccessFactoryDependencies dependencies;
    dependencies.openFunction = [&openedFunction](const QString &id) {
        openedFunction = id;
    };
    dependencies.openTool = [&openedTool](const QString &id) {
        openedTool = id;
    };
    dependencies.addFunction = [&addRequested]() { addRequested = true; };
    dependencies.searchMissed = [&missedKeyword](const QString &keyword) {
        missedKeyword = keyword;
    };

    const CommandCenterShellAccess access =
        createCommandCenterShellAccess(dependencies);
    access.openFunction(QStringLiteral("translate"));
    access.openTool(QStringLiteral("history"));
    access.addFunction();
    access.searchMissed(QStringLiteral("missing"));

    QCOMPARE(openedFunction, QStringLiteral("translate"));
    QCOMPARE(openedTool, QStringLiteral("history"));
    QVERIFY(addRequested);
    QCOMPARE(missedKeyword, QStringLiteral("missing"));
}

void CommandCenterShellAccessFactoryTests::handlesMissingDependencies()
{
    const CommandCenterShellAccess access =
        createCommandCenterShellAccess(
            CommandCenterShellAccessFactoryDependencies()
        );

    QVERIFY(access.functionsProvider);
    QVERIFY(access.openFunction);
    QVERIFY(access.openTool);
    QVERIFY(access.addFunction);
    QVERIFY(access.searchMissed);
    QVERIFY(access.functionsProvider().isEmpty());
    access.openFunction(QString());
    access.openTool(QString());
    access.addFunction();
    access.searchMissed(QString());
}

void CommandCenterShellAccessFactoryTests::hubWindowUsesIndependentFactory()
{
    const QString sourcePath = QFINDTESTDATA("../../src/ui/hub_window.cpp");
    QVERIFY2(!sourcePath.isEmpty(), "HubWindow source file not found");
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();

    QVERIFY(contents.contains("createCommandCenterShellAccess(dependencies)"));
    QVERIFY(!contents.contains("CommandCenterShellAccess access;"));
    QVERIFY(!contents.contains("CommandCenterFunctionItem item;"));
}

QTEST_APPLESS_MAIN(CommandCenterShellAccessFactoryTests)

#include "command_center_shell_access_factory_tests.moc"
