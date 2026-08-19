#include <QtTest>

#include "../../src/config/model_advanced_settings.h"
#include "../../src/providers/model_request_customization.h"
#include "../../src/providers/model_response_metadata.h"
#include "../../src/storage/model_request_log.h"

#include <QJsonArray>
#include <QTemporaryDir>

class ModelAdvancedRequestTests : public QObject
{
    Q_OBJECT

private slots:
    void rawJsonHasFinalPrecedenceAndCanDeleteFields();
    void storesIndependentSystemPromptAndDiagnosticsSettings();
    void parsesUsageFinishReasonAndStructuredCitations();
    void mergesAnthropicStreamingMetadata();
    void redactsOnlySensitiveRequestFields();
    void requestLogsDefaultToMetadataOnly();
};

void ModelAdvancedRequestTests::
rawJsonHasFinalPrecedenceAndCanDeleteFields()
{
    QJsonObject base;
    base.insert(QStringLiteral("model"), QStringLiteral("base-model"));
    base.insert(QStringLiteral("temperature"), 0.2);
    base.insert(QStringLiteral("max_tokens"), 1024);

    QJsonObject parameters;
    parameters.insert(QStringLiteral("temperature"), 0.7);
    parameters.insert(QStringLiteral("top_p"), 0.9);
    QJsonObject raw;
    raw.insert(QStringLiteral("temperature"), 1.3);
    raw.insert(QStringLiteral("max_tokens"), QJsonValue::Null);
    raw.insert(QStringLiteral("future_vendor_parameter"), QStringLiteral("works"));
    QJsonObject advanced;
    advanced.insert(QStringLiteral("enabled"), true);
    advanced.insert(QStringLiteral("parameters"), parameters);
    advanced.insert(QStringLiteral("raw_json"), raw);

    const QJsonObject result = customizedModelRequestBody(base, advanced);
    QCOMPARE(result.value(QStringLiteral("temperature")).toDouble(), 1.3);
    QCOMPARE(result.value(QStringLiteral("top_p")).toDouble(), 0.9);
    QVERIFY(!result.contains(QStringLiteral("max_tokens")));
    QCOMPARE(
        result.value(QStringLiteral("future_vendor_parameter")).toString(),
        QStringLiteral("works")
    );
}

void ModelAdvancedRequestTests::
storesIndependentSystemPromptAndDiagnosticsSettings()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("advanced.json"));
    ModelAdvancedSettingsStore store(path);

    ModelAdvancedProfile profile;
    profile.key = QStringLiteral("openai:test-model");
    profile.enabled = true;
    profile.parameters.insert(QStringLiteral("stream"), false);
    profile.rawJson.insert(QStringLiteral("unknown"), 42);
    profile.systemPromptOverrideEnabled = true;
    profile.activeSystemPromptId = QStringLiteral("empty-prompt");
    profile.systemPrompts.append(ModelSystemPromptPreset{
        QStringLiteral("empty-prompt"),
        QStringLiteral("可清空"),
        QString()
    });
    profile.modelsEndpoint = QStringLiteral("https://example.test/v1/models");
    profile.fetchedModels << QStringLiteral("vendor-new-model");
    profile.logRequestResponseContent = true;
    profile.inputPricePerMillion = 1.25;
    profile.outputPricePerMillion = 3.5;

    QVERIFY(store.saveProfile(profile));
    const ModelAdvancedProfile loaded = store.loadProfile(profile.key);
    QVERIFY(loaded.enabled);
    QVERIFY(loaded.systemPromptOverrideEnabled);
    QCOMPARE(loaded.activeSystemPromptId, QStringLiteral("empty-prompt"));
    QCOMPARE(loaded.systemPrompts.size(), 1);
    QVERIFY(loaded.systemPrompts.first().content.isEmpty());
    QCOMPARE(loaded.fetchedModels, QStringList() << QStringLiteral("vendor-new-model"));
    QVERIFY(loaded.logRequestResponseContent);
    QCOMPARE(loaded.inputPricePerMillion, 1.25);
    QCOMPARE(loaded.outputPricePerMillion, 3.5);
}

