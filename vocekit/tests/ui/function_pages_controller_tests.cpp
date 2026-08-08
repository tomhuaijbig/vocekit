#include <QtTest>

#include "../../src/ui/function_pages_controller.h"

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
    const QString normalized = id.trimmed();
    const QString rejected =
        property("rejectedFunctionId").toString();
    if (!rejected.isEmpty() && rejected == normalized) {
        return false;
    }
    m_functionId = normalized;
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

class FunctionPagesControllerTests : public QObject
{
    Q_OBJECT

private slots:
    void createsAndCachesBothPagesFromOneAssembly();
    void appliesFunctionSelectedBeforePageCreation();
    void updatesAndClearsCurrentFunction();
    void rejectedPageSwitchKeepsControllerAndPageIdsAligned();
    void refreshesOnlyCreatedPages();
    void refreshesCanvasStateWithoutRebuildingThePage();
    void forwardsRuntimeEventsOnlyToCreatedMatchingPage();
    void handlesMissingAccessProvider();
    void hubWindowDelegatesPageOwnership();
};

void FunctionPagesControllerTests::createsAndCachesBothPagesFromOneAssembly()
{
    QWidget parent;
    int accessLoads = 0;
    FunctionPagesControllerAccess access;
    access.accessProvider = [&accessLoads]() {
        ++accessLoads;
        return FunctionPagesAccessAssembly();
    };
    FunctionPagesController controller(&parent, access);

    QWidget *command = controller.commandPage();
    QWidget *management = controller.managementPage();

    QVERIFY(command);
    QVERIFY(management);
    QCOMPARE(controller.commandPage(), command);
    QCOMPARE(controller.managementPage(), management);
    QCOMPARE(accessLoads, 1);
    QVERIFY(controller.commandPageCreated());
    QVERIFY(controller.managementPageCreated());
}

void FunctionPagesControllerTests::appliesFunctionSelectedBeforePageCreation()
{
    QWidget parent;
    FunctionPagesController controller(
        &parent,
        FunctionPagesControllerAccess()
    );

    QVERIFY(controller.setCurrentFunctionId(QStringLiteral("  translate  ")));
    QVERIFY(!controller.commandPageCreated());

    FunctionCommandPage *page = controller.commandPageWidget();
    QVERIFY(page);
    QCOMPARE(page->functionId(), QStringLiteral("translate"));
    QCOMPARE(controller.currentFunctionId(), QStringLiteral("translate"));
}

void FunctionPagesControllerTests::updatesAndClearsCurrentFunction()
{
    QWidget parent;
    FunctionPagesController controller(
        &parent,
        FunctionPagesControllerAccess()
    );
    FunctionCommandPage *page = controller.commandPageWidget();

    QVERIFY(controller.setCurrentFunctionId(QStringLiteral("dictate")));
    QCOMPARE(page->functionId(), QStringLiteral("dictate"));
    QVERIFY(!controller.setCurrentFunctionId(QStringLiteral("  ")));
    QCOMPARE(page->functionId(), QStringLiteral("dictate"));

    controller.clearCurrentFunction();
    QVERIFY(controller.currentFunctionId().isEmpty());
    QVERIFY(page->functionId().isEmpty());
}

void FunctionPagesControllerTests::
rejectedPageSwitchKeepsControllerAndPageIdsAligned()
{
    QWidget parent;
    FunctionPagesController controller(
        &parent,
        FunctionPagesControllerAccess()
    );
    FunctionCommandPage *page = controller.commandPageWidget();

    QVERIFY(controller.setCurrentFunctionId(QStringLiteral("ask")));
    page->setProperty(
        "rejectedFunctionId",
        QStringLiteral("translate")
    );

    QVERIFY(!controller.setCurrentFunctionId(
        QStringLiteral("translate")
    ));
    QCOMPARE(
        controller.currentFunctionId(),
        QStringLiteral("ask")
    );
    QCOMPARE(page->functionId(), QStringLiteral("ask"));
}

