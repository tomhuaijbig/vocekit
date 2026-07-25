#include <QtTest>

#include "../../src/ui/command_search_router.h"

#include <QFile>
#include <type_traits>

class CommandSearchRouterTests : public QObject
{
    Q_OBJECT

private slots:
    void exposesTypedSearchResult();
    void resolvesFunctionsBeforePages();
    void resolvesPagesAndAliases();
    void rejectsEmptyAndUnknownQueries();
    void hubWindowDoesNotMatchSearchKeywords();
};

void CommandSearchRouterTests::exposesTypedSearchResult()
{
    QVERIFY((std::is_default_constructible<CommandSearchEntry>::value));
    QVERIFY((std::is_default_constructible<CommandSearchResult>::value));
}

void CommandSearchRouterTests::resolvesFunctionsBeforePages()
{
    CommandSearchEntry function;
    function.id = QStringLiteral("custom-summary");
    function.title = QString::fromUtf8("日志总结");

    CommandSearchEntry overlappingPage;
    overlappingPage.id = QStringLiteral("logs");
    overlappingPage.title = QString::fromUtf8("日志");

    const CommandSearchResult result = CommandSearchRouter::resolve(
        QString::fromUtf8("日志"),
        QVector<CommandSearchEntry>() << function,
        QVector<CommandSearchEntry>() << overlappingPage
    );

    QCOMPARE(result.type, CommandSearchTargetType::Function);
    QCOMPARE(result.id, QStringLiteral("custom-summary"));
}

void CommandSearchRouterTests::resolvesPagesAndAliases()
{
    const QVector<CommandSearchEntry> pages = CommandSearchRouter::defaultPages();

    CommandSearchResult result = CommandSearchRouter::resolve(
        QString::fromUtf8("历史"),
        QVector<CommandSearchEntry>(),
        pages
    );
    QCOMPARE(result.type, CommandSearchTargetType::Page);
    QCOMPARE(result.id, QStringLiteral("history"));

    result = CommandSearchRouter::resolve(
        QStringLiteral("OCR"),
        QVector<CommandSearchEntry>(),
        pages
    );
    QCOMPARE(result.type, CommandSearchTargetType::Page);
    QCOMPARE(result.id, QStringLiteral("ocr"));

    result = CommandSearchRouter::resolve(
        QString::fromUtf8("帮助"),
        QVector<CommandSearchEntry>(),
        pages
    );
    QCOMPARE(result.type, CommandSearchTargetType::Page);
    QCOMPARE(result.id, QStringLiteral("faq"));
}

void CommandSearchRouterTests::rejectsEmptyAndUnknownQueries()
{
    const QVector<CommandSearchEntry> pages = CommandSearchRouter::defaultPages();
    QVERIFY(!CommandSearchRouter::resolve(
        QStringLiteral("   "), QVector<CommandSearchEntry>(), pages).isValid());
    QVERIFY(!CommandSearchRouter::resolve(
        QString::fromUtf8("不存在的项目"), QVector<CommandSearchEntry>(), pages).isValid());
}

void CommandSearchRouterTests::hubWindowDoesNotMatchSearchKeywords()
{
    const QString hubPath = QFINDTESTDATA("../../src/ui/hub_window.cpp");
    QVERIFY2(!hubPath.isEmpty(), "找不到 HubWindow 源文件");
    QFile hub(hubPath);
    QVERIFY(hub.open(QIODevice::ReadOnly));
    const QByteArray hubContents = hub.readAll();
    QVERIFY(!hubContents.contains("navigateFromCommandSearch"));
    QVERIFY(!hubContents.contains("def.title.contains(keyword"));
    QVERIFY(!hubContents.contains("fn.name.contains(keyword"));
    QVERIFY(!hubContents.contains("QVector<QPair<QString, QString>> pages"));

    const QString shellPath = QFINDTESTDATA("../../src/ui/command_center_shell.cpp");
    QVERIFY2(!shellPath.isEmpty(), "找不到导航外壳源文件");
    QFile shell(shellPath);
    QVERIFY(shell.open(QIODevice::ReadOnly));
    QVERIFY(shell.readAll().contains("CommandSearchRouter::resolve"));
}

QTEST_APPLESS_MAIN(CommandSearchRouterTests)

#include "command_search_router_tests.moc"
