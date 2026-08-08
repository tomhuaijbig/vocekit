#include <QtTest>

#include "../../src/ui/command_center_shell.h"
#include "../../src/ui/hub_navigation_controller.h"
#include "../../src/ui/hub_page_router.h"

#include <QFile>
#include <QWidget>

namespace {

void registerPage(
    HubPageRouter *router,
    const QString &id,
    int *activationCount = nullptr
)
{
    HubPageRegistration registration;
    registration.id = id;
    registration.page = new QWidget;
    registration.activated = [activationCount](bool) {
        if (activationCount) {
            ++(*activationCount);
        }
    };
    QVERIFY(router->registerPage(registration));
}

struct NavigationFixture
{
    NavigationFixture()
        : router(new HubPageRouter)
    {
        registerPage(router, QStringLiteral("home"));
        registerPage(router, QStringLiteral("function"), &functionActivations);
        registerPage(router, QStringLiteral("settings"));

        CommandCenterShellAccess shellAccess;
        shellAccess.functionsProvider = [this]() {
            ++functionProviderCalls;
            return QVector<CommandCenterFunctionItem>();
        };
        shell.reset(new CommandCenterShell(shellAccess, router));

        HubNavigationControllerAccess access;
        access.currentFunctionId = [this]() { return functionId; };
        access.setCurrentFunctionId = [this](const QString &id) {
            functionId = id;
            return true;
        };
        access.clearCurrentFunction = [this]() { functionId.clear(); };
        access.canLeaveFunctionPage = [this]() {
            ++canLeaveCalls;
            return allowLeave;
        };
        access.addFunction = [this]() {
            ++addFunctionCalls;
            return true;
        };
        controller.reset(new HubNavigationController(router, shell.data(), access));
    }

    HubPageRouter *router = nullptr;
    QScopedPointer<CommandCenterShell> shell;
    QScopedPointer<HubNavigationController> controller;
    QString functionId;
    int functionActivations = 0;
    int addFunctionCalls = 0;
    int functionProviderCalls = 0;
    int canLeaveCalls = 0;
    bool allowLeave = true;
};

} // namespace

class HubNavigationControllerTests : public QObject
{
    Q_OBJECT

private slots:
    void synchronizesPageAndSidebarState();
    void opensFunctionOnlyOnce();
    void clearsFunctionWhenOpeningHomeOrTool();
    void blocksEveryCrossPageNavigationWhenDraftFlushFails();
    void doesNotGateFunctionToFunctionNavigation();
    void refreshesNavigationAndAddsFunction();
    void rejectsInvalidNavigation();
    void hubWindowDelegatesNavigation();
};

void HubNavigationControllerTests::synchronizesPageAndSidebarState()
{
    NavigationFixture fixture;
    fixture.functionId = QStringLiteral("dictate");

    QVERIFY(fixture.controller->selectPage(QStringLiteral("function")));
    QCOMPARE(fixture.router->currentPageId(), QStringLiteral("function"));
    QCOMPARE(fixture.shell->activePageId(), QStringLiteral("function"));
    QCOMPARE(fixture.shell->activeFunctionId(), QStringLiteral("dictate"));
}

void HubNavigationControllerTests::opensFunctionOnlyOnce()
{
    NavigationFixture fixture;

    QVERIFY(fixture.controller->openFunction(QStringLiteral(" translate ")));
    QCOMPARE(fixture.functionId, QStringLiteral("translate"));
    QCOMPARE(fixture.functionActivations, 1);

    QVERIFY(!fixture.controller->openFunction(QStringLiteral("translate")));
    QCOMPARE(fixture.functionActivations, 1);
}

void HubNavigationControllerTests::clearsFunctionWhenOpeningHomeOrTool()
{
    NavigationFixture fixture;
    QVERIFY(fixture.controller->openFunction(QStringLiteral("dictate")));

    QVERIFY(fixture.controller->openFunction(QStringLiteral("home")));
    QVERIFY(fixture.functionId.isEmpty());
    QCOMPARE(fixture.router->currentPageId(), QStringLiteral("home"));

    fixture.functionId = QStringLiteral("ask");
    QVERIFY(fixture.controller->openTool(QStringLiteral("settings")));
    QVERIFY(fixture.functionId.isEmpty());
    QCOMPARE(fixture.shell->activePageId(), QStringLiteral("settings"));
}

