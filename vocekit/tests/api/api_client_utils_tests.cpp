#include <QtTest>

#include "../../src/api/api_client_utils.h"

class ApiClientUtilsTests : public QObject
{
    Q_OBJECT

private slots:
    void normalizesOpenAiCompatibleChatUrls()
    {
        QCOMPARE(
            openAiCompatibleChatUrl(QStringLiteral("api.example.com")).toString(),
            QStringLiteral("https://api.example.com/v1/chat/completions")
        );
        QCOMPARE(
            openAiCompatibleChatUrl(QStringLiteral("https://api.example.com/v1")).toString(),
            QStringLiteral("https://api.example.com/v1/chat/completions")
        );
        QCOMPARE(
            openAiCompatibleChatUrl(QStringLiteral("https://api.example.com/v1/chat/completions")).toString(),
            QStringLiteral("https://api.example.com/v1/chat/completions")
        );
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
