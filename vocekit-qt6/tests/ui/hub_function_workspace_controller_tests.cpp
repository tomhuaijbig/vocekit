#include <QtTest>

#include "../../src/ui/hub_function_workspace_controller.h"

#include <QFile>

#include <type_traits>

class HubFunctionWorkspaceControllerTests : public QObject
{
    Q_OBJECT

private slots:
    void exposesTypedWorkspaceInterface();
    void hubWindowDelegatesWorkspaceOwnership();
};

void HubFunctionWorkspaceControllerTests::exposesTypedWorkspaceInterface()
{
    QVERIFY((std::is_constructible<
        HubFunctionWorkspaceController,
        const HubFunctionWorkspaceControllerAccess &
    >::value));
}

void HubFunctionWorkspaceControllerTests::hubWindowDelegatesWorkspaceOwnership()
{
    const QString hubPath = QFINDTESTDATA("../../src/ui/hub_window.cpp");
    const QString controllerPath = QFINDTESTDATA(
        "../../src/ui/hub_function_workspace_controller.cpp"
    );
    QVERIFY2(!hubPath.isEmpty(), "HubWindow source file not found");
    QVERIFY2(!controllerPath.isEmpty(), "Function workspace host source not found");

    QFile hubSource(hubPath);
    QVERIFY(hubSource.open(QIODevice::ReadOnly));
    const QByteArray hubContents = hubSource.readAll();

    QFile controllerSource(controllerPath);
    QVERIFY(controllerSource.open(QIODevice::ReadOnly));
    const QByteArray controllerContents = controllerSource.readAll();

    QVERIFY(hubContents.contains("HubFunctionWorkspaceController"));
    QVERIFY(hubContents.contains("functionWorkspaceController()->page()"));
    QVERIFY(hubContents.contains("functionWorkspaceController()->selectFunction("));
    QVERIFY(hubContents.contains("functionWorkspaceController()->addFunction()"));
    QVERIFY(!hubContents.contains("QScopedPointer<FunctionWorkspaceController>"));
    QVERIFY(!hubContents.contains("new FunctionWorkspaceController"));
    QVERIFY(!hubContents.contains("FunctionWorkspaceControllerAccess access"));
    QVERIFY(!hubContents.contains("runFunctionEditorDialog"));

    QVERIFY(controllerContents.contains("new FunctionWorkspaceController"));
    QVERIFY(controllerContents.contains("FunctionWorkspaceControllerAccess access"));
    QVERIFY(controllerContents.contains(
        "access.flows = this->access.flows"
    ));
    QVERIFY(controllerContents.contains(
        "access.functionRenamed = this->access.functionRenamed"
    ));
    QVERIFY(controllerContents.contains(
        "access.functionRemoved = this->access.functionRemoved"
    ));
    QVERIFY(controllerContents.contains("!m_impl->workspace"));
    QVERIFY(controllerContents.contains("flushAllPendingFlowDrafts"));
    QVERIFY(controllerContents.contains("discardAllPendingFlowDrafts"));
    QVERIFY(hubContents.contains("workspaceAccess.flows = m_functionFlows"));
    QVERIFY(hubContents.contains(
        "m_functionWorkspaceController->refreshActiveCanvas()"
    ));
    QVERIFY(hubContents.contains("canLeaveFunctionPage"));
    QVERIFY(!controllerContents.contains("runFunctionEditorDialog"));
    QVERIFY(!controllerContents.contains("FunctionEditorDialog"));
}

QTEST_MAIN(HubFunctionWorkspaceControllerTests)

#include "hub_function_workspace_controller_tests.moc"
