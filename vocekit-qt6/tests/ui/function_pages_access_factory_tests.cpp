#include <QtTest>

#include "../../src/ui/function_pages_access_factory.h"
#include "../../src/ui/hub_settings_state.h"

#include <QFile>

class FunctionPagesAccessFactoryTests : public QObject
{
    Q_OBJECT

private slots:
    void buildsCommandPageAccessFromSharedActions();
    void buildsManagementPageAccessFromSharedActions();
    void forwardsRemovalRefreshActions();
    void handlesMissingDependencies();
    void hubWindowUsesSharedAssemblyFactory();
};

void FunctionPagesAccessFactoryTests::buildsCommandPageAccessFromSharedActions()
{
    HubSettingsState settings;
    QStringList actions;

    FunctionPagesAccessDependencies dependencies;
    dependencies.settings = &settings;
    dependencies.prompts.snapshotProvider = []() {
        PromptRuntimeSnapshot snapshot;
        snapshot.settings.promptLocked = true;
        return snapshot;
    };
    dependencies.saveSettings = [&actions]() {
        actions.append(QStringLiteral("save"));
    };
    dependencies.functionRenamed = [&actions](const QString &id) {
        actions.append(QStringLiteral("renamed:") + id);
    };
    dependencies.functionRemoved = [&actions](const QString &id) {
        actions.append(QStringLiteral("removed:") + id);
    };
    dependencies.flows.readState = [](
        const QString &,
        FunctionFlowState *,
        OperationError *
    ) {
        return true;
    };

    const FunctionPagesAccessAssembly assembly =
        createFunctionPagesAccess(dependencies);

    QCOMPARE(assembly.command.settings, &settings);
    QVERIFY(assembly.command.prompts.snapshotProvider);
    QVERIFY(assembly.command.prompts.snapshotProvider().settings.promptLocked);
    QVERIFY(assembly.command.flows.readState);
    assembly.command.saveSettings();
    QVERIFY(assembly.command.functionRenamed);
    QVERIFY(assembly.command.functionRemoved);
    assembly.command.functionRenamed(QStringLiteral("custom_1"));
    assembly.command.functionRemoved(QStringLiteral("custom_2"));
    QCOMPARE(
        actions,
        QStringList()
            << QStringLiteral("save")
            << QStringLiteral("renamed:custom_1")
            << QStringLiteral("removed:custom_2")
    );
}

void FunctionPagesAccessFactoryTests::buildsManagementPageAccessFromSharedActions()
{
    QStringList actions;
    FunctionPagesAccessDependencies dependencies;
    dependencies.summaryProvider = [](
        const QString &id,
        const QString &shortcut
    ) {
        return id + QStringLiteral(":") + shortcut;
    };
    dependencies.addFunction = [&actions]() {
        actions.append(QStringLiteral("add"));
    };
    dependencies.editFunction = [&actions](
        const QString &id,
        const QString &,
        bool custom,
        const CustomFunctionDef &
    ) {
        actions.append(
            QStringLiteral("edit:") + id
            + (custom ? QStringLiteral(":custom") : QStringLiteral(":built-in"))
        );
    };

    const FunctionPagesAccessAssembly assembly =
        createFunctionPagesAccess(dependencies);
    QVERIFY(assembly.management.addFunction);
    QVERIFY(assembly.management.editFunction);

    assembly.management.addFunction();
    FunctionManagementItem item;
    item.id = QStringLiteral("dictate");
    item.title = QStringLiteral("Dictate");
    assembly.management.editFunction(item);

    QCOMPARE(
        actions,
        QStringList()
            << QStringLiteral("add")
            << QStringLiteral("edit:dictate:built-in")
    );
}

void FunctionPagesAccessFactoryTests::forwardsRemovalRefreshActions()
{
    AppSettingsData source;
    FunctionSettings custom;
    custom.id = QStringLiteral("custom_1");
    custom.name = QStringLiteral("Custom 1");
    source.functions.append(custom);

    HubWindowAccess stateAccess;
    stateAccess.settingsSnapshotProvider = [&source]() { return source; };
    HubSettingsState settings(stateAccess);
    QStringList actions;

    FunctionPagesAccessDependencies dependencies;
    dependencies.settings = &settings;
    dependencies.flows.removeCustomFunction = [&](
        const QString &id,
        OperationError *
    ) {
        actions.append(QStringLiteral("remove:") + id);
        source.functions.remove(source.functionIndex(id));
        return true;
    };

    const FunctionPagesAccessAssembly assembly =
        createFunctionPagesAccess(dependencies);
    FunctionManagementItem item;
    item.id = custom.id;
    item.custom = true;
    assembly.management.removeFunction(item);

    QVERIFY(settings.customFunctions().isEmpty());
    QCOMPARE(
        actions,
        QStringList() << QStringLiteral("remove:custom_1")
    );
}

void FunctionPagesAccessFactoryTests::handlesMissingDependencies()
{
    const FunctionPagesAccessAssembly assembly =
        createFunctionPagesAccess(FunctionPagesAccessDependencies());

    QVERIFY(assembly.command.settings == nullptr);
    QVERIFY(assembly.management.itemsProvider);
    QVERIFY(assembly.management.itemsProvider().isEmpty());
    QVERIFY(assembly.management.removeFunction);
}

void FunctionPagesAccessFactoryTests::hubWindowUsesSharedAssemblyFactory()
{
    const QString sourcePath = QFINDTESTDATA("../../src/ui/hub_window.cpp");
    const QString workspacePath = QFINDTESTDATA(
        "../../src/ui/function_workspace_controller.cpp"
    );
    QVERIFY2(!sourcePath.isEmpty(), "Cannot find HubWindow source file");
    QVERIFY2(!workspacePath.isEmpty(), "Cannot find function workspace source file");
    QFile source(sourcePath);
    QFile workspace(workspacePath);
    QVERIFY(source.open(QIODevice::ReadOnly));
    QVERIFY(workspace.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();
    const QByteArray workspaceContents = workspace.readAll();

    QVERIFY(contents.contains("FunctionWorkspaceController"));
    QVERIFY(workspaceContents.contains("createFunctionPagesAccess(dependencies)"));
    QVERIFY(!contents.contains("FunctionPagesAccessDependencies"));
    QVERIFY(!contents.contains("FunctionCommandPageAccessDependencies dependencies;"));
    QVERIFY(!contents.contains("FunctionManagementPageAccessDependencies dependencies;"));
}

QTEST_APPLESS_MAIN(FunctionPagesAccessFactoryTests)

#include "function_pages_access_factory_tests.moc"
