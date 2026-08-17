#include <QtTest>

#include "../../src/config/app_settings_data.h"
#include "../../src/tasks/voice_model_runtime_settings.h"

class VoiceModelRuntimeSettingsTests : public QObject
{
    Q_OBJECT

private slots:
    void buildsRuntimeSettingsFromTypedSnapshot()
    {
        AppSettingsData settings;
        settings.dictatePolishEnabled = true;
        settings.useSystemProxy = true;
        FunctionSettings function;
        function.id = QStringLiteral("dictate");
        function.modelId = QStringLiteral("model-a");
        function.sampling.temperatureEnabled = true;
        function.sampling.temperature = 0.75;
        function.sampling.topPEnabled = true;
        function.sampling.topP = 0.6;
        settings.functions.append(function);

        const VoiceModelRuntimeSettings runtime =
            buildVoiceModelRuntimeSettings(
                settings,
                QStringLiteral("dictate"),
                QStringLiteral(" custom prompt ")
            );

        QCOMPARE(runtime.defaultModel, QStringLiteral("model-a"));
        QCOMPARE(runtime.systemPrompt, QStringLiteral("custom prompt"));
        QCOMPARE(runtime.useSystemProxy, true);
        QCOMPARE(runtime.dictatePolishEnabled, true);
        QVERIFY(runtime.sampling.temperatureEnabled);
        QCOMPARE(runtime.sampling.temperature, 0.75);
        QVERIFY(runtime.sampling.topPEnabled);
        QCOMPARE(runtime.sampling.topP, 0.6);
    }

    void suppliesModeSpecificFallbackPrompt()
    {
        const AppSettingsData settings;
        const VoiceModelRuntimeSettings runtime =
            buildVoiceModelRuntimeSettings(
                settings,
                QStringLiteral("translate"),
                QString()
            );

        QVERIFY(runtime.systemPrompt.contains(QString::fromUtf8("翻译")));
    }
};

QTEST_MAIN(VoiceModelRuntimeSettingsTests)
#include "voice_model_runtime_settings_tests.moc"
