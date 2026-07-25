#include <QtTest>

#include "../../src/tasks/selected_text_diagnostic_task.h"

namespace {

QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

} // namespace

class SelectedTextDiagnosticTaskTests : public QObject
{
    Q_OBJECT

private slots:
    void emptySelectionReportsNotRecognized()
    {
        SelectedTextDiagnosticRequest request;
        request.strongMode = false;

        const SelectedTextDiagnosticResult result = runSelectedTextDiagnosticTask(request);

        QVERIFY(!result.success);
        QCOMPARE(result.characterCount, 0);
        QVERIFY(result.displayText.contains(tr8("未识别到")));
        QVERIFY(result.displayText.contains(tr8("普通读取")));
    }

    void strongModeUsesStrongTitle()
    {
        SelectedTextDiagnosticRequest request;
        request.strongMode = true;

        const SelectedTextDiagnosticResult result = runSelectedTextDiagnosticTask(request);

        QVERIFY(!result.success);
        QVERIFY(result.displayText.contains(tr8("强力读取")));
    }

    void selectedTextReportsCharacterCount()
    {
        SelectedTextDiagnosticRequest request;
        request.selectedText = tr8("测试文字");

        const SelectedTextDiagnosticResult result = runSelectedTextDiagnosticTask(request);

        QVERIFY(result.success);
        QCOMPARE(result.characterCount, 4);
        QVERIFY(result.displayText.contains(tr8("通过")));
        QVERIFY(result.displayText.contains(tr8("读取到 4 个字符")));
    }
};

QTEST_MAIN(SelectedTextDiagnosticTaskTests)
#include "selected_text_diagnostic_task_tests.moc"
