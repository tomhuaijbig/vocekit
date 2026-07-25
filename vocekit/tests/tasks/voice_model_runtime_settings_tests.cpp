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
