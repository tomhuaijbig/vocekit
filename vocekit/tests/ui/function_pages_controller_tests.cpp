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

class FunctionPagesControllerTests : public QObject
{
    Q_OBJECT

private slots:
    void createsAndCachesBothPagesFromOneAssembly();
    void appliesFunctionSelectedBeforePageCreation();
    void updatesAndClearsCurrentFunction();
    void refreshesOnlyCreatedPages();
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
