#include <QtTest>

#include "../../src/ui/function_workspace_controller.h"
#include "../../src/ui/hub_settings_state.h"

#include <QFile>
#include <QWidget>

FunctionCommandPage::FunctionCommandPage(
    const FunctionCommandPageAccess &access,
    QWidget *parent
)
    : QWidget(parent), m_access(access)
{
    setProperty("refreshCount", 0);
}

QString FunctionCommandPage::functionId() const
{
    return m_functionId;
}

void FunctionCommandPage::setFunctionId(const QString &id)
{
    m_functionId = id.trimmed();
    refresh();
}

void FunctionCommandPage::refresh()
{
    setProperty("refreshCount", property("refreshCount").toInt() + 1);
}

FunctionManagementPage::FunctionManagementPage(
    const FunctionManagementPageAccess &access,
    QWidget *parent
)
    : QWidget(parent), m_access(access)
{
    setProperty("refreshCount", 0);
}

void FunctionManagementPage::refresh()
{
    setProperty("refreshCount", property("refreshCount").toInt() + 1);
}

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
    return function;
}

HubSettingsState createSettings(const AppSettingsData &data = AppSettingsData())
{
    HubWindowAccess access;
    access.settingsSnapshotProvider = [data]() { return data; };
    return HubSettingsState(access);
}

FunctionWorkspaceControllerAccess createAccess(
    HubSettingsState *settings,
    QStringList *actions,
    FunctionEditorDialogRequest *request,
    bool dialogResult
)
{
    FunctionWorkspaceControllerAccess access;
    access.settings = settings;
    access.prompts.snapshotProvider = []() {
        return PromptRuntimeSnapshot();
    };
    access.saveSettings = [actions]() {
        if (actions) {
            actions->append(QStringLiteral("save"));
        }
    };
    access.openEditorDialog = [request, dialogResult](
        const FunctionEditorDialogRequest &dialogRequest,
        const FunctionEditorDialogAccess &dialogAccess
    ) {
        if (request) {
            *request = dialogRequest;
        }
        if (dialogResult) {
            dialogAccess.saveSettings();
        }
        return dialogResult;
    };
    return access;
}

} // namespace

class FunctionWorkspaceControllerTests : public QObject
{
    Q_OBJECT

private slots:
    void ownsPagesAndCurrentFunction();
    void editsThroughOneWorkspaceInterface();
    void rollsBackCancelledCustomFunction();
    void handlesMissingDependencies();
    void hubWindowDelegatesFunctionWorkspace();
};

void FunctionWorkspaceControllerTests::ownsPagesAndCurrentFunction()
{
    QWidget parent;
    HubSettingsState settings = createSettings();
    FunctionWorkspaceController controller(
        &parent,
        createAccess(&settings, nullptr, nullptr, true)
    );

    QVERIFY(controller.setCurrentFunctionId(QStringLiteral(" translate ")));
    QVERIFY(!controller.commandPageCreated());
    FunctionCommandPage *command = controller.commandPageWidget();
    FunctionManagementPage *management = controller.managementPageWidget();

    QVERIFY(command);
    QVERIFY(management);
    QCOMPARE(command->functionId(), QStringLiteral("translate"));
    QCOMPARE(controller.commandPageWidget(), command);
    QCOMPARE(controller.managementPageWidget(), management);
    QCOMPARE(controller.currentFunctionId(), QStringLiteral("translate"));

    controller.clearCurrentFunction();
    QVERIFY(controller.currentFunctionId().isEmpty());
    QVERIFY(command->functionId().isEmpty());
}

void FunctionWorkspaceControllerTests::editsThroughOneWorkspaceInterface()
{
    QWidget parent;
    AppSettingsData data;
    data.functions.append(builtInFunction());
    HubSettingsState settings = createSettings(data);
    QStringList actions;
    FunctionEditorDialogRequest request;
    FunctionWorkspaceController controller(
        &parent,
        createAccess(&settings, &actions, &request, true)
    );
    FunctionManagementPage *management = controller.managementPageWidget();
    const int refreshBefore = management->property("refreshCount").toInt();

    QVERIFY(controller.editFunction(
        QStringLiteral("dictate"),
        QString::fromUtf8("听写"),
        false,
        CustomFunctionDef()
    ));

    QCOMPARE(request.id, QStringLiteral("dictate"));
    QVERIFY(request.summaryText.contains(QString::fromUtf8("输入：语音")));
    QCOMPARE(actions, QStringList() << QStringLiteral("save"));
    QCOMPARE(
        management->property("refreshCount").toInt(),
        refreshBefore
    );
}

void FunctionWorkspaceControllerTests::rollsBackCancelledCustomFunction()
{
    QWidget parent;
    HubSettingsState settings = createSettings();
    QStringList actions;
    FunctionEditorDialogRequest request;
    FunctionWorkspaceController controller(
        &parent,
        createAccess(&settings, &actions, &request, false)
    );

    QVERIFY(!controller.addCustomFunction());
    QVERIFY(settings.customFunctions().isEmpty());
    QCOMPARE(request.id, QStringLiteral("custom_1"));
    QCOMPARE(actions.count(QStringLiteral("save")), 2);
    QCOMPARE(actions.count(QStringLiteral("modes")), 0);
    QCOMPARE(actions.count(QStringLiteral("prompts")), 0);
    QCOMPARE(actions.count(QStringLiteral("navigation")), 0);
}

void FunctionWorkspaceControllerTests::handlesMissingDependencies()
{
    QWidget parent;
    FunctionWorkspaceController controller(
        &parent,
        FunctionWorkspaceControllerAccess()
    );

    QVERIFY(controller.commandPage());
    QVERIFY(controller.managementPage());
    QVERIFY(!controller.addCustomFunction());
    QVERIFY(!controller.editFunction(
        QStringLiteral("dictate"),
        QString::fromUtf8("听写"),
        false,
        CustomFunctionDef()
    ));
    QVERIFY(controller.summaryText(
        QStringLiteral("dictate"),
        QStringLiteral("Alt+X")
    ).isEmpty());
}

void FunctionWorkspaceControllerTests::hubWindowDelegatesFunctionWorkspace()
{
    const QString sourcePath = QFINDTESTDATA("../../src/ui/hub_window.cpp");
    QVERIFY2(!sourcePath.isEmpty(), "Cannot find HubWindow source file");
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();

    QVERIFY(contents.contains("HubFunctionWorkspaceController"));
    QVERIFY(contents.contains("functionWorkspaceController()->page()"));
    QVERIFY(!contents.contains("functionWorkspaceController()->managementPage()"));
    QVERIFY(!contents.contains("FunctionPagesAccessDependencies"));
    QVERIFY(!contents.contains("CustomFunctionCreationActions"));
    QVERIFY(!contents.contains("FunctionEditorCoordinatorActions"));
    QVERIFY(!contents.contains("runFunctionEditorCoordinator("));
    QVERIFY(!contents.contains("createAndEditCustomFunction("));
}

QTEST_MAIN(FunctionWorkspaceControllerTests)

#include "function_workspace_controller_tests.moc"