void HubNavigationControllerTests::blocksEveryCrossPageNavigationWhenDraftFlushFails()
{
    NavigationFixture fixture;
    QVERIFY(fixture.controller->openFunction(QStringLiteral("dictate")));
    fixture.allowLeave = false;

    const int functionActivations = fixture.functionActivations;
    const QString activePage = fixture.shell->activePageId();
    const QString activeFunction = fixture.shell->activeFunctionId();

    QVERIFY(!fixture.controller->selectPage(QStringLiteral("settings")));
    QCOMPARE(fixture.canLeaveCalls, 1);
    QCOMPARE(fixture.router->currentPageId(), QStringLiteral("function"));
    QCOMPARE(fixture.shell->activePageId(), activePage);
    QCOMPARE(fixture.shell->activeFunctionId(), activeFunction);
    QCOMPARE(fixture.functionId, QStringLiteral("dictate"));
    QCOMPARE(fixture.functionActivations, functionActivations);

    QVERIFY(!fixture.controller->openTool(QStringLiteral("settings")));
    QCOMPARE(fixture.canLeaveCalls, 2);
    QCOMPARE(fixture.router->currentPageId(), QStringLiteral("function"));
    QCOMPARE(fixture.functionId, QStringLiteral("dictate"));

    QVERIFY(!fixture.controller->openFunction(QStringLiteral("home")));
    QCOMPARE(fixture.canLeaveCalls, 3);
    QCOMPARE(fixture.router->currentPageId(), QStringLiteral("function"));
    QCOMPARE(fixture.functionId, QStringLiteral("dictate"));
}

void HubNavigationControllerTests::doesNotGateFunctionToFunctionNavigation()
{
    NavigationFixture fixture;
    QVERIFY(fixture.controller->openFunction(QStringLiteral("dictate")));
    fixture.allowLeave = false;

    QVERIFY(fixture.controller->openFunction(QStringLiteral("translate")));
    QCOMPARE(fixture.canLeaveCalls, 0);
    QCOMPARE(fixture.functionId, QStringLiteral("translate"));
    QCOMPARE(fixture.router->currentPageId(), QStringLiteral("function"));
}

void HubNavigationControllerTests::refreshesNavigationAndAddsFunction()
{
    NavigationFixture fixture;
    const int refreshBefore = fixture.functionProviderCalls;

    fixture.controller->refreshFunctions();
    QVERIFY(fixture.functionProviderCalls > refreshBefore);
    QVERIFY(fixture.controller->addFunction());
    QCOMPARE(fixture.addFunctionCalls, 1);
}

void HubNavigationControllerTests::rejectsInvalidNavigation()
{
    NavigationFixture fixture;

    QVERIFY(!fixture.controller->selectPage(QString()));
    QVERIFY(!fixture.controller->openFunction(QStringLiteral("  ")));
    QVERIFY(!fixture.controller->openTool(QStringLiteral("missing")));

    HubNavigationController emptyController(
        nullptr,
        nullptr,
        HubNavigationControllerAccess()
    );
    QVERIFY(!emptyController.selectPage(QStringLiteral("home")));
    QVERIFY(!emptyController.addFunction());
}

void HubNavigationControllerTests::hubWindowDelegatesNavigation()
{
    const QString sourcePath = QFINDTESTDATA("../../src/ui/hub_window.cpp");
    QVERIFY2(!sourcePath.isEmpty(), "Cannot find HubWindow source file");
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();

    QVERIFY(contents.contains("HubNavigationController"));
    QVERIFY(contents.contains("navigationController()->openFunction(id)"));
    QVERIFY(contents.contains("navigationController()->openTool(id)"));
    QVERIFY(!contents.contains("m_commandShell"));
    QVERIFY(!contents.contains("m_pageRouter"));
}

QTEST_MAIN(HubNavigationControllerTests)

#include "hub_navigation_controller_tests.moc"