void ModelAdvancedRequestTests::
parsesUsageFinishReasonAndStructuredCitations()
{
    QJsonObject usage;
    usage.insert(QStringLiteral("prompt_tokens"), 12);
    usage.insert(QStringLiteral("completion_tokens"), 7);
    usage.insert(QStringLiteral("total_tokens"), 19);
    QJsonObject completionDetails;
    completionDetails.insert(QStringLiteral("reasoning_tokens"), 3);
    usage.insert(QStringLiteral("completion_tokens_details"), completionDetails);
    QJsonObject promptDetails;
    promptDetails.insert(QStringLiteral("cached_tokens"), 4);
    usage.insert(QStringLiteral("prompt_tokens_details"), promptDetails);

    QJsonObject citation;
    citation.insert(QStringLiteral("url"), QStringLiteral("https://example.test/source"));
    citation.insert(QStringLiteral("title"), QStringLiteral("Example Source"));
    QJsonObject message;
    message.insert(QStringLiteral("annotations"), QJsonArray() << citation);
    QJsonObject choice;
    choice.insert(QStringLiteral("finish_reason"), QStringLiteral("tool_calls"));
    choice.insert(QStringLiteral("message"), message);
    QJsonObject root;
    root.insert(QStringLiteral("usage"), usage);
    root.insert(QStringLiteral("choices"), QJsonArray() << choice);

    ModelRequestTelemetry telemetry;
    updateModelMetadataFromJson(root, &telemetry);
    QCOMPARE(telemetry.usage.inputTokens, qint64(12));
    QCOMPARE(telemetry.usage.outputTokens, qint64(7));
    QCOMPARE(telemetry.usage.totalTokens, qint64(19));
    QCOMPARE(telemetry.usage.reasoningTokens, qint64(3));
    QCOMPARE(telemetry.usage.cacheHitTokens, qint64(4));
    QCOMPARE(telemetry.finishReason, QStringLiteral("tool_calls"));
    QCOMPARE(telemetry.citations.size(), 1);
    QCOMPARE(
        telemetry.citations.first().url,
        QStringLiteral("https://example.test/source")
    );
}

void ModelAdvancedRequestTests::mergesAnthropicStreamingMetadata()
{
    QJsonObject startUsage;
    startUsage.insert(QStringLiteral("input_tokens"), 31);
    startUsage.insert(QStringLiteral("cache_read_input_tokens"), 9);
    QJsonObject message;
    message.insert(QStringLiteral("usage"), startUsage);
    QJsonObject messageStart;
    messageStart.insert(QStringLiteral("type"), QStringLiteral("message_start"));
    messageStart.insert(QStringLiteral("message"), message);

    QJsonObject deltaUsage;
    deltaUsage.insert(QStringLiteral("output_tokens"), 14);
    QJsonObject delta;
    delta.insert(QStringLiteral("stop_reason"), QStringLiteral("tool_use"));
    QJsonObject messageDelta;
    messageDelta.insert(QStringLiteral("type"), QStringLiteral("message_delta"));
    messageDelta.insert(QStringLiteral("usage"), deltaUsage);
    messageDelta.insert(QStringLiteral("delta"), delta);

    ModelRequestTelemetry telemetry;
    updateModelMetadataFromJson(messageStart, &telemetry);
    updateModelMetadataFromJson(messageDelta, &telemetry);
    QCOMPARE(telemetry.usage.inputTokens, qint64(31));
    QCOMPARE(telemetry.usage.outputTokens, qint64(14));
    QCOMPARE(telemetry.usage.cacheHitTokens, qint64(9));
    QCOMPARE(telemetry.finishReason, QStringLiteral("tool_use"));
}

