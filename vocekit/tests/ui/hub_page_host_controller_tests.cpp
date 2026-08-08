#include <QtTest>

#include "../../src/ui/hub_page_host_controller.h"

#include <QFile>
#include <QPointer>
#include <QWidget>

namespace {

HubPageFactory countingFactory(int *count)
{
    return [count]() {
        ++(*count);
        return new QWidget;
    };
}

HubPageHostControllerAccess completeAccess(int *factoryCalls)
{
    HubPageHostControllerAccess access;
    access.homePage = countingFactory(factoryCalls);
    access.functionPage = countingFactory(factoryCalls);
    access.historyPage = countingFactory(factoryCalls);
    access.vocabularyPage = countingFactory(factoryCalls);
    access.ocrPage = countingFactory(factoryCalls);
    access.promptsPage = countingFactory(factoryCalls);
    access.diagnosticsPage = countingFactory(factoryCalls);
    access.logsPage = countingFactory(factoryCalls);
    access.settingsPage = countingFactory(factoryCalls);
    access.faqPage = countingFactory(factoryCalls);
    return access;
}

} // namespace

class HubPageHostControllerTests : public QObject
{
    Q_OBJECT

private slots:
    void createsAndRetainsOnePageRouter();
    void ownsActivationRefreshAssembly();
    void tracksRouterLifetimeWithoutRecreatingPages();
    void hubWindowDelegatesPageHosting();
};

void HubPageHostControllerTests::createsAndRetainsOnePageRouter()
{
    int factoryCalls = 0;
    QWidget parent;
    HubPageHostController controller(
        completeAccess(&factoryCalls),
        &parent
    );

    HubPageRouter *router = controller.router();
    QVERIFY(router);
    QCOMPARE(router->parentWidget(), &parent);
    QCOMPARE(router->count(), 10);
    QCOMPARE(factoryCalls, 1);
    QVERIFY(router->selectPage(QStringLiteral("history")));
    QCOMPARE(factoryCalls, 2);
    QVERIFY(router->selectPage(QStringLiteral("history")));
    QCOMPARE(factoryCalls, 2);
    QCOMPARE(controller.router(), router);
    QCOMPARE(factoryCalls, 2);
}

void HubPageHostControllerTests::ownsActivationRefreshAssembly()
{
    int factoryCalls = 0;
    int historyRefreshes = 0;
    int vocabularyRefreshes = 0;
    int settingsRefreshes = 0;
    QObject deferredContext;

    HubPageHostControllerAccess access = completeAccess(&factoryCalls);
    access.deferredContext = &deferredContext;
    access.refreshHistory = [&historyRefreshes]() {
        ++historyRefreshes;
    };
    access.refreshVocabulary = [&vocabularyRefreshes]() {
        ++vocabularyRefreshes;
    };
    access.refreshSettings = [&settingsRefreshes]() {
        ++settingsRefreshes;
    };

    HubPageHostController controller(access);
    HubPageRouter *router = controller.router();
    QVERIFY(router->selectPage(QStringLiteral("history")));
    QCOMPARE(historyRefreshes, 0);
    QTRY_COMPARE(historyRefreshes, 1);

    QVERIFY(router->selectPage(QStringLiteral("vocabulary")));
    QCOMPARE(vocabularyRefreshes, 1);

    QVERIFY(router->selectPage(QStringLiteral("settings")));
    QVERIFY(router->selectPage(QStringLiteral("settings")));
    QCOMPARE(settingsRefreshes, 1);
}

void HubPageHostControllerTests::tracksRouterLifetimeWithoutRecreatingPages()
{
    int factoryCalls = 0;
    QPointer<HubPageRouter> router;
    {
        QWidget parent;
        HubPageHostController controller(
            completeAccess(&factoryCalls),
            &parent
        );
        router = controller.router();
        QVERIFY(router);
        QVERIFY(router->selectPage(QStringLiteral("faq")));
        QVERIFY(router->selectPage(QStringLiteral("faq")));
        QCOMPARE(factoryCalls, 2);
    }

    QVERIFY(router.isNull());
    QCOMPARE(factoryCalls, 2);
}

void HubPageHostControllerTests::hubWindowDelegatesPageHosting()
{
    const QString sourcePath = QFINDTESTDATA("../../src/ui/hub_window.cpp");
    QVERIFY2(!sourcePath.isEmpty(), "HubWindow source file not found");
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();

    QVERIFY(contents.contains("HubPageHostController"));
    QVERIFY(contents.contains("pageHostController()->router()"));
    QVERIFY(!contents.contains("HubPageComposition::create"));
    QVERIFY(!contents.contains("createHubPageCompositionAccess"));
    QVERIFY(!contents.contains("HubPageRouter *pagesWidget()"));
}

QTEST_MAIN(HubPageHostControllerTests)

#include "hub_page_host_controller_tests.moc"
