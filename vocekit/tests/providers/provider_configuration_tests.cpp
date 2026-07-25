#include <QtTest>

#include "../../src/providers/provider_configuration.h"

class ProviderConfigurationTests : public QObject
{
    Q_OBJECT

private slots:
    void validatesSpeechProviderSecrets()
    {
        SecretConfig secrets;
        QVERIFY(!speechProviderConfigurationErrorForSecrets(
            secrets,
            QStringLiteral("baidu")
        ).isEmpty());
        secrets.baiduApiKey = QStringLiteral("key");
        secrets.baiduSecretKey = QStringLiteral("secret");
        QVERIFY(speechProviderConfigurationErrorForSecrets(
            secrets,
            QStringLiteral("baidu")
        ).isEmpty());

        QVERIFY(!speechProviderConfigurationErrorForSecrets(
            secrets,
            QStringLiteral("xfyun")
        ).isEmpty());
        secrets.xfyunAppId = QStringLiteral("app");
        secrets.xfyunApiKey = QStringLiteral("key");
        secrets.xfyunApiSecret = QStringLiteral("secret");
        QVERIFY(speechProviderConfigurationErrorForSecrets(
            secrets,
            QStringLiteral("xfyun")
        ).isEmpty());

        QVERIFY(!speechProviderConfigurationErrorForSecrets(
            secrets,
            QStringLiteral("custom")
        ).isEmpty());
        secrets.customSpeechUrl = QStringLiteral("https://example.com/asr");
        QVERIFY(speechProviderConfigurationErrorForSecrets(
            secrets,
            QStringLiteral("custom")
        ).isEmpty());
    }

    void validatesModelProviderSecrets()
    {
        SecretConfig secrets;
        QVERIFY(!isModelProviderConfiguredForSecrets(
            secrets,
            QStringLiteral("deepseek-v4-flash")
        ));
        secrets.deepseekApiKey = QStringLiteral("deepseek-key");
        QVERIFY(isModelProviderConfiguredForSecrets(
            secrets,
            QStringLiteral("deepseek-v4-flash")
        ));

        secrets.openaiApiKey = QStringLiteral("openai-key");
        QVERIFY(isModelProviderConfiguredForSecrets(
            secrets,
            QStringLiteral("openai:gpt-5.5")
        ));
        secrets.anthropicApiKey = QStringLiteral("claude-key");
        QVERIFY(isModelProviderConfiguredForSecrets(
            secrets,
            QStringLiteral("claude:claude-opus-4-8")
        ));

        CustomModelProfile profile;
        profile.id = QStringLiteral("local");
        profile.url = QStringLiteral("https://example.com/v1");
        secrets.customModels.append(profile);
        QVERIFY(isModelProviderConfiguredForSecrets(
            secrets,
            QStringLiteral("custom:local")
        ));
        QVERIFY(!isModelProviderConfiguredForSecrets(
            secrets,
            QStringLiteral("custom:missing")
        ));
    }
};

QTEST_MAIN(ProviderConfigurationTests)
#include "provider_configuration_tests.moc"
