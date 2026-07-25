#include <QtTest>

#include "../../src/ui/function_command_page.h"

#include <QFile>
#include <type_traits>

class FunctionCommandPageHeaderTests : public QObject
{
    Q_OBJECT

  private slots:
    void exposesIndependentPageInterface();
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
