#include "../../src/runtime/function_flow_runtime_log.h"

#include <QtTest>

#include <QFile>
#include <QJsonDocument>
#include <QSet>
#include <QTemporaryDir>

namespace {

FunctionFlowRuntimeLogEntry completeEntry()
{
    FunctionFlowRuntimeLogEntry entry;
    entry.functionId = QStringLiteral("custom_translate");
    entry.publishedRevision = 3;
    entry.publishedHash = QString(64, QChar('a'));
    entry.runId.value = QStringLiteral("run-123");
    entry.trigger = QStringLiteral("MainHotkey");
    entry.nodeId = QStringLiteral("model-1");
    entry.nodeType = QStringLiteral("Model");
    entry.status = QStringLiteral("Failed");
    entry.elapsedMs = 245;
    entry.error.code = QStringLiteral("flow_model_failed");
    entry.error.message = QStringLiteral("完整选中文字");
    entry.error.detail =
        QStringLiteral("data:image/png;base64,private-image sk-private-key");
    entry.modelId = QStringLiteral("deepseek-v4-flash");
    entry.promptVersion = QStringLiteral("translate-v3");
    entry.httpStatus = 429;
    return entry;
}

} // namespace

class FunctionFlowRuntimeLogTests : public QObject
{
    Q_OBJECT

private slots:
    void serializesOnlyWhitelistedMetadata();
    void dropsUnsafeAndUnknownValues();
    void appendsCompactJsonLines();
};

void FunctionFlowRuntimeLogTests::serializesOnlyWhitelistedMetadata()
{
    const FunctionFlowRuntimeLogEntry entry = completeEntry();
    const QJsonObject object = functionFlowRuntimeLogMetadata(entry);

    const QSet<QString> expected = QSet<QString>::fromList(
        QStringList()
            << QStringLiteral("functionId")
            << QStringLiteral("publishedRevision")
            << QStringLiteral("publishedHash")
            << QStringLiteral("runId")
            << QStringLiteral("trigger")
            << QStringLiteral("nodeId")
            << QStringLiteral("nodeType")
            << QStringLiteral("status")
            << QStringLiteral("elapsedMs")
            << QStringLiteral("errorCode")
            << QStringLiteral("modelId")
            << QStringLiteral("promptVersion")
            << QStringLiteral("httpStatus")
    );
    QCOMPARE(QSet<QString>::fromList(object.keys()), expected);
    QCOMPARE(object.value(QStringLiteral("functionId")).toString(), entry.functionId);
    QCOMPARE(object.value(QStringLiteral("publishedRevision")).toInt(), 3);
    QCOMPARE(object.value(QStringLiteral("publishedHash")).toString(), entry.publishedHash);
    QCOMPARE(object.value(QStringLiteral("runId")).toString(), entry.runId.value);
    QCOMPARE(object.value(QStringLiteral("elapsedMs")).toInt(), 245);
    QCOMPARE(
        object.value(QStringLiteral("errorCode")).toString(),
        QStringLiteral("flow_model_failed")
    );
    QCOMPARE(object.value(QStringLiteral("httpStatus")).toInt(), 429);

    const QByteArray serialized = functionFlowRuntimeLogLine(entry);
    QVERIFY(!serialized.contains("完整选中文字"));
    QVERIFY(!serialized.contains("data:image"));
    QVERIFY(!serialized.contains("sk-private-key"));
    QVERIFY(!serialized.contains("message"));
    QVERIFY(!serialized.contains("detail"));
}

void FunctionFlowRuntimeLogTests::dropsUnsafeAndUnknownValues()
{
    FunctionFlowRuntimeLogEntry entry;
    entry.functionId = QStringLiteral("完整用户正文");
    entry.publishedHash = QStringLiteral("not-a-full-hash");
    entry.runId.value = QStringLiteral("data:image/png;base64,private");
    entry.trigger = QStringLiteral("MainHotkey");
    entry.nodeId = QStringLiteral("node-1");
    entry.nodeType = QStringLiteral("Model");
    entry.status = QStringLiteral("Failed");
    entry.error.code = QStringLiteral("provider said sk-private");
    entry.error.message = QStringLiteral("正文");
    entry.error.detail = QStringLiteral("Authorization: Bearer private");
    entry.modelId = QStringLiteral("sk-private-api-key");
    entry.promptVersion = QString(400, QChar('p'));
    entry.httpStatus = 999;

    const QJsonObject object = functionFlowRuntimeLogMetadata(entry);
    QVERIFY(!object.contains(QStringLiteral("functionId")));
    QVERIFY(!object.contains(QStringLiteral("publishedHash")));
    QVERIFY(!object.contains(QStringLiteral("runId")));
    QVERIFY(!object.contains(QStringLiteral("errorCode")));
    QVERIFY(!object.contains(QStringLiteral("modelId")));
    QVERIFY(!object.contains(QStringLiteral("promptVersion")));
    QVERIFY(!object.contains(QStringLiteral("httpStatus")));

    const QByteArray serialized = functionFlowRuntimeLogLine(entry);
    QVERIFY(!serialized.contains("完整用户正文"));
    QVERIFY(!serialized.contains("data:image"));
    QVERIFY(!serialized.contains("sk-private"));
    QVERIFY(!serialized.contains("Authorization"));
    QVERIFY(!serialized.contains("Bearer"));
}

void FunctionFlowRuntimeLogTests::appendsCompactJsonLines()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString path =
        temp.filePath(QStringLiteral("logs/function-flow.jsonl"));
    const FunctionFlowRuntimeLogEntry entry = completeEntry();

    QVERIFY(appendFunctionFlowRuntimeLog(path, entry));
    QVERIFY(appendFunctionFlowRuntimeLog(path, entry));

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QList<QByteArray> lines = file.readAll().split('\n');
    QCOMPARE(lines.size(), 3);
    QVERIFY(lines.last().isEmpty());

    for (int i = 0; i < 2; ++i) {
        QVERIFY(!lines.at(i).contains("data:image"));
        QVERIFY(!lines.at(i).contains("sk-private"));
        const QJsonDocument document = QJsonDocument::fromJson(lines.at(i));
        QVERIFY(document.isObject());
        QCOMPARE(
            document.object().value(QStringLiteral("runId")).toString(),
            QStringLiteral("run-123")
        );
    }
}

QTEST_APPLESS_MAIN(FunctionFlowRuntimeLogTests)

#include "function_flow_runtime_log_tests.moc"
