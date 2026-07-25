#include <QtTest>

#include "../../src/ui/home_page.h"

#include <QFile>
#include <type_traits>

class HomePageHeaderTests : public QObject
{
    Q_OBJECT

private slots:
    void exposesIndependentPageInterface();
    void hubWindowOnlyWiresHomePage();
};

void HomePageHeaderTests::exposesIndependentPageInterface()
{
    QVERIFY((std::is_default_constructible<HomePageAccess>::value));
    QVERIFY((std::is_base_of<QWidget, HomePage>::value));
    QVERIFY((std::is_constructible<
             HomePage,
             const HomePageAccess &,
             QWidget *>::value));
}

void HomePageHeaderTests::hubWindowOnlyWiresHomePage()
{
    const QString sourcePath = QFINDTESTDATA("../../src/ui/hub_window.cpp");
    QVERIFY2(!sourcePath.isEmpty(), "找不到 HubWindow 源文件");
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();
    QVERIFY(contents.contains("HomePage"));
    QVERIFY(!contents.contains("QWidget *content()"));
    QVERIFY(!contents.contains("QWidget *historyPanel()"));
    QVERIFY(!contents.contains("QWidget *statusPanel()"));
    QVERIFY(!contents.contains("m_modeGrid"));
    QVERIFY(!contents.contains("m_recentHistoryPanel"));
    QVERIFY(!contents.contains("m_statusPanel"));
}

QTEST_APPLESS_MAIN(HomePageHeaderTests)

#include "home_page_header_tests.moc"