void FunctionPagesControllerTests::refreshesOnlyCreatedPages()
{
    QWidget parent;
    FunctionPagesController controller(
        &parent,
        FunctionPagesControllerAccess()
    );

    controller.refreshCommandPage();
    controller.refreshManagementPage();
    QVERIFY(!controller.commandPageCreated());
    QVERIFY(!controller.managementPageCreated());

    FunctionCommandPage *command = controller.commandPageWidget();
    FunctionManagementPage *management = controller.managementPageWidget();
    const int commandBefore = command->property("refreshCount").toInt();
    const int managementBefore = management->property("refreshCount").toInt();

    controller.refreshCommandPage();
    controller.refreshManagementPage();

    QCOMPARE(command->property("refreshCount").toInt(), commandBefore + 1);
    QCOMPARE(
        management->property("refreshCount").toInt(),
        managementBefore + 1
    );
}

void FunctionPagesControllerTests::
refreshesCanvasStateWithoutRebuildingThePage()
{
    QWidget parent;
    FunctionPagesController controller(
        &parent,
        FunctionPagesControllerAccess()
    );

    controller.refreshCanvasState();
    QVERIFY(!controller.commandPageCreated());
    FunctionCommandPage *page = controller.commandPageWidget();
    const int fullBefore =
        page->property("refreshCount").toInt();
    const int canvasBefore =
        page->property("canvasRefreshCount").toInt();

    controller.refreshCanvasState();

    QCOMPARE(
        page->property("refreshCount").toInt(),
        fullBefore
    );
    QCOMPARE(
        page->property("canvasRefreshCount").toInt(),
        canvasBefore + 1
    );
}

void FunctionPagesControllerTests::forwardsRuntimeEventsOnlyToCreatedMatchingPage()
{
    QWidget parent;
    FunctionPagesController controller(
        &parent,
        FunctionPagesControllerAccess()
    );
    FunctionFlowNodeExecutionEvent nodeEvent;
    nodeEvent.functionId = QStringLiteral("translate");
    FunctionFlowRunExecutionEvent runEvent;
    runEvent.functionId = QStringLiteral("translate");

    QVERIFY(!controller.applyFunctionFlowRuntimeEvent(nodeEvent));
    QVERIFY(!controller.applyFunctionFlowRunEvent(runEvent));
    QVERIFY(!controller.commandPageCreated());

    FunctionCommandPage *page = controller.commandPageWidget();
    QVERIFY(controller.setCurrentFunctionId(QStringLiteral("ask")));
    QVERIFY(!controller.applyFunctionFlowRuntimeEvent(nodeEvent));
    QVERIFY(!controller.applyFunctionFlowRunEvent(runEvent));
    QVERIFY(!page->property("runtimeEventFunctionId").isValid());
    QVERIFY(!page->property("runEventFunctionId").isValid());

    QVERIFY(controller.setCurrentFunctionId(QStringLiteral("translate")));
    QVERIFY(controller.applyFunctionFlowRuntimeEvent(nodeEvent));
    QVERIFY(controller.applyFunctionFlowRunEvent(runEvent));
    QCOMPARE(
        page->property("runtimeEventFunctionId").toString(),
        QStringLiteral("translate")
    );
    QCOMPARE(
        page->property("runEventFunctionId").toString(),
        QStringLiteral("translate")
    );
}

void FunctionPagesControllerTests::handlesMissingAccessProvider()
{
    QWidget parent;
    FunctionPagesController controller(
        &parent,
        FunctionPagesControllerAccess()
    );

    QVERIFY(controller.commandPage());
    QVERIFY(controller.managementPage());
    QVERIFY(controller.setCurrentFunctionId(QStringLiteral("ask")));
    QCOMPARE(controller.commandPageWidget()->functionId(), QStringLiteral("ask"));
}

void FunctionPagesControllerTests::hubWindowDelegatesPageOwnership()
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

    QVERIFY(contents.contains("HubFunctionWorkspaceController"));
    QVERIFY(contents.contains("functionWorkspaceController()->page()"));
    QVERIFY(!contents.contains("functionWorkspaceController()->managementPage()"));
    QVERIFY(workspaceContents.contains("new FunctionPagesController("));
    QVERIFY(!contents.contains("FunctionCommandPage *m_functionCommandPage"));
    QVERIFY(!contents.contains("FunctionManagementPage *m_functionManagementPage"));
    QVERIFY(!contents.contains("QString m_currentCommandFunctionId"));
}

QTEST_MAIN(FunctionPagesControllerTests)

#include "function_pages_controller_tests.moc"
