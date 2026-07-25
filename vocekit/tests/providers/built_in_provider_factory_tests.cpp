#include <QtTest>

#include "../../src/config/app_settings_defaults.h"
#include "../../src/providers/built_in_provider_factory.h"
#include "../../src/providers/baidu_speech_provider.h"
#include "../../src/providers/claude_model_provider.h"
#include "../../src/providers/custom_speech_provider.h"
#include "../../src/providers/deepseek_model_provider.h"
#include "../../src/providers/openai_compatible_model_provider.h"
#include "../../src/providers/xfyun_speech_provider.h"
#include "../../src/tasks/cancellation_token.h"

#include <QDir>
#include <QFileInfo>

class BuiltInProviderFactoryTests : public QObject
{
    Q_OBJECT

private slots:
    void registersBuiltInAdapters()
    {
        ProviderRegistry registry;
        registerBuiltInProviders(&registry);

        QVERIFY(!registry.speechProvider(speechProviderBaidu()).isNull());
        QVERIFY(!registry.speechProvider(speechProviderXfyun()).isNull());
        QVERIFY(!registry.speechProvider(speechProviderCustom()).isNull());
        QVERIFY(!registry.modelProvider(QStringLiteral("deepseek")).isNull());
        QVERIFY(!registry.modelProvider(QStringLiteral("openai")).isNull());
        QVERIFY(!registry.modelProvider(QStringLiteral("claude")).isNull());
        QVERIFY(!registry.modelProvider(QStringLiteral("custom")).isNull());
    }

    void normalizesSpeechProviderIds()
    {
        const QSharedPointer<ISpeechProvider> provider =
            createBuiltInSpeechProvider(QStringLiteral("bad"));

        QCOMPARE(provider->id(), speechProviderBaidu());
    }

    void cancelledSpeechDoesNotReachNetwork()
    {
        const QSharedPointer<ISpeechProvider> provider =
            createBuiltInSpeechProvider(speechProviderBaidu());
        CancellationSource source;
        source.cancel();

        SpeechRecognitionRequest request;
        request.audioData = QByteArrayLiteral("pcm");

        const SpeechRecognitionResult result =
            provider->recognize(request, source.token());

        QCOMPARE(result.error.code, QStringLiteral("task.cancelled"));
        QVERIFY(result.text.isEmpty());
    }

    void cancelledModelDoesNotReachNetwork()
    {
        const QSharedPointer<IModelProvider> provider =
            createBuiltInModelProvider(QStringLiteral("deepseek"));
        CancellationSource source;
        source.cancel();

        ModelRequest request;
        request.modelId = QStringLiteral("deepseek-v4-flash");
        request.systemPrompt = QStringLiteral("system");
        request.userPrompt = QStringLiteral("user");

        const ModelResult result =
            provider->complete(request, ModelDeltaCallback(), source.token());

        QCOMPARE(result.error.code, QStringLiteral("task.cancelled"));
        QVERIFY(result.text.isEmpty());
    }

    void deepSeekFactoryUsesIndependentProvider()
    {
        const QSharedPointer<IModelProvider> provider =
            createBuiltInModelProvider(QStringLiteral("deepseek"));

        QVERIFY(
            dynamic_cast<DeepSeekModelProvider *>(provider.data()) != nullptr
        );
    }

    void openAiCompatibleFactoriesUseIndependentProvider()
    {
        const QStringList providerIds = QStringList()
            << QStringLiteral("openai")
            << QStringLiteral("custom")
            << QStringLiteral("custom:office");
        for (const QString &providerId : providerIds) {
            const QSharedPointer<IModelProvider> provider =
                createBuiltInModelProvider(providerId);
            QVERIFY2(
                dynamic_cast<OpenAiCompatibleModelProvider *>(
                    provider.data()
                ) != nullptr,
                qPrintable(providerId)
            );
        }
    }

    void claudeFactoryUsesIndependentProvider()
    {
        const QSharedPointer<IModelProvider> provider =
            createBuiltInModelProvider(QStringLiteral("claude"));

        QVERIFY(
            dynamic_cast<ClaudeModelProvider *>(provider.data()) != nullptr
        );
    }

    void customSpeechFactoryUsesIndependentProvider()
    {
        const QSharedPointer<ISpeechProvider> provider =
            createBuiltInSpeechProvider(speechProviderCustom());

        QVERIFY(
            dynamic_cast<CustomSpeechProvider *>(provider.data()) != nullptr
        );
    }

    void baiduSpeechFactoryUsesIndependentProvider()
    {
        const QSharedPointer<ISpeechProvider> provider =
            createBuiltInSpeechProvider(speechProviderBaidu());

        QVERIFY(
            dynamic_cast<BaiduSpeechProvider *>(provider.data()) != nullptr
        );
    }

    void xfyunSpeechFactoryUsesIndependentProvider()
    {
        const QSharedPointer<ISpeechProvider> provider =
            createBuiltInSpeechProvider(speechProviderXfyun());

        QVERIFY(
            dynamic_cast<XfyunSpeechProvider *>(provider.data()) != nullptr
        );
    }

    void legacyApiClientImplementationIsRemoved()
    {
        const QString utilityPath =
            QFINDTESTDATA("../../src/api/api_client_utils.cpp");
        QVERIFY2(!utilityPath.isEmpty(), "找不到 API 公共工具源文件");
        const QDir apiDirectory = QFileInfo(utilityPath).absoluteDir();

        QVERIFY(!apiDirectory.exists(QStringLiteral("api_client.cpp")));
        QVERIFY(!apiDirectory.exists(QStringLiteral("api_client.h")));
    }
};

QTEST_MAIN(BuiltInProviderFactoryTests)
#include "built_in_provider_factory_tests.moc"
