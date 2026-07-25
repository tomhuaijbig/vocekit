#include <QtTest>

#include "../../src/ui/hub_home_page_controller.h"

#include <QFile>

#include <type_traits>

class HubHomePageControllerTests : public QObject
{
    Q_OBJECT

private slots:
    void exposesTypedLifecycleInterface();
    void hubWindowDelegatesHomePageOwnership();
};

void HubHomePageControllerTests::exposesTypedLifecycleInterface()
{
    QVERIFY((std::is_constructible<
        HubHomePageController,
        const HomePageAccess &,
        QWidget *
    >::value));
}

void HubHomePageControllerTests::hubWindowDelegatesHomePageOwnership()
{
    const QString hubPath = QFINDTESTDATA("../../src/ui/hub_window.cpp");
    const QString controllerPath = QFINDTESTDATA(
        "../../src/ui/hub_home_page_controller.cpp"
    );
    QVERIFY2(!hubPath.isEmpty(), "HubWindow source file not found");
    QVERIFY2(!controllerPath.isEmpty(), "Home page controller source not found");

    QFile hubSource(hubPath);
    QVERIFY(hubSource.open(QIODevice::ReadOnly));
    const QByteArray hubContents = hubSource.readAll();

    QFile controllerSource(controllerPath);
    QVERIFY(controllerSource.open(QIODevice::ReadOnly));
    const QByteArray controllerContents = controllerSource.readAll();

    QVERIFY(hubContents.contains("HubHomePageController"));
    QVERIFY(hubContents.contains("homePageController()->page()"));
    QVERIFY(hubContents.contains("homePageController()->refreshFunctionModes()"));
    QVERIFY(hubContents.contains("homePageController()->refreshRecentHistory()"));
    QVERIFY(hubContents.contains("homePageController()->refreshCurrentStatus()"));
    QVERIFY(!hubContents.contains("HomePage *m_homePage"));
    QVERIFY(!hubContents.contains("new HomePage"));
    QVERIFY(!hubContents.contains("m_homePage->"));

    QVERIFY(controllerContents.contains("new HomePage"));
    QVERIFY(controllerContents.contains("QPointer<HomePage>"));
}

QTEST_MAIN(HubHomePageControllerTests)

#include "hub_home_page_controller_tests.moc"
