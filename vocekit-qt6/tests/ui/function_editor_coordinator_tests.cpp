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
    void buildsBuiltInSummary();
    void usesCustomShortcutAndLibraryPromptTitle();
    void handlesMissingSettings();
    void workspaceContainsNoDialogFlow();
};

void FunctionEditorCoordinatorTests::buildsBuiltInSummary()
{
    AppSettingsData data;
    data.functions.append(builtInFunction());
    HubSettingsState settings = createSettings(data);

    FunctionEditorCoordinatorActions actions;
    actions.settings = &settings;
    actions.prompts = promptAccess(data);

    const QString summary = functionEditorSummaryText(
        QStringLiteral("dictate"),
        QStringLiteral("Alt+X"),
        actions
    );
    QVERIFY(summary.startsWith(QStringLiteral("Alt + X")));
    QVERIFY(summary.contains(QString::fromUtf8("输入：语音")));
    QVERIFY(summary.contains(QString::fromUtf8("提示词：听写提示词")));
}

void FunctionEditorCoordinatorTests::usesCustomShortcutAndLibraryPromptTitle()
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
    HubSettingsState settings = createSettings(
        data,
        QVector<PromptLibraryItem>() << libraryPrompt
    );

    FunctionEditorCoordinatorActions actions;
    actions.settings = &settings;
    actions.prompts.snapshotProvider = [data, libraryPrompt]() {
        PromptRuntimeSnapshot snapshot;
        snapshot.settings = data;
        snapshot.libraryItems.append(libraryPrompt);
        return snapshot;
    };

    const QString summary = functionEditorSummaryText(
        custom.id,
        QStringLiteral("Shift+F2"),
        actions
    );
    QVERIFY(summary.startsWith(QStringLiteral("Shift + F2")));
    QVERIFY(summary.contains(QString::fromUtf8("提示词：正式表达")));
}

void FunctionEditorCoordinatorTests::handlesMissingSettings()
{
    QVERIFY(functionEditorSummaryText(
        QStringLiteral("dictate"),
        QStringLiteral("Alt+X"),
        FunctionEditorCoordinatorActions()
    ).isEmpty());
}

void FunctionEditorCoordinatorTests::workspaceContainsNoDialogFlow()
{
    const QString path = QFINDTESTDATA(
        "../../src/ui/function_workspace_controller.cpp"
    );
    QVERIFY2(!path.isEmpty(), "找不到功能工作区源文件");
    QFile source(path);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();

    QVERIFY(contents.contains("functionEditorSummaryText("));
    QVERIFY(!contents.contains("runFunctionEditorCoordinator("));
    QVERIFY(!contents.contains("FunctionEditorDialog"));
}

QTEST_MAIN(FunctionEditorCoordinatorTests)
#include "function_editor_coordinator_tests.moc"
