#include <QtTest>

#include "../../src/ui/function_editor_dialog_access_factory.h"

#include <QFile>

class FunctionEditorDialogAccessFactoryTests : public QObject
{
    Q_OBJECT

private slots:
    void providesTypedDependencies();
    void forwardsSaveAction();
    void exposesOnlyTheSaveChangeBoundary();
    void handlesMissingCallbacks();
    void hubWindowUsesIndependentFactory();
};

void FunctionEditorDialogAccessFactoryTests::providesTypedDependencies()
{
    FunctionEditorDialogAccessFactoryDependencies dependencies;
    dependencies.settings = reinterpret_cast<HubSettingsState *>(quintptr(1));
    dependencies.prompts.snapshotProvider = []() {
        PromptRuntimeSnapshot snapshot;
        snapshot.settings.promptLocked = true;
        return snapshot;
    };

    const FunctionEditorDialogAccess access =
        createFunctionEditorDialogAccess(dependencies);

    QCOMPARE(access.settings, dependencies.settings);
    QVERIFY(access.prompts.snapshotProvider);
    QVERIFY(access.prompts.snapshotProvider().settings.promptLocked);
}

void FunctionEditorDialogAccessFactoryTests::forwardsSaveAction()
{
    int saves = 0;
    FunctionEditorDialogAccessFactoryDependencies dependencies;
    dependencies.saveSettings = [&saves]() { ++saves; };

    const FunctionEditorDialogAccess access =
        createFunctionEditorDialogAccess(dependencies);
    QVERIFY(access.saveSettings);
    access.saveSettings();

    QCOMPARE(saves, 1);
}

void FunctionEditorDialogAccessFactoryTests::exposesOnlyTheSaveChangeBoundary()
{
    const QString headerPath = QFINDTESTDATA(
        "../../src/ui/function_editor_dialog_access_factory.h"
    );
    QVERIFY2(!headerPath.isEmpty(), "Cannot find editor access factory header");
    QFile header(headerPath);
    QVERIFY(header.open(QIODevice::ReadOnly));
    const QByteArray contents = header.readAll();

    QVERIFY(contents.contains("saveSettings"));
    QVERIFY(!contents.contains("refreshSettingsPanel"));
    QVERIFY(!contents.contains("refreshFunctionManagement"));
    QVERIFY(!contents.contains("refreshFunctionModes"));
    QVERIFY(!contents.contains("refreshPromptSelector"));
}

void FunctionEditorDialogAccessFactoryTests::handlesMissingCallbacks()
{
    const FunctionEditorDialogAccess access = createFunctionEditorDialogAccess(
        FunctionEditorDialogAccessFactoryDependencies()
    );

    QVERIFY(!access.settings);
    QVERIFY(access.saveSettings);
    access.saveSettings();
}

void FunctionEditorDialogAccessFactoryTests::hubWindowUsesIndependentFactory()
{
    const QString hubPath = QFINDTESTDATA("../../src/ui/hub_window.cpp");
    const QString coordinatorPath = QFINDTESTDATA(
        "../../src/ui/function_editor_coordinator.cpp"
    );
    const QString workspacePath = QFINDTESTDATA(
        "../../src/ui/function_workspace_controller.cpp"
    );
    QVERIFY2(!hubPath.isEmpty(), "找不到 HubWindow 源文件");
    QVERIFY2(!coordinatorPath.isEmpty(), "找不到功能编辑协调器源文件");
    QVERIFY2(!workspacePath.isEmpty(), "找不到功能工作区源文件");
    QFile hub(hubPath);
    QFile coordinator(coordinatorPath);
    QFile workspace(workspacePath);
    QVERIFY(hub.open(QIODevice::ReadOnly));
    QVERIFY(coordinator.open(QIODevice::ReadOnly));
    QVERIFY(workspace.open(QIODevice::ReadOnly));
    const QByteArray hubContents = hub.readAll();
    const QByteArray coordinatorContents = coordinator.readAll();
    const QByteArray workspaceContents = workspace.readAll();

    QVERIFY(hubContents.contains("FunctionWorkspaceController"));
    QVERIFY(!hubContents.contains("runFunctionEditorCoordinator("));
    QVERIFY(workspaceContents.contains("runFunctionEditorCoordinator("));
    QVERIFY(!hubContents.contains("createFunctionEditorDialogAccess(dependencies)"));
    QVERIFY(coordinatorContents.contains(
        "createFunctionEditorDialogAccess(dependencies)"
    ));
}

QTEST_APPLESS_MAIN(FunctionEditorDialogAccessFactoryTests)

#include "function_editor_dialog_access_factory_tests.moc"
