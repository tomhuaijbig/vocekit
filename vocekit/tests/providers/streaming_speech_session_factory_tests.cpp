#include <QtTest>

#include "../../src/providers/streaming_speech_session_factory.h"

class StubStreamingSpeechSession : public IStreamingSpeechSession
{
public:
    explicit StubStreamingSpeechSession(const QString &kind)
        : providerKind(kind)
    {
    }

    bool start(QString *) override { return true; }
    bool pushAudio(const QByteArray &) override { return true; }
    void finish() override {}
    void cancel() override {}
    StreamingSpeechState state() const override
    {
        return StreamingSpeechState::Idle;
    }

    QString providerKind;
};

namespace {

SecretConfig completeSecrets()
{
    SecretConfig secrets;
    secrets.xfyunAppId = QStringLiteral("xf-app");
    secrets.xfyunApiKey = QStringLiteral("xf-key");
    secrets.xfyunApiSecret = QStringLiteral("xf-secret");
    secrets.baiduAppId = QStringLiteral("123456");
    secrets.baiduApiKey = QStringLiteral("baidu-key");
    secrets.baiduSecretKey = QStringLiteral("batch-secret");
    return secrets;
}

StreamingSpeechSessionFactoryDependencies dependenciesFor(
    const SecretConfig &secrets,
    int *xfyunCount,
    int *baiduCount,
    int *windowsCount = nullptr,
    int *secretLoadCount = nullptr
)
{
    StreamingSpeechSessionFactoryDependencies dependencies;
    dependencies.loadSecrets = [secrets, secretLoadCount]() {
        if (secretLoadCount) {
            ++*secretLoadCount;
        }
        return secrets;
    };
    dependencies.createXfyun = [xfyunCount](
        const SecretConfig &,
        const StreamingSpeechSessionRequest &,
        const StreamingSpeechCallbacks &
    ) {
        ++*xfyunCount;
        return QSharedPointer<IStreamingSpeechSession>(
            new StubStreamingSpeechSession(QStringLiteral("xfyun"))
        );
    };
    dependencies.createBaidu = [baiduCount](
        const SecretConfig &,
        const StreamingSpeechSessionRequest &,
        const StreamingSpeechCallbacks &
    ) {
        ++*baiduCount;
        return QSharedPointer<IStreamingSpeechSession>(
            new StubStreamingSpeechSession(QStringLiteral("baidu"))
        );
    };
    dependencies.createWindows = [windowsCount](
        const StreamingSpeechSessionRequest &,
        const StreamingSpeechCallbacks &
    ) {
        if (windowsCount) {
            ++*windowsCount;
        }
        return QSharedPointer<IStreamingSpeechSession>(
            new StubStreamingSpeechSession(QStringLiteral("windows-local"))
        );
    };
    return dependencies;
}

} // namespace

class StreamingSpeechSessionFactoryTests : public QObject
{
    Q_OBJECT

private slots:
    void selectsXfyunAndBaiduOnly()
    {
        int xfyunCount = 0;
        int baiduCount = 0;
        const StreamingSpeechSessionFactoryDependencies dependencies =
            dependenciesFor(completeSecrets(), &xfyunCount, &baiduCount);
        StreamingSpeechSessionRequest request;

        request.provider = QStringLiteral("xfyun");
        const StreamingSpeechSessionCreation xfyun =
            createStreamingSpeechSession(
                request,
                StreamingSpeechCallbacks(),
                dependencies
            );
        QVERIFY(!xfyun.session.isNull());
        QVERIFY(xfyun.unavailableReason.isEmpty());
        QCOMPARE(xfyunCount, 1);
        QCOMPARE(baiduCount, 0);

        request.provider = QStringLiteral("baidu");
        const StreamingSpeechSessionCreation baidu =
            createStreamingSpeechSession(
                request,
                StreamingSpeechCallbacks(),
                dependencies
            );
        QVERIFY(!baidu.session.isNull());
        QCOMPARE(xfyunCount, 1);
        QCOMPARE(baiduCount, 1);

        request.provider = QStringLiteral("custom");
        const StreamingSpeechSessionCreation custom =
            createStreamingSpeechSession(
                request,
                StreamingSpeechCallbacks(),
                dependencies
            );
        QVERIFY(custom.session.isNull());
        QVERIFY(custom.unavailableReason.contains(QString::fromUtf8("整段识别")));
        QCOMPARE(xfyunCount, 1);
        QCOMPARE(baiduCount, 1);
    }

