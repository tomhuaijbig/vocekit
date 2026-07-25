#include <QtTest>

#include "../../src/ui/function_editor_dialog.h"

#include <QFile>
#include <type_traits>

class FunctionEditorDialogHeaderTests : public QObject
{
    Q_OBJECT

  private slots:
    void exposesIndependentDialogInterface();
    void hubWindowDoesNotConstructFunctionEditor();
};

void FunctionEditorDialogHeaderTests::exposesIndependentDialogInterface()
{
    QVERIFY((std::is_default_constructible<FunctionEditorDialogRequest>::value));
    QVERIFY((std::is_default_constructible<FunctionEditorDialogAccess>::value));
    QVERIFY((std::is_same<
             decltype(runFunctionEditorDialog(
                 std::declval<const FunctionEditorDialogRequest &>(),
                 std::declval<const FunctionEditorDialogAccess &>(),
                 static_cast<QWidget *>(nullptr))),
             bool>::value));
}

void FunctionEditorDialogHeaderTests::hubWindowDoesNotConstructFunctionEditor()
{
    const QString sourcePath = QFINDTESTDATA("../../src/ui/hub_window.cpp");
    const QString workspacePath = QFINDTESTDATA(
        "../../src/ui/hub_function_workspace_controller.cpp"
    );
    QVERIFY2(!sourcePath.isEmpty(), "找不到 HubWindow 源文件");
    QVERIFY2(!workspacePath.isEmpty(), "Cannot find workspace controller source");
    QFile source(sourcePath);
    QFile workspaceSource(workspacePath);
    QVERIFY(source.open(QIODevice::ReadOnly));
    QVERIFY(workspaceSource.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();
    const QByteArray workspaceContents = workspaceSource.readAll();
    QVERIFY(!contents.contains("AppDialog dialog(this);"));
    QVERIFY(!contents.contains("QWidget *dialogSection("));
    QVERIFY(!contents.contains("QComboBox *hubModelCombo("));
    QVERIFY(!contents.contains("QComboBox *hubOutputModeCombo("));
    QVERIFY(!contents.contains("QComboBox *hubResultTemplateCombo("));
    QVERIFY(!contents.contains("QSpinBox *hubDisplayTimeSpinBox("));
    QVERIFY(!contents.contains("runFunctionEditorDialog"));
    QVERIFY(workspaceContents.contains("runFunctionEditorDialog"));
}

QTEST_APPLESS_MAIN(FunctionEditorDialogHeaderTests)

#include "function_editor_dialog_header_tests.moc"
