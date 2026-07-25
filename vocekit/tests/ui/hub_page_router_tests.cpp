#include <QtTest>

#include "../../src/ui/hub_page_router.h"

#include <QFile>
#include <QWidget>
#include <type_traits>

class HubPageRouterTests : public QObject
{
    Q_OBJECT

private slots:
    void exposesIndependentRouterInterface();
    void selectsRegisteredPagesAndReportsChanges();
    void rejectsInvalidAndDuplicatePages();
    void hubWindowDoesNotUseFixedPageIndexes();
};

void HubPageRouterTests::exposesIndependentRouterInterface()
{
    QVERIFY((std::is_default_constructible<HubPageRegistration>::value));
    QVERIFY((std::is_base_of<QStackedWidget, HubPageRouter>::value));
    QVERIFY((std::is_constructible<HubPageRouter, QWidget *>::value));
}

void HubPageRouterTests::selectsRegisteredPagesAndReportsChanges()
{
    HubPageRouter router;
    auto *home = new QWidget;
    auto *history = new QWidget;
    QVector<bool> historyChanges;

    HubPageRegistration homePage;
    homePage.id = QStringLiteral("home");
    homePage.page = home;
    QVERIFY(router.registerPage(homePage));

    HubPageRegistration historyPage;
    historyPage.id = QStringLiteral("history");
    historyPage.page = history;
    historyPage.activated = [&historyChanges](bool changed) {
        historyChanges.append(changed);
    };
    QVERIFY(router.registerPage(historyPage));

    QVERIFY(router.selectPage(QStringLiteral("history")));
    QCOMPARE(router.currentPageId(), QStringLiteral("history"));
    QCOMPARE(router.currentWidget(), history);
    QCOMPARE(historyChanges, QVector<bool>() << true);

    QVERIFY(router.selectPage(QStringLiteral("history")));
    QCOMPARE(historyChanges, QVector<bool>() << true << false);
    QVERIFY(!router.selectPage(QStringLiteral("missing")));
    QCOMPARE(router.currentPageId(), QStringLiteral("history"));
}

void HubPageRouterTests::rejectsInvalidAndDuplicatePages()
{
    HubPageRouter router;
    HubPageRegistration invalid;
    QVERIFY(!router.registerPage(invalid));

    HubPageRegistration page;
    page.id = QStringLiteral("home");
    page.page = new QWidget;
    QVERIFY(router.registerPage(page));

    HubPageRegistration duplicate;
    duplicate.id = QStringLiteral("home");
    duplicate.page = new QWidget;
    QVERIFY(!router.registerPage(duplicate));
    delete duplicate.page;
}

void HubPageRouterTests::hubWindowDoesNotUseFixedPageIndexes()
{
    const QString sourcePath = QFINDTESTDATA("../../src/ui/hub_window.cpp");
    QVERIFY2(!sourcePath.isEmpty(), "找不到 HubWindow 源文件");
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();
    QVERIFY(contents.contains("HubPageRouter"));
    QVERIFY(!contents.contains("m_pages"));
    QVERIFY(!contents.contains("const QStringList order"));
    QVERIFY(!contents.contains("currentIndex() == 1"));
    QVERIFY(!contents.contains("setCurrentIndex(index)"));
}

QTEST_MAIN(HubPageRouterTests)

#include "hub_page_router_tests.moc"
