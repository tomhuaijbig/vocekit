#include <QtTest>

#include "../../src/providers/provider_registry.h"

class FakeSpeechProvider : public ISpeechProvider
{
public:
    explicit FakeSpeechProvider(const QString &providerId)
        : m_id(providerId)
    {
    }

    QString id() const override
    {
        return m_id;
    }

    ProviderCheckResult checkConfiguration(
        const CancellationToken &cancellation) const override
    {
        checkCancellationWasRequested =
            cancellation.isCancellationRequested();
        ProviderCheckResult result;
        result.available = true;
        return result;
    }

    SpeechRecognitionResult recognize(
        const SpeechRecognitionRequest &,
        const CancellationToken &) override
    {
        SpeechRecognitionResult result;
        result.text = QStringLiteral("recognized");
        return result;
    }

    void refreshConfiguration() override
    {
        ++refreshCount;
    }

    int refreshCount = 0;
    mutable bool checkCancellationWasRequested = false;

private:
    QString m_id;
};

class FakeModelProvider : public IModelProvider
{
public:
    explicit FakeModelProvider(const QString &providerId)
        : m_id(providerId)
    {
    }

    QString id() const override
    {
        return m_id;
    }

    ProviderCheckResult checkConfiguration(
        const CancellationToken &cancellation) const override
    {
        checkCancellationWasRequested =
            cancellation.isCancellationRequested();
        ProviderCheckResult result;
        result.available = true;
        return result;
    }

    ModelResult complete(
        const ModelRequest &,
        const ModelDeltaCallback &,
        const CancellationToken &) override
    {
        ModelResult result;
        result.text = QStringLiteral("completed");
        return result;
    }

    mutable bool checkCancellationWasRequested = false;

private:
    QString m_id;
};

class ProviderRegistryTests : public QObject
{
    Q_OBJECT

private slots:
    void routesProvidersByStableId()
    {
        ProviderRegistry registry;
        QSharedPointer<FakeSpeechProvider> speech(
            new FakeSpeechProvider(QStringLiteral("fake-speech"))
        );
        QSharedPointer<FakeModelProvider> model(
            new FakeModelProvider(QStringLiteral("fake-model"))
        );

        QVERIFY(registry.addSpeechProvider(speech));
        QVERIFY(registry.addModelProvider(model));
        QCOMPARE(
            registry.speechProvider(QStringLiteral("fake-speech"))->id(),
            QStringLiteral("fake-speech")
        );
        QCOMPARE(
            registry.modelProvider(QStringLiteral("fake-model"))->id(),
            QStringLiteral("fake-model")
        );
        QVERIFY(
            registry.speechProvider(QStringLiteral("missing")).isNull()
        );
        QVERIFY(
            registry.modelProvider(QStringLiteral("missing")).isNull()
        );
    }

    void rejectsEmptyProviderIds()
    {
        ProviderRegistry registry;
        QVERIFY(!registry.addSpeechProvider(
            QSharedPointer<ISpeechProvider>(
                new FakeSpeechProvider(QString())
            )
        ));
    }

    void reportsUnknownProvider()
    {
        ProviderRegistry registry;
        const ProviderCheckResult result =
            registry.checkSpeechProvider(QStringLiteral("missing"));

        QVERIFY(!result.available);
        QCOMPARE(
            result.error.code,
            QStringLiteral("provider.not_found")
        );
    }

    void refreshesRegisteredProviders()
    {
        ProviderRegistry registry;
        QSharedPointer<FakeSpeechProvider> speech(
            new FakeSpeechProvider(QStringLiteral("fake-speech"))
        );
        QVERIFY(registry.addSpeechProvider(speech));

        registry.refreshConfiguration();

        QCOMPARE(speech->refreshCount, 1);
    }

    void forwardsCancellationToProviderChecks()
    {
        ProviderRegistry registry;
        QSharedPointer<FakeSpeechProvider> speech(
            new FakeSpeechProvider(QStringLiteral("fake-speech"))
        );
        QSharedPointer<FakeModelProvider> model(
            new FakeModelProvider(QStringLiteral("fake-model"))
        );
        QVERIFY(registry.addSpeechProvider(speech));
        QVERIFY(registry.addModelProvider(model));

        CancellationSource cancellation;
        cancellation.cancel();
        registry.checkSpeechProvider(
            QStringLiteral("fake-speech"),
            cancellation.token()
        );
        registry.checkModelProvider(
            QStringLiteral("fake-model"),
            cancellation.token()
        );

        QVERIFY(speech->checkCancellationWasRequested);
        QVERIFY(model->checkCancellationWasRequested);
    }
};

QTEST_MAIN(ProviderRegistryTests)
#include "provider_registry_tests.moc"
