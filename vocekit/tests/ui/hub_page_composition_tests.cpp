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
