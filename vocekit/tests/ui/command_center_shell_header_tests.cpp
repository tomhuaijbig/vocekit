#include <QtTest>

#include "../../src/ui/command_center_shell.h"

#include <QCoreApplication>
#include <QEvent>
#include <QFile>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>
#include <type_traits>

class CommandCenterShellHeaderTests : public QObject
{
    Q_OBJECT

private slots:
    void exposesNavigationShellInterface();
    void hubWindowDoesNotRenderNavigationControls();
    void addFunctionActionFollowsFunctionRows();
};

void CommandCenterShellHeaderTests::exposesNavigationShellInterface()
{
    QVERIFY((std::is_default_constructible<CommandCenterFunctionItem>::value));
    QVERIFY((std::is_default_constructible<CommandCenterShellAccess>::value));
    QVERIFY((std::is_base_of<QWidget, CommandCenterShell>::value));
    QVERIFY((std::is_constructible<
             CommandCenterShell,
             const CommandCenterShellAccess &,
             QWidget *,
             QWidget *>::value));
}

void CommandCenterShellHeaderTests::hubWindowDoesNotRenderNavigationControls()
{
    const QString sourcePath = QFINDTESTDATA("../../src/ui/hub_window.cpp");
    QVERIFY2(!sourcePath.isEmpty(), "找不到 HubWindow 源文件");
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();
    QVERIFY(contents.contains("CommandCenterShell"));
    QVERIFY(!contents.contains("QWidget *sidebar()"));
    QVERIFY(!contents.contains("QWidget *commandToolbar()"));
    QVERIFY(!contents.contains("QPushButton *functionNavButton("));
    QVERIFY(!contents.contains("QPushButton *toolNavButton("));
    QVERIFY(!contents.contains("m_functionNavButtons"));
    QVERIFY(!contents.contains("m_toolNavButtons"));
    QVERIFY(!contents.contains("m_customFunctionNavLayout"));
    QVERIFY(!contents.contains("m_commandSearchEdit"));
}

void CommandCenterShellHeaderTests::addFunctionActionFollowsFunctionRows()
{
    QVector<CommandCenterFunctionItem> functions;
    CommandCenterFunctionItem dictate;
    dictate.id = QStringLiteral("dictate");
    dictate.title = QString::fromUtf8("听写");
    functions.append(dictate);

    CommandCenterFunctionItem custom;
    custom.id = QStringLiteral("custom_1");
    custom.title = QString::fromUtf8("自定义功能 1");
    functions.append(custom);

    int addFunctionCalls = 0;
    CommandCenterShellAccess access;
    access.functionsProvider = [&functions]() {
        return functions;
    };
    access.addFunction = [&addFunctionCalls]() {
        ++addFunctionCalls;
    };

    CommandCenterShell shell(access, new QWidget);
    shell.resize(720, 220);
    shell.show();
    QCoreApplication::processEvents();

    const QString addFunctionText = QString::fromUtf8("新增功能");
    const auto addButtonsByText = [&shell, &addFunctionText]() {
        QList<QPushButton *> matches;
        const QList<QPushButton *> buttons = shell.findChildren<QPushButton *>();
        for (QPushButton *button : buttons) {
            if (button->text() == addFunctionText) {
                matches.append(button);
            }
        }
        return matches;
    };

    auto *scroll = shell.findChild<QScrollArea *>(QStringLiteral("commandFunctionScroll"));
    auto *add = shell.findChild<QPushButton *>(QStringLiteral("commandAddFunctionButton"));
    QVERIFY(scroll);
    QVERIFY(add);
    const QList<QPushButton *> initialTextMatches = addButtonsByText();
    QCOMPARE(initialTextMatches.size(), 1);
    QCOMPARE(initialTextMatches.first(), add);
    QVERIFY(scroll->widget());
    QVERIFY(scroll->widget()->isAncestorOf(add));

    auto *functionLayout = qobject_cast<QVBoxLayout *>(scroll->widget()->layout());
    QVERIFY(functionLayout);
    QCOMPARE(functionLayout->count(), functions.size() + 2);
    auto *dictateButton = qobject_cast<QPushButton *>(functionLayout->itemAt(0)->widget());
    auto *customButton = qobject_cast<QPushButton *>(functionLayout->itemAt(1)->widget());
    QVERIFY(dictateButton);
    QVERIFY(customButton);
    QCOMPARE(dictateButton->property("functionId").toString(), QStringLiteral("dictate"));
    QCOMPARE(customButton->property("functionId").toString(), QStringLiteral("custom_1"));
    QCOMPARE(functionLayout->itemAt(functions.size())->widget(), add);
    QVERIFY(functionLayout->itemAt(functions.size() + 1)->spacerItem());

    scroll->ensureWidgetVisible(add);
    QCoreApplication::processEvents();
    QTest::mouseClick(add, Qt::LeftButton);
    QCOMPARE(addFunctionCalls, 1);

    shell.refreshFunctions();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();

    const QList<QPushButton *> refreshedTextMatches = addButtonsByText();
    QCOMPARE(refreshedTextMatches.size(), 1);
    const QList<QPushButton *> addButtons =
        shell.findChildren<QPushButton *>(QStringLiteral("commandAddFunctionButton"));
    QCOMPARE(addButtons.size(), 1);
    add = addButtons.first();
    QCOMPARE(refreshedTextMatches.first(), add);
    QVERIFY(scroll->widget()->isAncestorOf(add));
    functionLayout = qobject_cast<QVBoxLayout *>(scroll->widget()->layout());
    QVERIFY(functionLayout);
    QCOMPARE(functionLayout->count(), functions.size() + 2);
    dictateButton = qobject_cast<QPushButton *>(functionLayout->itemAt(0)->widget());
    customButton = qobject_cast<QPushButton *>(functionLayout->itemAt(1)->widget());
    QVERIFY(dictateButton);
    QVERIFY(customButton);
    QCOMPARE(dictateButton->property("functionId").toString(), QStringLiteral("dictate"));
    QCOMPARE(customButton->property("functionId").toString(), QStringLiteral("custom_1"));
    QCOMPARE(functionLayout->itemAt(functions.size())->widget(), add);
    QVERIFY(functionLayout->itemAt(functions.size() + 1)->spacerItem());

    QVERIFY(scroll->verticalScrollBar()->maximum() > 0);
    scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
    QCoreApplication::processEvents();
    const QRect addRect(add->mapTo(scroll->viewport(), QPoint(0, 0)), add->size());
    QVERIFY(scroll->viewport()->rect().intersects(addRect));
    QTest::mouseClick(add, Qt::LeftButton);
    QCOMPARE(addFunctionCalls, 2);
}

QTEST_MAIN(CommandCenterShellHeaderTests)

#include "command_center_shell_header_tests.moc"
