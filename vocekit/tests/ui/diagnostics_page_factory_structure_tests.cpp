#include <QtTest>

#include <QFile>

class DiagnosticsPageFactoryStructureTests : public QObject
{
    Q_OBJECT

private slots:
    void factoryOwnsDefaultCardAssembly();
    void utilityControllerUsesDiagnosticsFactory();
};

void DiagnosticsPageFactoryStructureTests::factoryOwnsDefaultCardAssembly()
{
    const QString sourcePath = QFINDTESTDATA(
        "../../src/ui/diagnostics_page_factory.cpp"
    );
    QVERIFY2(!sourcePath.isEmpty(), "找不到诊断页工厂源文件");
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();

    QVERIFY(contents.contains("DiagnosticsPanelDefaultCards cards;"));
    QVERIFY(contents.contains("buildVocabularyDiagnosticRequest"));
    QVERIFY(contents.contains("new ResultChoicePopup"));
    QVERIFY(contents.contains("addDefaultCards(cards)"));
}

void DiagnosticsPageFactoryStructureTests::utilityControllerUsesDiagnosticsFactory()
{
    const QString sourcePath = QFINDTESTDATA(
        "../../src/ui/hub_utility_pages_controller.cpp"
    );
    QVERIFY2(!sourcePath.isEmpty(), "找不到辅助页面控制器源文件");
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();

    QVERIFY(contents.contains("createDiagnosticsPanel(dependencies)"));
    QVERIFY(!contents.contains("DiagnosticsPanelDefaultCards cards;"));
    QVERIFY(!contents.contains("new VocabularyTestCard"));
    QVERIFY(!contents.contains("new ResultPopupTestCard"));
    QVERIFY(!contents.contains("new ResultChoicePopup"));
}

QTEST_APPLESS_MAIN(DiagnosticsPageFactoryStructureTests)

#include "diagnostics_page_factory_structure_tests.moc"
