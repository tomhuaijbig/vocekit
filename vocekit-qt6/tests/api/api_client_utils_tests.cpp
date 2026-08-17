#include <QtTest>

#include "../../src/api/api_client_utils.h"

class ApiClientUtilsTests : public QObject
{
    Q_OBJECT

private slots:
    void normalizesOpenAiCompatibleChatUrls()
    {
        QCOMPARE(
            openAiCompatibleChatUrl(QString()).toString(),
            QString()
        );
        QCOMPARE(
            openAiCompatibleChatUrl(QStringLiteral("https://api.example.com")).toString(),
            QStringLiteral("https://api.example.com/v1/chat/completions")
        );
        QCOMPARE(
            openAiCompatibleChatUrl(QStringLiteral("api.example.com/")).toString(),
            QStringLiteral("https://api.example.com/v1/chat/completions")
        );
        QCOMPARE(
            openAiCompatibleChatUrl(QStringLiteral("https://api.example.com/v1")).toString(),
            QStringLiteral("https://api.example.com/v1/chat/completions")
        );
        QCOMPARE(
            openAiCompatibleChatUrl(QStringLiteral("https://api.example.com/v1/")).toString(),
            QStringLiteral("https://api.example.com/v1/chat/completions")
        );
        QCOMPARE(
            openAiCompatibleChatUrl(QStringLiteral("https://api.example.com/v1/chat/completions")).toString(),
            QStringLiteral("https://api.example.com/v1/chat/completions")
        );
        QCOMPARE(
            openAiCompatibleChatUrl(QStringLiteral("https://api.example.com/v1/chat/completions/")).toString(),
            QStringLiteral("https://api.example.com/v1/chat/completions")
        );
        QCOMPARE(
            openAiCompatibleChatUrl(QStringLiteral("https://api.example.com/proxy/v1/chat/completions/")).toString(),
            QStringLiteral("https://api.example.com/proxy/v1/chat/completions")
        );
        QCOMPARE(
            openAiCompatibleChatUrl(QStringLiteral("https://api.example.com/service")).toString(),
            QStringLiteral("https://api.example.com/service/v1/chat/completions")
        );
        QCOMPARE(
            openAiCompatibleChatUrl(QStringLiteral("https://api.example.com/service/v1")).toString(),
            QStringLiteral("https://api.example.com/service/v1/chat/completions")
        );
        QCOMPARE(
            openAiCompatibleChatUrl(QStringLiteral("https://api.example.com/nested/gateway")).toString(),
            QStringLiteral("https://api.example.com/nested/gateway/v1/chat/completions")
        );
        QCOMPARE(
            openAiCompatibleChatUrl(QStringLiteral("https://api.example.com/%2Fservice")).toString(QUrl::FullyEncoded),
            QStringLiteral("https://api.example.com/%2Fservice/v1/chat/completions")
        );
        QCOMPARE(
            openAiCompatibleChatUrl(QStringLiteral("https://user:pass@api.example.com:8443/service")).toString(QUrl::FullyEncoded),
            QStringLiteral("https://user:pass@api.example.com:8443/service/v1/chat/completions")
        );
        QCOMPARE(
            openAiCompatibleChatUrl(QStringLiteral("https://[2001:db8::1]:8443/service")).toString(QUrl::FullyEncoded),
            QStringLiteral("https://[2001:db8::1]:8443/service/v1/chat/completions")
        );
        const QUrl mixedCaseUrl = openAiCompatibleChatUrl(
            QStringLiteral("HtTpS://api.example.com/service")
        );
        QCOMPARE(mixedCaseUrl.scheme(), QStringLiteral("https"));
        QCOMPARE(mixedCaseUrl.host(), QStringLiteral("api.example.com"));
        QCOMPARE(
            mixedCaseUrl.path(QUrl::FullyEncoded),
            QStringLiteral("/service/v1/chat/completions")
        );
    }

    void rejectsInvalidOpenAiCompatibleChatUrls()
    {
        QVERIFY(openAiCompatibleChatUrl(QStringLiteral("ftp://api.example.com")).isEmpty());
        QVERIFY(openAiCompatibleChatUrl(QStringLiteral("https://")).isEmpty());
        QVERIFY(openAiCompatibleChatUrl(QStringLiteral("not a host")).isEmpty());
        QVERIFY(openAiCompatibleChatUrl(QStringLiteral("https://api.example.com/v1/v1")).isEmpty());
        QVERIFY(openAiCompatibleChatUrl(QStringLiteral("https://api.example.com/v1/v1/chat/completions")).isEmpty());
        QVERIFY(openAiCompatibleChatUrl(QStringLiteral("https://api.example.com?key=value")).isEmpty());
        QVERIFY(openAiCompatibleChatUrl(QStringLiteral("https://api.example.com#fragment")).isEmpty());
        QVERIFY(openAiCompatibleChatUrl(QStringLiteral("https://api.example.com?")).isEmpty());
        QVERIFY(openAiCompatibleChatUrl(QStringLiteral("https://api.example.com#")).isEmpty());
    }