void ModelAdvancedRequestTests::redactsOnlySensitiveRequestFields()
{
    QJsonObject nested;
    nested.insert(QStringLiteral("api_key"), QStringLiteral("secret-value"));
    nested.insert(QStringLiteral("max_tokens"), 2048);
    QJsonObject request;
    request.insert(QStringLiteral("Authorization"), QStringLiteral("Bearer secret"));
    request.insert(QStringLiteral("token_usage"), 99);
    request.insert(QStringLiteral("nested"), nested);

    const QJsonObject redacted = redactedModelRequestJson(request);
    QCOMPARE(
        redacted.value(QStringLiteral("Authorization")).toString(),
        QStringLiteral("***REDACTED***")
    );
    QCOMPARE(redacted.value(QStringLiteral("token_usage")).toInt(), 99);
    const QJsonObject redactedNested = redacted.value(QStringLiteral("nested")).toObject();
    QCOMPARE(
        redactedNested.value(QStringLiteral("api_key")).toString(),
        QStringLiteral("***REDACTED***")
    );
    QCOMPARE(redactedNested.value(QStringLiteral("max_tokens")).toInt(), 2048);
    QCOMPARE(
        redactedModelLogText(QStringLiteral("Authorization: Bearer abc.def-123")),
        QStringLiteral("Authorization: Bearer ***REDACTED***")
    );
    QVERIFY(!redactedModelLogText(QStringLiteral(
        "{\"error\":{\"api_key\":\"private-value\"}}"
    )).contains(QStringLiteral("private-value")));
}

void ModelAdvancedRequestTests::requestLogsDefaultToMetadataOnly()
{
    ModelResult result;
    result.text = QStringLiteral("private answer");
    result.rawResponse = QByteArrayLiteral(
        "{\"answer\":\"private response\",\"api_key\":\"secret\"}"
    );
    result.telemetry.providerId = QStringLiteral("openai");
    result.telemetry.modelId = QStringLiteral("openai:test-model");
    result.telemetry.httpStatusCode = 200;
    result.telemetry.actualRequest.insert(
        QStringLiteral("model"),
        QStringLiteral("test-model")
    );
    result.telemetry.actualRequest.insert(
        QStringLiteral("temperature"),
        0.4
    );
    QJsonObject message;
    message.insert(QStringLiteral("role"), QStringLiteral("user"));
    message.insert(QStringLiteral("content"), QStringLiteral("private question"));
    result.telemetry.actualRequest.insert(
        QStringLiteral("messages"),
        QJsonArray() << message
    );

    const QJsonObject metadataOnly = modelRequestLogEntry(result, false);
    QVERIFY(metadataOnly.contains(QStringLiteral("request_metadata")));
    QVERIFY(!metadataOnly.contains(QStringLiteral("actual_request")));
    QVERIFY(!metadataOnly.contains(QStringLiteral("raw_response")));
    const QString metadataText = QString::fromUtf8(
        QJsonDocument(metadataOnly).toJson(QJsonDocument::Compact)
    );
    QVERIFY(!metadataText.contains(QStringLiteral("private question")));
    QVERIFY(!metadataText.contains(QStringLiteral("private response")));

    const QJsonObject contentLog = modelRequestLogEntry(result, true);
    QVERIFY(contentLog.contains(QStringLiteral("actual_request")));
    QVERIFY(contentLog.contains(QStringLiteral("raw_response")));
    const QString contentText = QString::fromUtf8(
        QJsonDocument(contentLog).toJson(QJsonDocument::Compact)
    );
    QVERIFY(contentText.contains(QStringLiteral("private question")));
    QVERIFY(contentText.contains(QStringLiteral("private response")));
    QVERIFY(!contentText.contains(QStringLiteral("\"secret\"")));
}

QTEST_MAIN(ModelAdvancedRequestTests)
#include "model_advanced_request_tests.moc"
