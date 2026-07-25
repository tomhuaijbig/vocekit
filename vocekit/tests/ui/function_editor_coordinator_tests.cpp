#include <QtTest>

#include "../../src/ui/function_editor_coordinator.h"
#include "../../src/ui/hub_settings_state.h"

#include <QFile>

namespace {

FunctionSettings builtInFunction()
{
    FunctionSettings function;
    function.id = QStringLiteral("dictate");
    function.name = QString::fromUtf8("听写");
    function.builtIn = true;
    function.shortcut = QStringLiteral("Alt+X");
    function.modelId = QStringLiteral("deepseek-v4-flash");
    function.promptId = QStringLiteral("dictate");
    function.input.useVoice = true;
    function.output.outputMode = QStringLiteral("auto_write");
    function.output.resultTemplate = QStringLiteral("simple");
    function.output.floatingBarSeconds = 2;
    function.output.resultPopupSeconds = 0;
    function.recording.countdownSeconds = 3;
    function.recording.beepEnabled = true;
    function.recording.triggerMode = QStringLiteral("toggle");
    return function;
}

HubSettingsState createSettings(
    const AppSettingsData &data,
    const QVector<PromptLibraryItem> &library = QVector<PromptLibraryItem>()
)
{
    HubWindowAccess access;
    access.settingsSnapshotProvider = [data]() { return data; };
    access.promptLibraryProvider = [library]() { return library; };
    return HubSettingsState(access);
}

PromptSettingsAccess promptAccess(const AppSettingsData &data)
{
    PromptSettingsAccess access;
    access.snapshotProvider = [data]() {
        PromptRuntimeSnapshot snapshot;
        snapshot.settings = data;
        return snapshot;
    };
    return access;
}

} // namespace

class FunctionEditorCoordinatorTests : public QObject
{
    Q_OBJECT

private slots:
    void buildsBuiltInRequestAndSavesOnce();
    void usesCustomFunctionShortcutAndLibraryPromptTitle();
    void handlesMissingDependencies();
    void hubWindowDelegatesToCoordinator();
};

void FunctionEditorCoordinatorTests::buildsBuiltInRequestAndSavesOnce()
{
    AppSettingsData data;
    data.functions.append(builtInFunction());
    HubSettingsState settings = createSettings(data);
    QStringList actions;
    FunctionEditorDialogRequest captured;

    FunctionEditorCoordinatorActions coordinator;
    coordinator.settings = &settings;
    coordinator.prompts = promptAccess(data);
    coordinator.saveSettings = [&actions]() { actions.append(QStringLiteral("save")); };
    coordinator.openDialog = [&captured](
        const FunctionEditorDialogRequest &request,
        const FunctionEditorDialogAccess &access
    ) {
        captured = request;
        access.saveSettings();
        return true;
    };

    QVERIFY(runFunctionEditorCoordinator(
        QStringLiteral("dictate"),
        QString::fromUtf8("听写"),
        false,
        CustomFunctionDef(),
        coordinator
    ));
    QCOMPARE(captured.id, QStringLiteral("dictate"));
    QCOMPARE(captured.title, QString::fromUtf8("听写"));
    QVERIFY(!captured.custom);
    QVERIFY(captured.summaryText.startsWith(QStringLiteral("Alt + X")));
    QVERIFY(captured.summaryText.contains(QString::fromUtf8("输入：语音")));
    QVERIFY(captured.summaryText.contains(QString::fromUtf8("提示词：听写提示词")));
    QCOMPARE(actions, QStringList() << QStringLiteral("save"));
}

void FunctionEditorCoordinatorTests::usesCustomFunctionShortcutAndLibraryPromptTitle()
{
    AppSettingsData data;
    FunctionSettings custom;
    custom.id = QStringLiteral("custom_1");
    custom.name = QString::fromUtf8("润色");
    custom.shortcut = QStringLiteral("Ctrl+Alt+1");
    custom.modelId = QStringLiteral("deepseek-v4-flash");
    custom.promptId = QStringLiteral("prompt_1");
    custom.input.useSelection = true;
    data.functions.append(custom);

    PromptLibraryItem libraryPrompt;
    libraryPrompt.id = QStringLiteral("prompt_1");
    libraryPrompt.name = QString::fromUtf8("正式表达");
    libraryPrompt.content = QString::fromUtf8("使用正式语气。");

    QVector<PromptLibraryItem> library;
    library.append(libraryPrompt);
    HubSettingsState settings = createSettings(data, library);
    FunctionEditorDialogRequest captured;
    FunctionEditorCoordinatorActions coordinator;
    coordinator.settings = &settings;
    coordinator.prompts.snapshotProvider = [data, libraryPrompt]() {
        PromptRuntimeSnapshot snapshot;
        snapshot.settings = data;
        snapshot.libraryItems.append(libraryPrompt);
        return snapshot;
    };
    coordinator.openDialog = [&captured](
        const FunctionEditorDialogRequest &request,
        const FunctionEditorDialogAccess &
    ) {
        captured = request;
        return true;
    };

    CustomFunctionDef edited;
    edited.id = custom.id;
    edited.name = custom.name;
    edited.shortcut = QStringLiteral("Shift+F2");
    QVERIFY(runFunctionEditorCoordinator(
        custom.id,
        custom.name,
        true,
        edited,
        coordinator
    ));
    QVERIFY(captured.summaryText.startsWith(QStringLiteral("Shift + F2")));
    QVERIFY(captured.summaryText.contains(QString::fromUtf8("提示词：正式表达")));
}

void FunctionEditorCoordinatorTests::handlesMissingDependencies()
{
    FunctionEditorCoordinatorActions coordinator;
    QVERIFY(!runFunctionEditorCoordinator(
        QStringLiteral("dictate"),
        QString::fromUtf8("听写"),
        false,
        CustomFunctionDef(),
        coordinator
    ));

    HubSettingsState settings;
    coordinator.settings = &settings;
    QVERIFY(!runFunctionEditorCoordinator(
        QStringLiteral("dictate"),
        QString::fromUtf8("听写"),
        false,
        CustomFunctionDef(),
        coordinator
    ));
}

void FunctionEditorCoordinatorTests::hubWindowDelegatesToCoordinator()
{
    const QString sourcePath = QFINDTESTDATA("../../src/ui/hub_window.cpp");
    const QString workspacePath = QFINDTESTDATA(
        "../../src/ui/function_workspace_controller.cpp"
    );
    QVERIFY2(!sourcePath.isEmpty(), "找不到 HubWindow 源文件");
    QVERIFY2(!workspacePath.isEmpty(), "找不到功能工作区源文件");
    QFile source(sourcePath);
    QFile workspace(workspacePath);
    QVERIFY(source.open(QIODevice::ReadOnly));
    QVERIFY(workspace.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();
    const QByteArray workspaceContents = workspace.readAll();

    QVERIFY(contents.contains("FunctionWorkspaceController"));
    QVERIFY(!contents.contains("runFunctionEditorCoordinator("));
    QVERIFY(workspaceContents.contains("runFunctionEditorCoordinator("));
    QVERIFY(!contents.contains("FunctionEditorDialogRequest request;"));
}

QTEST_MAIN(FunctionEditorCoordinatorTests)
#include "function_editor_coordinator_tests.moc"
