#include <QtTest>

#include "../../src/ui/function_management_page.h"

#include <QFile>
#include <type_traits>

class FunctionManagementPageHeaderTests : public QObject
{
    Q_OBJECT

  private slots:
    void exposesIndependentPageInterface();
    void hubWindowDoesNotRenderManagementCards();
};

void FunctionManagementPageHeaderTests::exposesIndependentPageInterface()
{
    QVERIFY((std::is_default_constructible<FunctionManagementItem>::value));
    QVERIFY((std::is_default_constructible<FunctionManagementPageAccess>::value));
    QVERIFY((std::is_base_of<QWidget, FunctionManagementPage>::value));
    QVERIFY((std::is_constructible<
             FunctionManagementPage,
             const FunctionManagementPageAccess &,
             QWidget *>::value));
}

void FunctionManagementPageHeaderTests::hubWindowDoesNotRenderManagementCards()
{
    const QString sourcePath = QFINDTESTDATA("../../src/ui/hub_window.cpp");
    QVERIFY2(!sourcePath.isEmpty(), "找不到 HubWindow 源文件");
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();
    QVERIFY(!contents.contains("QWidget *functionSummaryCard("));
    QVERIFY(!contents.contains("m_hubCustomListLayout"));
    QVERIFY(contents.contains("FunctionWorkspaceController"));
    QVERIFY(!contents.contains("functionWorkspaceController()->managementPage()"));
    QVERIFY(!contents.contains("FunctionManagementPage *m_functionManagementPage"));
}

QTEST_APPLESS_MAIN(FunctionManagementPageHeaderTests)

#include "function_management_page_header_tests.moc"
