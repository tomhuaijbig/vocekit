#include <QtTest>

#include "../../src/output/classic_auto_write_executor.h"

class ClassicAutoWriteExecutorTests : public QObject
{
    Q_OBJECT

private slots:
    void failuresPreserveCompleteOutputInOnePopup_data()
    {
        QTest::addColumn<QString>("code");
        QTest::newRow("missing target")
            << QStringLiteral("flow_target_window_unavailable");
        QTest::newRow("activation")
            << QStringLiteral("flow_target_window_activation_failed");
        QTest::newRow("injection")
            << QStringLiteral("flow_input_injection_failed");
        QTest::newRow("clipboard")
            << QStringLiteral("flow_clipboard_unavailable");
    }

    void failuresPreserveCompleteOutputInOnePopup()
    {
        QFETCH(QString, code);
        const QString output = QString::fromUtf8(
            "这是完整结果。\n第二行也必须保留。"
        );
        int writes = 0;
        bool receivedExpectedRequest = false;
        QStringList popups;
        QStringList statuses;
        QStringList logs;
        ClassicAutoWriteAccess access;
        access.checkedWrite = [&](const QString &text, bool replace, bool selected)
            -> ClipboardWriteResult {
            ++writes;
            receivedExpectedRequest =
                text == output && replace && selected;
            ClipboardWriteResult result;
            result.errorCode = code;
            return result;
        };
        access.setStatus = [&](const QString &title, const QString &detail) {
            statuses << title + QStringLiteral("|") + detail;
        };
        access.showFallbackPopup = [&](const QString &text) {
            popups.append(text);
        };
        access.log = [&](const QString &action, const QString &detail) {
            logs << action + QStringLiteral("|") + detail;
        };
        ClassicAutoWriteRequest request;
        request.text = output;
        request.hasSelection = true;

        const ClipboardWriteResult result =
            ClassicAutoWriteExecutor::execute(request, access);

        QVERIFY(!result.ok);
        QCOMPARE(result.errorCode, code);
        QCOMPARE(writes, 1);
        QVERIFY(receivedExpectedRequest);
        QCOMPARE(popups, QStringList() << output);
        QVERIFY(!statuses.join(QStringLiteral(" ")).contains(
            QString::fromUtf8("已写入")
        ));
        QVERIFY(logs.join(QStringLiteral(" ")).contains(code));
        QVERIFY(!logs.join(QStringLiteral(" ")).contains(output));
    }

    void successAndDisabledFallbackDoNotOpenPopup()
    {
        int popups = 0;
        int writes = 0;
        ClassicAutoWriteAccess access;
        access.checkedWrite = [&](const QString &, bool, bool)
            -> ClipboardWriteResult {
            ++writes;
            ClipboardWriteResult result;
            result.ok = writes == 1;
            result.errorCode = result.ok
                ? QString()
                : QStringLiteral("flow_input_injection_failed");
            return result;
        };
        access.showFallbackPopup = [&](const QString &) { ++popups; };
        ClassicAutoWriteRequest request;
        request.text = QStringLiteral("output");
        QCOMPARE(ClassicAutoWriteExecutor::execute(request, access).ok, true);
        QCOMPARE(popups, 0);
        request.popupFallbackEnabled = false;
        QCOMPARE(ClassicAutoWriteExecutor::execute(request, access).ok, false);
        QCOMPARE(popups, 0);
        QCOMPARE(writes, 2);
    }

    void missingWriterReturnsFocusedFailure()
    {
        int popups = 0;
        ClassicAutoWriteRequest request;
        request.text = QStringLiteral("preserved");
        ClassicAutoWriteAccess access;
        access.showFallbackPopup = [&](const QString &text) {
            QCOMPARE(text, request.text);
            ++popups;
        };
        const ClipboardWriteResult result =
            ClassicAutoWriteExecutor::execute(request, access);
        QVERIFY(!result.ok);
        QCOMPARE(result.errorCode,
                 QStringLiteral("flow_auto_write_failed"));
        QCOMPARE(popups, 1);
    }
};

QTEST_APPLESS_MAIN(ClassicAutoWriteExecutorTests)

#include "classic_auto_write_executor_tests.moc"
