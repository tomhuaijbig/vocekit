#include "../../src/providers/model_catalog.h"

#include <QtTest>

#include <QStringList>

class ModelCatalogTests : public QObject
{
    Q_OBJECT

private slots:
    void builtInOptionsExposeOnlyCurrentOpenAiAndAnthropicModels();
    void normalizeModelIdMigratesLegacyAndUnknownProviderModels();
    void normalizeModelIdAcceptsCurrentModelsWithAndWithoutProviderPrefixes();
    void displayTextHandlesEmptyBuiltInAndUnknownIds();
    void currentModelAliasesUseCurrentTitlesWithoutProviderPrefixes();
};

void ModelCatalogTests::builtInOptionsExposeOnlyCurrentOpenAiAndAnthropicModels()
{
    const QVector<ModelOption> options = modelOptionsForSecrets(SecretConfig());
    QStringList ids;
    QStringList titles;
    for (const ModelOption &option : options) {
        ids.append(option.id);
        titles.append(option.title);
    }

    QCOMPARE(ids, QStringList()
        << QStringLiteral("deepseek-v4-flash")
        << QStringLiteral("deepseek-v4-pro")
        << QStringLiteral("openai:gpt-5.6-sol")
        << QStringLiteral("openai:gpt-5.6-terra")
        << QStringLiteral("openai:gpt-5.6-luna")
        << QStringLiteral("claude:claude-fable-5")
        << QStringLiteral("claude:claude-opus-5")
        << QStringLiteral("claude:claude-sonnet-5")
        << QStringLiteral("claude:claude-haiku-4-5"));
    QCOMPARE(titles, QStringList()
        << QStringLiteral("deepseek-v4-flash")
        << QStringLiteral("deepseek-v4-pro")
        << QStringLiteral("GPT-5.6 Sol")
        << QStringLiteral("GPT-5.6 Terra")
        << QStringLiteral("GPT-5.6 Luna")
        << QStringLiteral("Claude Fable 5")
        << QStringLiteral("Claude Opus 5")
        << QStringLiteral("Claude Sonnet 5")
        << QStringLiteral("Claude Haiku 4.5"));
    QVERIFY(!ids.contains(QStringLiteral("openai:gpt-5.5")));
    QVERIFY(!ids.contains(QStringLiteral("openai:gpt-5.4")));
    QVERIFY(!ids.contains(QStringLiteral("openai:gpt-5.4-mini")));
    QVERIFY(!ids.contains(QStringLiteral("claude:claude-opus-4-8")));
    QVERIFY(!ids.contains(QStringLiteral("claude:claude-opus-4-7")));
    QVERIFY(!ids.contains(QStringLiteral("claude:claude-sonnet-4-6")));
}

void ModelCatalogTests::normalizeModelIdMigratesLegacyAndUnknownProviderModels()
{
    QCOMPARE(normalizeModelId(QStringLiteral("gpt-5.5")), QStringLiteral("openai:gpt-5.6-sol"));
    QCOMPARE(normalizeModelId(QStringLiteral("openai:gpt-5.5")), QStringLiteral("openai:gpt-5.6-sol"));
    QCOMPARE(normalizeModelId(QStringLiteral("gpt-5.4")), QStringLiteral("openai:gpt-5.6-terra"));
    QCOMPARE(normalizeModelId(QStringLiteral("openai:gpt-5.4")), QStringLiteral("openai:gpt-5.6-terra"));
    QCOMPARE(normalizeModelId(QStringLiteral("gpt-5.4-mini")), QStringLiteral("openai:gpt-5.6-luna"));
    QCOMPARE(normalizeModelId(QStringLiteral("openai:gpt-5.4-mini")), QStringLiteral("openai:gpt-5.6-luna"));
    QCOMPARE(normalizeModelId(QStringLiteral("gpt-4o")), QStringLiteral("openai:gpt-5.6-terra"));
    QCOMPARE(normalizeModelId(QStringLiteral("openai:gpt-4.1")), QStringLiteral("openai:gpt-5.6-terra"));
    QCOMPARE(normalizeModelId(QStringLiteral("gpt-future")), QStringLiteral("openai:gpt-5.6-terra"));
    QCOMPARE(normalizeModelId(QStringLiteral("openai:future")), QStringLiteral("openai:gpt-5.6-terra"));
    QCOMPARE(normalizeModelId(QStringLiteral("opus-4-8")), QStringLiteral("claude:claude-opus-5"));
    QCOMPARE(normalizeModelId(QStringLiteral("claude:opus-4-8")), QStringLiteral("claude:claude-opus-5"));
    QCOMPARE(normalizeModelId(QStringLiteral("opus-4-7")), QStringLiteral("claude:claude-opus-5"));
    QCOMPARE(normalizeModelId(QStringLiteral("claude:claude-opus-4-7")), QStringLiteral("claude:claude-opus-5"));
    QCOMPARE(normalizeModelId(QStringLiteral("sonnet-4-6")), QStringLiteral("claude:claude-sonnet-5"));
    QCOMPARE(normalizeModelId(QStringLiteral("claude:claude-sonnet-4-6")), QStringLiteral("claude:claude-sonnet-5"));
    QCOMPARE(normalizeModelId(QStringLiteral("claude-3-7-sonnet")), QStringLiteral("claude:claude-sonnet-5"));
    QCOMPARE(normalizeModelId(QStringLiteral("claude:claude-3-5-haiku")), QStringLiteral("claude:claude-sonnet-5"));
    QCOMPARE(normalizeModelId(QStringLiteral("claude-future")), QStringLiteral("claude:claude-sonnet-5"));
    QCOMPARE(normalizeModelId(QStringLiteral("claude:future")), QStringLiteral("claude:claude-sonnet-5"));
    QCOMPARE(normalizeModelId(QStringLiteral("openai:gpt-5.6-sol")), QStringLiteral("openai:gpt-5.6-sol"));
    QCOMPARE(normalizeModelId(QStringLiteral("claude:claude-fable-5")), QStringLiteral("claude:claude-fable-5"));
    QCOMPARE(normalizeModelId(QStringLiteral("custom:kept")), QStringLiteral("custom:kept"));
    QCOMPARE(normalizeModelId(QStringLiteral("deepseek-v4-pro")), QStringLiteral("deepseek-v4-pro"));
}

