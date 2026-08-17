#include <QtTest>

#include "../../src/ui/hub_page_composition.h"

#include <QFile>
#include <QWidget>
#include <type_traits>

namespace {

std::function<QWidget *()> pageFactory()
{
    return []() { return new QWidget; };
}

HubPageCompositionAccess completeAccess()
{
    HubPageCompositionAccess access;
    access.homePage = pageFactory();
    access.functionPage = pageFactory();
    access.historyPage = pageFactory();
    access.vocabularyPage = pageFactory();
    access.ocrPage = pageFactory();
    access.promptsPage = pageFactory();
    access.diagnosticsPage = pageFactory();
    access.logsPage = pageFactory();
    access.settingsPage = pageFactory();
    access.faqPage = pageFactory();
    return access;
}

} // namespace

class HubPageCompositionTests : public QObject
{
    Q_OBJECT

private slots:
    void exposesIndependentCompositionInterface();
    void registersAllCommandCenterPages();
    void defersNonHomePagesUntilFirstSelection();
    void forwardsActivationState();
    void skipsMissingPageFactories();
    void hubWindowDoesNotRegisterPagesDirectly();
};

void HubPageCompositionTests::exposesIndependentCompositionInterface()
{
    QVERIFY((std::is_default_constructible<HubPageCompositionAccess>::value));
    QVERIFY((std::is_same<
             decltype(HubPageComposition::create(
                 std::declval<const HubPageCompositionAccess &>(),
                 static_cast<QWidget *>(nullptr))),
             HubPageRouter *>::value));
}

void HubPageCompositionTests::registersAllCommandCenterPages()
{
    QScopedPointer<HubPageRouter> router(HubPageComposition::create(completeAccess()));
    QCOMPARE(router->count(), 10);

    const QStringList pageIds = {
        QStringLiteral("home"),
        QStringLiteral("function"),
        QStringLiteral("history"),
        QStringLiteral("vocabulary"),
        QStringLiteral("ocr"),
        QStringLiteral("prompts"),
        QStringLiteral("diagnostics"),
        QStringLiteral("logs"),
        QStringLiteral("settings"),
        QStringLiteral("faq")
    };
    for (const QString &pageId : pageIds) {
        QVERIFY2(router->selectPage(pageId), qPrintable(pageId));
        QCOMPARE(router->currentPageId(), pageId);
    }
}

void HubPageCompositionTests::defersNonHomePagesUntilFirstSelection()
{
    int homeCreations = 0;
    int historyCreations = 0;
    int settingsCreations = 0;
    HubPageCompositionAccess access = completeAccess();
    access.homePage = [&homeCreations]() {
        ++homeCreations;
        return new QWidget;
    };
    access.historyPage = [&historyCreations]() {
        ++historyCreations;
        return new QWidget;
    };
    access.settingsPage = [&settingsCreations]() {
        ++settingsCreations;
        return new QWidget;
    };

    QScopedPointer<HubPageRouter> router(
        HubPageComposition::create(access)
    );
    QCOMPARE(homeCreations, 1);
    QCOMPARE(historyCreations, 0);
    QCOMPARE(settingsCreations, 0);

    QVERIFY(router->selectPage(QStringLiteral("history")));
    QCOMPARE(historyCreations, 1);
    QCOMPARE(settingsCreations, 0);

    QVERIFY(router->selectPage(QStringLiteral("history")));
    QCOMPARE(historyCreations, 1);

    QVERIFY(router->selectPage(QStringLiteral("settings")));
    QCOMPARE(settingsCreations, 1);
}

void HubPageCompositionTests::forwardsActivationState()
{
    HubPageCompositionAccess access = completeAccess();
    QVector<bool> settingsChanges;
    access.settingsActivated = [&settingsChanges](bool changed) {
        settingsChanges.append(changed);
    };

    QScopedPointer<HubPageRouter> router(HubPageComposition::create(access));
    QVERIFY(router->selectPage(QStringLiteral("settings")));
    QVERIFY(router->selectPage(QStringLiteral("settings")));
    QCOMPARE(settingsChanges, QVector<bool>() << true << false);
}

void HubPageCompositionTests::skipsMissingPageFactories()
{
    QScopedPointer<HubPageRouter> router(
        HubPageComposition::create(HubPageCompositionAccess())
    );
    QCOMPARE(router->count(), 0);
    QVERIFY(!router->selectPage(QStringLiteral("home")));
}

void HubPageCompositionTests::hubWindowDoesNotRegisterPagesDirectly()
{
    const QString sourcePath = QFINDTESTDATA("../../src/ui/hub_window.cpp");
    QVERIFY2(!sourcePath.isEmpty(), "找不到 HubWindow 源文件");
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();
    QVERIFY(contents.contains("HubPageHostController"));
    QVERIFY(!contents.contains("HubPageComposition::create"));
    QVERIFY(!contents.contains("HubPageRegistration registration"));
    QVERIFY(!contents.contains("m_pageRouter->registerPage"));
}

QTEST_MAIN(HubPageCompositionTests)

#include "hub_page_composition_tests.moc"
