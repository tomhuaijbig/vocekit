#include <QtTest>

#include "../../src/ui/function_command_page.h"

#include <QFile>
#include <type_traits>

class FunctionCommandPageHeaderTests : public QObject
{
    Q_OBJECT

  private slots:
    void exposesIndependentPageInterface();
    void exposesOnlyNarrowCanvasLifecycleOperations();
    void canvasModeUsesFixedWorkspaceInsteadOfPageScrolling();
    void hubWindowDoesNotRenderFunctionSettingsPage();
};

void FunctionCommandPageHeaderTests::exposesIndependentPageInterface()
{
    QVERIFY((std::is_base_of<QWidget, FunctionCommandPage>::value));
    QVERIFY((std::is_constructible<FunctionCommandPage, const FunctionCommandPageAccess &,
                                   QWidget *>::value));
    QVERIFY(
        (std::is_same<decltype(std::declval<FunctionCommandPage>().functionId()), QString>::value));
}

void FunctionCommandPageHeaderTests::exposesOnlyNarrowCanvasLifecycleOperations()
{
    QVERIFY((std::is_same<
        decltype(std::declval<FunctionCommandPage>()
                     .flushPendingFlowDraft()),
        bool
    >::value));
    QVERIFY((std::is_same<
        decltype(std::declval<FunctionCommandPage>()
                     .canvasEditor()),
        FunctionCanvasEditor *
    >::value));

    const QString sourcePath = QFINDTESTDATA(
        "../../src/ui/function_command_page.cpp"
    );
    QVERIFY2(!sourcePath.isEmpty(), "Cannot find FunctionCommandPage source");
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();

    QVERIFY(contents.contains("flushAllPendingSaves"));
    QVERIFY(contents.contains("discardPendingSaves"));
    QVERIFY(contents.contains("FunctionCanvasEditor"));
    QVERIFY(!contents.contains("FunctionFlowPublicationService"));
    QVERIFY(!contents.contains("function_flow_json"));
    QVERIFY(!contents.contains("FunctionCanvasNodeItem"));
    QVERIFY(!contents.contains("FunctionCanvasEdgeItem"));
}

void FunctionCommandPageHeaderTests::
canvasModeUsesFixedWorkspaceInsteadOfPageScrolling()
{
    const QString sourcePath = QFINDTESTDATA(
        "../../src/ui/function_command_page.cpp"
    );
    QVERIFY2(!sourcePath.isEmpty(), "Cannot find FunctionCommandPage source");
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();

    QVERIFY(contents.contains(
        "m_pageStack->setCurrentWidget(m_canvasHost)"
    ));
    QVERIFY(contents.contains(
        "m_pageStack->setCurrentWidget(m_scroll)"
    ));
    QVERIFY(contents.contains("m_contentLayout->addWidget(editor, 1)"));
}

void FunctionCommandPageHeaderTests::hubWindowDoesNotRenderFunctionSettingsPage()
{
    const QString sourcePath = QFINDTESTDATA("../../src/ui/hub_window.cpp");
    QVERIFY2(!sourcePath.isEmpty(), "找不到 HubWindow 源文件");
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();
    QVERIFY(!contents.contains("QWidget *commandAccordionCard("));
    QVERIFY(!contents.contains("QWidget *commandControlSection("));
    QVERIFY(!contents.contains("void refreshCommandFunctionPage()"));
    QVERIFY(contents.contains("HubFunctionWorkspaceController"));
    QVERIFY(contents.contains("functionWorkspaceController()->page()"));
    QVERIFY(!contents.contains("FunctionCommandPage *m_functionCommandPage"));
}

QTEST_APPLESS_MAIN(FunctionCommandPageHeaderTests)

#include "function_command_page_header_tests.moc"
