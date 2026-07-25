#include <QtTest>

#include "../../src/ui/command_center_shell.h"

#include <QFile>
#include <type_traits>

class CommandCenterShellHeaderTests : public QObject
{
    Q_OBJECT

private slots:
    void exposesNavigationShellInterface();
    void hubWindowDoesNotRenderNavigationControls();
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

QTEST_APPLESS_MAIN(CommandCenterShellHeaderTests)

#include "command_center_shell_header_tests.moc"
