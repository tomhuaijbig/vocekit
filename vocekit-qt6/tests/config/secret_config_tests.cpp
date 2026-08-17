#include <QtTest>

#include "../../src/config/baidu_sample_parser.h"
#include "../../src/config/secret_config.h"
#include "../../src/config/secret_store.h"
#include "../../src/file_utils.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

class SecretConfigTests : public QObject
{
    Q_OBJECT

private slots:
    void savesAndLoadsSpeechAndModelSecrets()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        SecretStore store(QDir(temp.path()).filePath(QStringLiteral("config/secrets.json")));

        SecretConfig secrets;
        secrets.baiduApiKey = QStringLiteral("baidu-key");
        secrets.baiduSecretKey = QStringLiteral("baidu-secret");
        secrets.xfyunAppId = QStringLiteral("xfyun-app");
        secrets.xfyunApiKey = QStringLiteral("xfyun-key");
        secrets.xfyunApiSecret = QStringLiteral("xfyun-secret");
        secrets.customOcrUrl = QStringLiteral("https://ocr.example.test");

        CustomModelProfile profile;
        profile.id = QStringLiteral("My Model!");
        profile.name = QStringLiteral("我的模型");
        profile.url = QStringLiteral("https://model.example.test/v1/chat/completions");
        profile.apiKey = QStringLiteral("model-key");
        profile.model = QStringLiteral("model-a");
        profile.temperatureEnabled = true;
        profile.temperature = 0.75;
        profile.topPEnabled = true;
        profile.topP = 0.9;
        secrets.customModels.append(profile);

        QVERIFY(store.save(secrets));

        const SecretConfig loaded = store.load();
        QVERIFY(loaded.hasBaidu());
        QVERIFY(loaded.hasXfyun());
        QCOMPARE(loaded.customOcrUrl, QStringLiteral("https://ocr.example.test"));

