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

bool FunctionCommandPage::setFunctionId(const QString &id)
{
    m_functionId = id.trimmed();
    refresh();
    return true;
}

void FunctionCommandPage::refresh()
{
    setProperty("refreshCount", property("refreshCount").toInt() + 1);
}

void FunctionCommandPage::refreshCanvasState()
{
    setProperty(
        "canvasRefreshCount",
        property("canvasRefreshCount").toInt() + 1
    );
}

bool FunctionCommandPage::flushPendingFlowDraft()
{
    setProperty(
        "flushCount",
        property("flushCount").toInt() + 1
    );
    return !property("flushBlocked").toBool();
}

void FunctionCommandPage::discardPendingFlowDraft()
{
    setProperty("discarded", true);
}

bool FunctionCommandPage::applyFunctionFlowRuntimeEvent(
    const FunctionFlowNodeExecutionEvent &event)
{
    setProperty("runtimeEventFunctionId", event.functionId);
    return true;
}

bool FunctionCommandPage::applyFunctionFlowRunEvent(
    const FunctionFlowRunExecutionEvent &event)
{
    setProperty("runEventFunctionId", event.functionId);
    return true;
}

FunctionCanvasEditor *FunctionCommandPage::canvasEditor() const
{
    return nullptr;
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

HubSettingsState createSettings(AppSettingsData *data)
{
    HubWindowAccess access;
    access.settingsSnapshotProvider = [data]() {
        return data ? *data : AppSettingsData();
    };
    return HubSettingsState(access);
}

FunctionWorkspaceControllerAccess createAccess(
    HubSettingsState *settings,
    QStringList *actions,
    AppSettingsData *persisted
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
    access.flows.addCustomFunction = [actions, persisted](
        const FunctionSettings &function,
        OperationError *
    ) {
        if (!persisted) {
            return false;
        }
        persisted->functions.append(function);
        persisted->functionOrder.append(function.id);
        if (actions) {
            actions->append(QStringLiteral("add"));
        }
        return true;
    };
    return access;
}

} // namespace

class FunctionWorkspaceControllerTests : public QObject
{
    Q_OBJECT

private slots:
    void ownsPagesAndCurrentFunction();
    void editsBySelectingInlineFunctionPage();
    void createsAndSelectsCustomFunction();
    void failedFlushPreventsSwitchingFunctions();
    void handlesMissingDependencies();
    void hubWindowDelegatesFunctionWorkspace();
};

void FunctionWorkspaceControllerTests::ownsPagesAndCurrentFunction()
{
    QWidget parent;
    AppSettingsData data;
    HubSettingsState settings = createSettings(&data);
    FunctionWorkspaceController controller(
        &parent,
        createAccess(&settings, nullptr, &data)
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

void FunctionWorkspaceControllerTests::editsBySelectingInlineFunctionPage()
{
    QWidget parent;
    AppSettingsData data;
    data.functions.append(builtInFunction());
    HubSettingsState settings = createSettings(&data);
    QStringList actions;
    FunctionWorkspaceController controller(
        &parent,
        createAccess(&settings, &actions, &data)
    );
    FunctionManagementPage *management = controller.managementPageWidget();
    const int refreshBefore = management->property("refreshCount").toInt();

    QVERIFY(controller.editFunction(
        QStringLiteral("dictate"),
        QString::fromUtf8("听写"),
        false,
        CustomFunctionDef()
    ));

    QCOMPARE(controller.currentFunctionId(), QStringLiteral("dictate"));
    QVERIFY(actions.isEmpty());
    QCOMPARE(
        management->property("refreshCount").toInt(),
        refreshBefore
    );
}

void FunctionWorkspaceControllerTests::createsAndSelectsCustomFunction()
{
    QWidget parent;
    AppSettingsData data;
    HubSettingsState settings = createSettings(&data);
    QStringList actions;
    FunctionWorkspaceController controller(
        &parent,
        createAccess(&settings, &actions, &data)
    );

    QVERIFY(controller.addCustomFunction());
    QCOMPARE(settings.customFunctions().size(), 1);
    QCOMPARE(settings.customFunctions().first().id, QStringLiteral("custom_1"));
    QCOMPARE(controller.currentFunctionId(), QStringLiteral("custom_1"));
    QCOMPARE(actions, QStringList() << QStringLiteral("add"));
}

void FunctionWorkspaceControllerTests::
failedFlushPreventsSwitchingFunctions()
{
    QWidget parent;
    AppSettingsData data;
    HubSettingsState settings = createSettings(&data);
    FunctionWorkspaceController controller(
        &parent,
        createAccess(&settings, nullptr, &data)
    );
    QVERIFY(controller.setCurrentFunctionId(QStringLiteral("custom_1")));
    FunctionCommandPage *page = controller.commandPageWidget();
    page->setProperty("flushBlocked", true);
    QVERIFY(!controller.setCurrentFunctionId(
        QStringLiteral("custom_2")
    ));
    QCOMPARE(
        controller.currentFunctionId(),
        QStringLiteral("custom_1")
    );
    QCOMPARE(page->functionId(), QStringLiteral("custom_1"));
    QCOMPARE(page->property("flushCount").toInt(), 1);
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
    QVERIFY(controller.editFunction(
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
    QVERIFY(!contents.contains("FunctionEditorDialog"));
}

QTEST_MAIN(FunctionWorkspaceControllerTests)

#include "function_workspace_controller_tests.moc"
