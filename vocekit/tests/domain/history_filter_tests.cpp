#include "../../src/domain/history_filter.h"

#include <QtTest>

namespace {

HistoryEntry entry(
    const QString &modeId,
    const QString &mode,
    const QString &output
)
{
    HistoryEntry item;
    item.modeId = modeId;
    item.mode = mode;
    item.output = output;
    return item;
}

CustomFunctionDef customFunction(const QString &id, const QString &name)
{
    CustomFunctionDef fn;
    fn.id = id;
    fn.name = name;
    return fn;
}

} // namespace

class HistoryFilterTests : public QObject
{
    Q_OBJECT

private slots:
    void filtersByModeAndSearchText();
    void returnsMatchingIndexes();
    void supportsFavoriteFolders();
    void supportsCustomFunctionNameFallback();
};

void HistoryFilterTests::filtersByModeAndSearchText()
{
    QVector<HistoryEntry> entries;
    entries.append(entry(QStringLiteral("dictate"), QStringLiteral("听写"), QStringLiteral("alpha note")));
    entries.append(entry(QStringLiteral("translate"), QStringLiteral("翻译"), QStringLiteral("alpha translated")));
    entries.append(entry(QStringLiteral("dictate"), QStringLiteral("听写"), QStringLiteral("beta note")));

    HistoryFilter filter;
    filter.modeId = QStringLiteral("dictate");
    filter.searchText = QStringLiteral("alpha");

    const QVector<HistoryEntry> filtered = filterHistoryEntries(entries, filter);
    QCOMPARE(filtered.size(), 1);
    QCOMPARE(filtered.first().modeId, QStringLiteral("dictate"));
    QCOMPARE(filtered.first().output, QStringLiteral("alpha note"));
}

void HistoryFilterTests::returnsMatchingIndexes()
{
    QVector<HistoryEntry> entries;
    entries.append(entry(QStringLiteral("dictate"), QStringLiteral("听写"), QStringLiteral("first")));
    entries.append(entry(QStringLiteral("ask"), QStringLiteral("问答"), QStringLiteral("target one")));
    entries.append(entry(QStringLiteral("ask"), QStringLiteral("问答"), QStringLiteral("target two")));

    HistoryFilter filter;
    filter.modeId = QStringLiteral("ask");
    filter.searchText = QStringLiteral("target");

    const QVector<int> indexes = historyEntryIndexesMatchingFilter(entries, filter);
    QCOMPARE(indexes, QVector<int>() << 1 << 2);
}

void HistoryFilterTests::supportsFavoriteFolders()
{
    HistoryEntry item = entry(QStringLiteral("dictate"), QStringLiteral("听写"), QStringLiteral("favorite"));
    item.favorite = true;
    item.favoriteFolder = QStringLiteral("工作");

    HistoryFilter filter;
    filter.modeId = QStringLiteral("__favorite_folder:工作");
    QVERIFY(historyEntryMatchesFilter(item, filter));

    filter.modeId = QStringLiteral("__favorite_folder:学习");
    QVERIFY(!historyEntryMatchesFilter(item, filter));
}

void HistoryFilterTests::supportsCustomFunctionNameFallback()
{
    HistoryEntry item = entry(QString(), QStringLiteral("会议整理"), QStringLiteral("minutes"));
    HistoryFilter filter;
    filter.modeId = QStringLiteral("custom_meeting");
    filter.customFunctions.append(customFunction(QStringLiteral("custom_meeting"), QStringLiteral("会议整理")));

    QVERIFY(historyEntryMatchesFilter(item, filter));
}

QTEST_MAIN(HistoryFilterTests)

#include "history_filter_tests.moc"