void ModelCatalogTests::normalizeModelIdAcceptsCurrentModelsWithAndWithoutProviderPrefixes()
{
    struct ModelIdAlias {
        const char *bareId;
        const char *canonicalId;
    };
    const ModelIdAlias aliases[] = {
        {"gpt-5.6-sol", "openai:gpt-5.6-sol"},
        {"gpt-5.6-terra", "openai:gpt-5.6-terra"},
        {"gpt-5.6-luna", "openai:gpt-5.6-luna"},
        {"claude-fable-5", "claude:claude-fable-5"},
        {"claude-opus-5", "claude:claude-opus-5"},
        {"claude-sonnet-5", "claude:claude-sonnet-5"},
        {"claude-haiku-4-5", "claude:claude-haiku-4-5"}
    };

    for (const ModelIdAlias &alias : aliases) {
        const QString bareId = QString::fromLatin1(alias.bareId);
        const QString canonicalId = QString::fromLatin1(alias.canonicalId);
        QCOMPARE(normalizeModelId(bareId), canonicalId);
        QCOMPARE(normalizeModelId(canonicalId), canonicalId);
    }

    QCOMPARE(
        normalizeModelId(QStringLiteral("  gpt-5.6-sol  ")),
        QStringLiteral("openai:gpt-5.6-sol")
    );
    QCOMPARE(
        normalizeModelId(QStringLiteral("GPT-5.6-SOL"), QStringLiteral("kept-fallback")),
        QStringLiteral("kept-fallback")
    );
}

void ModelCatalogTests::displayTextHandlesEmptyBuiltInAndUnknownIds()
{
    QCOMPARE(
        modelDisplayText(QString()),
        QString::fromUtf8("\u672a\u8c03\u7528\u5927\u6a21\u578b")
    );
    QCOMPARE(
        modelDisplayText(QStringLiteral("deepseek-v4-flash")),
        QStringLiteral("deepseek-v4-flash")
    );
    QCOMPARE(modelTitle(QStringLiteral("gpt-5.5")), QStringLiteral("GPT-5.6 Sol"));
    QCOMPARE(
        modelDisplayText(QStringLiteral("openai:gpt-5.4-mini")),
        QString::fromUtf8("GPT-5.6 Luna\uff08openai:gpt-5.4-mini\uff09")
    );
    QCOMPARE(modelTitle(QStringLiteral("claude:claude-opus-4-8")), QStringLiteral("Claude Opus 5"));
    QCOMPARE(
        modelDisplayText(QStringLiteral("claude-3-7-sonnet")),
        QString::fromUtf8("Claude Sonnet 5\uff08claude-3-7-sonnet\uff09")
    );
    QCOMPARE(
        modelDisplayText(QStringLiteral("unknown-model")),
        QStringLiteral("unknown-model")
    );
    QCOMPARE(modelTitle(QStringLiteral("unknown-model")), QStringLiteral("unknown-model"));
}

void ModelCatalogTests::currentModelAliasesUseCurrentTitlesWithoutProviderPrefixes()
{
    struct DisplayAlias {
        const char *bareId;
        const char *title;
    };
    const DisplayAlias aliases[] = {
        {"gpt-5.6-sol", "GPT-5.6 Sol"},
        {"gpt-5.6-luna", "GPT-5.6 Luna"},
        {"claude-fable-5", "Claude Fable 5"},
        {"claude-opus-5", "Claude Opus 5"},
        {"claude-haiku-4-5", "Claude Haiku 4.5"}
    };

    for (const DisplayAlias &alias : aliases) {
        const QString bareId = QString::fromLatin1(alias.bareId);
        const QString title = QString::fromLatin1(alias.title);
        QCOMPARE(modelTitle(bareId), title);
        QCOMPARE(
            modelDisplayText(bareId),
            title + QString::fromUtf8("\uff08") + bareId + QString::fromUtf8("\uff09")
        );
    }
}

QTEST_MAIN(ModelCatalogTests)

#include "model_catalog_tests.moc"
