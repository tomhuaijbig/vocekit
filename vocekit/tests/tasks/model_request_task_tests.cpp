#include <QtTest>

#include "../../src/tasks/model_request_task.h"

class FakeTaskModelProvider : public IModelProvider
{
public:
    QString id() const override
    {
        return QStringLiteral("fake-model");
    }

    ProviderCheckResult checkConfiguration(
        const CancellationToken &) const override
    {
        ProviderCheckResult result;
        result.available = true;
        return result;
    }

    ModelResult complete(
        const ModelRequest &request,
        const ModelDeltaCallback &onDelta,
        const CancellationToken &cancellation) override
    {
        lastRequest = request;
        lastCancellationWasValid = cancellation.isValid();
        lastCancellationExecutionId = cancellation.executionId();
        lastCancellationWasRequested =
            cancellation.isCancellationRequested();
        if (onDelta) {
            onDelta(QStringLiteral("delta"));
        }

        ModelResult result;
        result.executionId = request.executionId;
        result.text = QStringLiteral("completed");
        result.durationMs = 42;
        return result;
    }

    ModelRequest lastRequest;
    bool lastCancellationWasValid = false;
    bool lastCancellationWasRequested = false;
    ExecutionId lastCancellationExecutionId;
};

class ModelRequestTaskTests : public QObject
{
    Q_OBJECT

private slots:
    void buildsRequestAndReturnsMetadata()
    {
        QSharedPointer<FakeTaskModelProvider> provider(
            new FakeTaskModelProvider
        );
        ModelRequestTaskRequest request;
        request.modelId = QStringLiteral("deepseek-v4-flash");
        request.systemPrompt = QStringLiteral("system");
        request.userPrompt = QStringLiteral("user");
        request.stream = true;
        request.useSystemProxy = true;
        request.networkPolicy = QStringLiteral("direct");

        QString delta;
        const ModelRequestTaskResult result = runModelRequestTask(
            request,
            provider,
            [&delta](const QString &chunk) {
                delta += chunk;
            }
        );

        QCOMPARE(result.text, QStringLiteral("completed"));
        QCOMPARE(result.durationMs, qint64(42));
        QCOMPARE(result.promptVersion.size(), 12);
        QVERIFY(result.executionId.isValid());
        QCOMPARE(delta, QStringLiteral("delta"));
        QCOMPARE(provider->lastRequest.modelId, request.modelId);
        QCOMPARE(provider->lastRequest.systemPrompt, request.systemPrompt);
        QCOMPARE(provider->lastRequest.userPrompt, request.userPrompt);
        QCOMPARE(provider->lastRequest.stream, true);
        QCOMPARE(provider->lastRequest.network.globalUseSystemProxy, true);
        QCOMPARE(provider->lastRequest.network.networkPolicy, QStringLiteral("direct"));
        QVERIFY(provider->lastRequest.executionId.isValid());
        QVERIFY(provider->lastCancellationWasValid);
    }

    void reportsMissingProvider()
    {
        ModelRequestTaskRequest request;
        request.systemPrompt = QStringLiteral("system");

        const ModelRequestTaskResult result =
            runModelRequestTask(request, QSharedPointer<IModelProvider>());

        QVERIFY(result.text.isEmpty());
        QVERIFY(!result.errorMessage.isEmpty());
        QCOMPARE(result.promptVersion.size(), 12);
    }

    void forwardsCallerCancellationToken()
    {
        QSharedPointer<FakeTaskModelProvider> provider(
            new FakeTaskModelProvider
        );
        CancellationSource cancellation;
        ModelRequestTaskRequest request;
        request.modelId = QStringLiteral("deepseek-v4-flash");
        request.systemPrompt = QStringLiteral("system");
        request.userPrompt = QStringLiteral("user");
        request.cancellation = cancellation.token();

        const ModelRequestTaskResult result =
            runModelRequestTask(request, provider);

        QCOMPARE(result.executionId, cancellation.executionId());
        QCOMPARE(
            provider->lastRequest.executionId,
            cancellation.executionId()
        );
        QCOMPARE(
            provider->lastCancellationExecutionId,
            cancellation.executionId()
        );
        QVERIFY(provider->lastCancellationWasValid);
        QVERIFY(!provider->lastCancellationWasRequested);
    }

    void forwardsCallerCancellationState()
    {
        QSharedPointer<FakeTaskModelProvider> provider(
            new FakeTaskModelProvider
        );
        CancellationSource cancellation;
        cancellation.cancel();
        ModelRequestTaskRequest request;
        request.modelId = QStringLiteral("deepseek-v4-flash");
        request.systemPrompt = QStringLiteral("system");
        request.userPrompt = QStringLiteral("user");
        request.cancellation = cancellation.token();

        const ModelRequestTaskResult result =
            runModelRequestTask(request, provider);

        QVERIFY(provider->lastCancellationWasRequested);
        QVERIFY(result.cancelled);
    }
};

QTEST_MAIN(ModelRequestTaskTests)
#include "model_request_task_tests.moc"