        const QVector<CustomModelProfile> profiles = loaded.effectiveCustomModels();
        QCOMPARE(profiles.size(), 1);
        QCOMPARE(profiles.constFirst().id, QStringLiteral("MyModel"));
        QCOMPARE(profiles.constFirst().name, QStringLiteral("我的模型"));
        QCOMPARE(profiles.constFirst().apiKey, QStringLiteral("model-key"));
        QVERIFY(profiles.constFirst().temperatureEnabled);
        QCOMPARE(profiles.constFirst().temperature, 0.75);
        QVERIFY(profiles.constFirst().topPEnabled);
        QCOMPARE(profiles.constFirst().topP, 0.9);
        QVERIFY(loaded.hasCustomModel());
    }

    void savesAndLoadsProviderBaseUrls()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const QString path = QDir(temp.path()).filePath(QStringLiteral("config/secrets.json"));
        SecretStore store(path);

        SecretConfig secrets;
        secrets.openaiBaseUrl = QString::fromUtf8("https://\xE4\xBE\x8B\xE5\xAD\x90.example/openai");
        secrets.anthropicBaseUrl = QStringLiteral("https://gateway.example.test/v1");
        QVERIFY(store.save(secrets));

        const SecretConfig loaded = store.load();
        QCOMPARE(loaded.openaiBaseUrl, secrets.openaiBaseUrl);
        QCOMPARE(loaded.anthropicBaseUrl, secrets.anthropicBaseUrl);

        QJsonObject root;
        QVERIFY(readJsonObjectFile(path, &root));
        QCOMPARE(root.value(QStringLiteral("openai_base_url")).toString(), secrets.openaiBaseUrl);
        QCOMPARE(root.value(QStringLiteral("anthropic_base_url")).toString(), secrets.anthropicBaseUrl);
    }

    void loadsLegacySecretsWithoutProviderBaseUrls()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const QString path = QDir(temp.path()).filePath(QStringLiteral("config/secrets.json"));
        SecretStore store(path);

        QJsonObject root;
        root.insert(QStringLiteral("openai_api_key"), QStringLiteral("legacy-openai-key"));
        root.insert(QStringLiteral("anthropic_api_key"), QStringLiteral("legacy-anthropic-key"));
        QVERIFY(writeBytesAtomically(path, QJsonDocument(root).toJson(QJsonDocument::Indented)));

        const SecretConfig loaded = store.load();
        QCOMPARE(loaded.openaiApiKey, QStringLiteral("legacy-openai-key"));
        QCOMPARE(loaded.anthropicApiKey, QStringLiteral("legacy-anthropic-key"));
        QVERIFY(loaded.openaiBaseUrl.isEmpty());
        QVERIFY(loaded.anthropicBaseUrl.isEmpty());
    }

    void loadsLegacySingleCustomModelFields()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const QString path = QDir(temp.path()).filePath(QStringLiteral("config/secrets.json"));
        SecretStore store(path);

        QJsonObject root;
        root.insert(QStringLiteral("custom_model_url"), QStringLiteral("https://legacy.example.test"));
        root.insert(QStringLiteral("custom_model_api_key"), QStringLiteral("legacy-key"));
        root.insert(QStringLiteral("custom_model_name"), QStringLiteral("legacy-model"));
        QVERIFY(writeBytesAtomically(path, QJsonDocument(root).toJson(QJsonDocument::Indented)));

        const SecretConfig loaded = store.load();
        const QVector<CustomModelProfile> profiles = loaded.effectiveCustomModels();
        QCOMPARE(profiles.size(), 1);
        QCOMPARE(profiles.constFirst().id, QStringLiteral("model"));
        QCOMPARE(profiles.constFirst().name, QStringLiteral("自定义大模型"));
        QCOMPARE(profiles.constFirst().url, QStringLiteral("https://legacy.example.test"));
        QCOMPARE(profiles.constFirst().apiKey, QStringLiteral("legacy-key"));
        QCOMPARE(profiles.constFirst().model, QStringLiteral("legacy-model"));
        QVERIFY(!profiles.constFirst().temperatureEnabled);
        QVERIFY(!profiles.constFirst().topPEnabled);
    }

    void omitsDisabledOrInvalidSamplingParameters()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const QString path = QDir(temp.path()).filePath(
            QStringLiteral("config/secrets.json")
        );
        SecretStore store(path);

        SecretConfig secrets;
        CustomModelProfile profile;
        profile.id = QStringLiteral("default-sampling");
        profile.url = QStringLiteral("https://model.example.test");
        profile.temperatureEnabled = false;
        profile.temperature = 1.4;
        profile.topPEnabled = true;
        profile.topP = 2.0;
        secrets.customModels.append(profile);
        QVERIFY(store.save(secrets));

        QJsonObject root;
        QVERIFY(readJsonObjectFile(path, &root));
        const QJsonArray models = root.value(
            QStringLiteral("custom_models")
        ).toArray();
        QCOMPARE(models.size(), 1);
        const QJsonObject saved = models.first().toObject();
        QVERIFY(!saved.contains(QStringLiteral("temperature")));
        QVERIFY(!saved.contains(QStringLiteral("top_p")));

        const SecretConfig loaded = store.load();
        QCOMPARE(loaded.customModels.size(), 1);
        QVERIFY(!loaded.customModels.constFirst().temperatureEnabled);
        QVERIFY(!loaded.customModels.constFirst().topPEnabled);
    }

    void findsCustomModelByProviderId()
    {
        SecretConfig secrets;
        CustomModelProfile first;
        first.id = QStringLiteral("alpha");
        first.url = QStringLiteral("https://alpha.example.test");
        CustomModelProfile second;
        second.id = QStringLiteral("beta");
        second.url = QStringLiteral("https://beta.example.test");
        secrets.customModels << first << second;

        QCOMPARE(
            secrets.customModelProfileForProviderId(QStringLiteral("custom:beta")).url,
            QStringLiteral("https://beta.example.test")
        );
        QCOMPARE(
            secrets.customModelProfileForProviderId(QString()).url,
            QStringLiteral("https://alpha.example.test")
        );
        QVERIFY(secrets.customModelProfileForProviderId(QStringLiteral("missing")).url.isEmpty());
    }

    void extractsBaiduCredentialsFromSampleCode()
    {
        const QString sample = QStringLiteral(
            "curl_easy_setopt(curl, CURLOPT_URL, "
            "\"https://aip.baidubce.com/oauth/2.0/token?client_id=BAIDU_TEST_CLIENT_ID_123&client_secret=BAIDU_TEST_SECRET_456&grant_type=client_credentials\");"
        );

        QString apiKey;
        QString secretKey;
        QVERIFY(extractBaiduCredentialsFromSampleCode(sample, &apiKey, &secretKey));
        QCOMPARE(apiKey, QStringLiteral("BAIDU_TEST_CLIENT_ID_123"));
        QCOMPARE(secretKey, QStringLiteral("BAIDU_TEST_SECRET_456"));
    }

    void extractsEscapedBaiduCredentials()
    {
        const QString sample = QStringLiteral(
            "https:\\/\\/aip.baidubce.com\\/oauth\\/2.0\\/token?client_id=abc%2D123\\u0026client_secret=sec%2D456\\u0026grant_type=client_credentials"
        );

        QString apiKey;
        QString secretKey;
        QVERIFY(extractBaiduCredentialsFromSampleCode(sample, &apiKey, &secretKey));
        QCOMPARE(apiKey, QStringLiteral("abc-123"));
        QCOMPARE(secretKey, QStringLiteral("sec-456"));
    }
};

QTEST_MAIN(SecretConfigTests)

#include "secret_config_tests.moc"