    void normalizesAnthropicMessagesUrls()
    {
        QCOMPARE(anthropicMessagesUrl(QString()).toString(), QString());
        QCOMPARE(
            anthropicMessagesUrl(QStringLiteral("https://api.example.com")).toString(),
            QStringLiteral("https://api.example.com/v1/messages")
        );
        QCOMPARE(
            anthropicMessagesUrl(QStringLiteral("api.example.com/")).toString(),
            QStringLiteral("https://api.example.com/v1/messages")
        );
        QCOMPARE(
            anthropicMessagesUrl(QStringLiteral("https://api.example.com/v1/")).toString(),
            QStringLiteral("https://api.example.com/v1/messages")
        );
        QCOMPARE(
            anthropicMessagesUrl(QStringLiteral("https://api.example.com/v1/messages")).toString(),
            QStringLiteral("https://api.example.com/v1/messages")
        );
        QCOMPARE(
            anthropicMessagesUrl(QStringLiteral("https://api.example.com/v1/messages/")).toString(),
            QStringLiteral("https://api.example.com/v1/messages")
        );
        QCOMPARE(
            anthropicMessagesUrl(QStringLiteral("https://api.example.com/proxy/v1/messages/")).toString(),
            QStringLiteral("https://api.example.com/proxy/v1/messages")
        );
        QCOMPARE(
            anthropicMessagesUrl(QStringLiteral("https://api.example.com/service")).toString(),
            QStringLiteral("https://api.example.com/service/v1/messages")
        );
        QCOMPARE(
            anthropicMessagesUrl(QStringLiteral("https://api.example.com/service/v1")).toString(),
            QStringLiteral("https://api.example.com/service/v1/messages")
        );
        QCOMPARE(
            anthropicMessagesUrl(QStringLiteral("https://api.example.com/nested/gateway")).toString(),
            QStringLiteral("https://api.example.com/nested/gateway/v1/messages")
        );
        QCOMPARE(
            anthropicMessagesUrl(QStringLiteral("https://api.example.com/%2Fservice")).toString(QUrl::FullyEncoded),
            QStringLiteral("https://api.example.com/%2Fservice/v1/messages")
        );
        QCOMPARE(
            anthropicMessagesUrl(QStringLiteral("https://user:pass@api.example.com:8443/service")).toString(QUrl::FullyEncoded),
            QStringLiteral("https://user:pass@api.example.com:8443/service/v1/messages")
        );
        QCOMPARE(
            anthropicMessagesUrl(QStringLiteral("https://[2001:db8::1]:8443/service")).toString(QUrl::FullyEncoded),
            QStringLiteral("https://[2001:db8::1]:8443/service/v1/messages")
        );
        const QUrl mixedCaseUrl = anthropicMessagesUrl(
            QStringLiteral("HtTpS://api.example.com/service")
        );
        QCOMPARE(mixedCaseUrl.scheme(), QStringLiteral("https"));
        QCOMPARE(mixedCaseUrl.host(), QStringLiteral("api.example.com"));
        QCOMPARE(mixedCaseUrl.path(QUrl::FullyEncoded), QStringLiteral("/service/v1/messages"));
    }

    void rejectsInvalidAnthropicMessagesUrls()
    {
        QVERIFY(anthropicMessagesUrl(QStringLiteral("file:///tmp/api")).isEmpty());
        QVERIFY(anthropicMessagesUrl(QStringLiteral("https://")).isEmpty());
        QVERIFY(anthropicMessagesUrl(QStringLiteral("not a host")).isEmpty());
        QVERIFY(anthropicMessagesUrl(QStringLiteral("https://api.example.com/v1/v1/messages")).isEmpty());
        QVERIFY(anthropicMessagesUrl(QStringLiteral("https://api.example.com?key=value")).isEmpty());
        QVERIFY(anthropicMessagesUrl(QStringLiteral("https://api.example.com#fragment")).isEmpty());
        QVERIFY(anthropicMessagesUrl(QStringLiteral("https://api.example.com?")).isEmpty());
        QVERIFY(anthropicMessagesUrl(QStringLiteral("https://api.example.com#")).isEmpty());
    }

    void readsFirstJsonValueFromNestedPaths()
    {
        QJsonObject root;
        QJsonObject choice;
        QJsonObject message;
        message.insert(QStringLiteral("content"), QStringLiteral("ok"));
        choice.insert(QStringLiteral("message"), message);
        QJsonArray choices;
        choices.append(choice);
        root.insert(QStringLiteral("choices"), choices);

        QCOMPARE(
            firstJsonStringValue(
                root,
                QVector<QStringList>()
                    << (QStringList() << QStringLiteral("missing"))
                    << (QStringList() << QStringLiteral("choices") << QStringLiteral("0") << QStringLiteral("message") << QStringLiteral("content"))
            ),
            QStringLiteral("ok")
        );
    }

    void hashesHmacSha256()
    {
        const QByteArray digest = hmacSha256(
            QByteArrayLiteral("key"),
            QByteArrayLiteral("The quick brown fox jumps over the lazy dog")
        );

        QCOMPARE(
            QString::fromLatin1(digest.toHex()),
            QStringLiteral("f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8")
        );
    }

    void compactsLogText()
    {
        QCOMPARE(
            compactLogText(QStringLiteral(" a\nb\tc "), 4),
            QStringLiteral("a b ...")
        );
    }
};

QTEST_MAIN(ApiClientUtilsTests)
#include "api_client_utils_tests.moc"