    void xfyunRequiresAllThreeCredentials()
    {
        SecretConfig secrets = completeSecrets();
        secrets.xfyunApiSecret.clear();
        int xfyunCount = 0;
        int baiduCount = 0;
        StreamingSpeechSessionRequest request;
        request.provider = QStringLiteral("xfyun");

        const StreamingSpeechSessionCreation result =
            createStreamingSpeechSession(
                request,
                StreamingSpeechCallbacks(),
                dependenciesFor(secrets, &xfyunCount, &baiduCount)
            );

        QVERIFY(result.session.isNull());
        QVERIFY(result.unavailableReason.contains(QString::fromUtf8("API Secret")));
        QCOMPARE(xfyunCount, 0);
    }

    void baiduRealtimeRequiresNumericAppIdAndApiKeyButNotSecretKey()
    {
        SecretConfig secrets = completeSecrets();
        secrets.baiduSecretKey.clear();
        int xfyunCount = 0;
        int baiduCount = 0;
        StreamingSpeechSessionRequest request;
        request.provider = QStringLiteral("baidu");

        StreamingSpeechSessionCreation result = createStreamingSpeechSession(
            request,
            StreamingSpeechCallbacks(),
            dependenciesFor(secrets, &xfyunCount, &baiduCount)
        );
        QVERIFY(!result.session.isNull());
        QCOMPARE(baiduCount, 1);

        secrets.baiduAppId = QStringLiteral("not-number");
        result = createStreamingSpeechSession(
            request,
            StreamingSpeechCallbacks(),
            dependenciesFor(secrets, &xfyunCount, &baiduCount)
        );
        QVERIFY(result.session.isNull());
        QVERIFY(result.unavailableReason.contains(QString::fromUtf8("AppID")));
        QCOMPARE(baiduCount, 1);
    }

    void windowsLocalDoesNotLoadSecrets()
    {
        int xfyunCount = 0;
        int baiduCount = 0;
        int windowsCount = 0;
        int secretLoadCount = 0;
        StreamingSpeechSessionRequest request;
        request.provider = QStringLiteral(" WINDOWS-LOCAL ");

        const StreamingSpeechSessionCreation result =
            createStreamingSpeechSession(
                request,
                StreamingSpeechCallbacks(),
                dependenciesFor(
                    SecretConfig(), &xfyunCount, &baiduCount,
                    &windowsCount, &secretLoadCount
                )
            );

        QVERIFY(!result.session.isNull());
        QCOMPARE(windowsCount, 1);
        QCOMPARE(secretLoadCount, 0);
        QCOMPARE(
            static_cast<StubStreamingSpeechSession *>(result.session.data())
                ->providerKind,
            QStringLiteral("windows-local")
        );
    }

    void missingWindowsFactoryReportsLocalComponent()
    {
        int xfyunCount = 0;
        int baiduCount = 0;
        StreamingSpeechSessionFactoryDependencies dependencies =
            dependenciesFor(completeSecrets(), &xfyunCount, &baiduCount);
        dependencies.createWindows =
            StreamingSpeechSessionFactoryDependencies::LocalProviderFactory();
        StreamingSpeechSessionRequest request;
        request.provider = QStringLiteral("windows-local");

        const StreamingSpeechSessionCreation result =
            createStreamingSpeechSession(
                request,
                StreamingSpeechCallbacks(),
                dependencies
            );

        QVERIFY(result.session.isNull());
        QVERIFY(result.unavailableReason.contains(QString::fromUtf8("本地")));
        QVERIFY(result.unavailableReason.contains(QString::fromUtf8("组件")));
    }
};

QTEST_MAIN(StreamingSpeechSessionFactoryTests)

#include "streaming_speech_session_factory_tests.moc"
