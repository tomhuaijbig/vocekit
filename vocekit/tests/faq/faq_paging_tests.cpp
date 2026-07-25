#include <QtTest>

#include "../../src/faq_paging.h"

class FaqPagingTests : public QObject
{
    Q_OBJECT

private slots:
    void limitsInitialResults()
    {
        QVector<FaqPagingItem> items;
        for (int i = 0; i < 20; ++i) {
            items.append({QStringLiteral("问题 %1 网络").arg(i), QStringLiteral("network")});
        }

        const QVector<int> visible = faqVisibleIndexes(items, QString(), QStringLiteral("all"), 10);
        QCOMPARE(visible.size(), 10);
        QCOMPARE(faqMatchCount(items, QString(), QStringLiteral("all")), 20);
        QVERIFY(faqHasMoreMatches(items, QString(), QStringLiteral("all"), 10));
    }

    void filtersBeforeApplyingLimit()
    {
        const QVector<FaqPagingItem> items = {
            {QStringLiteral("网络请求失败"), QStringLiteral("network")},
            {QStringLiteral("麦克风没有声音"), QStringLiteral("microphone")},
            {QStringLiteral("网络请求超时"), QStringLiteral("network")}
        };

        const QVector<int> visible = faqVisibleIndexes(
            items,
            QStringLiteral("网络"),
            QStringLiteral("network"),
            1
        );
        QCOMPARE(visible, QVector<int>() << 0);
        QVERIFY(faqHasMoreMatches(items, QStringLiteral("网络"), QStringLiteral("network"), 1));
        QVERIFY(!faqHasMoreMatches(items, QStringLiteral("麦克风"), QStringLiteral("microphone"), 8));
    }

    void growsByConfiguredBatch()
    {
        QCOMPARE(faqNextVisibleLimit(10, 40, 10), 20);
        QCOMPARE(faqNextVisibleLimit(35, 40, 10), 40);
        QCOMPARE(faqNextVisibleLimit(40, 40, 10), 40);
    }
};

QTEST_APPLESS_MAIN(FaqPagingTests)

#include "faq_paging_tests.moc"
