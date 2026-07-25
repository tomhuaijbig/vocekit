#include "../../src/domain/history_selection.h"

#include <QtTest>

namespace {

HistoryEntry entryWithPath(const QString &path, const QString &output = QString())
{
    HistoryEntry entry;
    entry.filePath = path;
    entry.output = output;
    return entry;
}

} // namespace

class HistorySelectionTests : public QObject
{
    Q_OBJECT

private slots:
    void ignoresEmptyFilePaths();
    void selectsEntriesAndReturnsOnlyCurrentMatches();
    void togglesSingleEntrySelection();
    void clearsSelection();
};

void HistorySelectionTests::ignoresEmptyFilePaths()
{
    HistorySelectionState selection;
    selection.setFileSelected(QString(), true);
    selection.setEntrySelected(entryWithPath(QStringLiteral("   ")), true);

    QVERIFY(selection.isEmpty());
    QCOMPARE(selection.count(), 0);
    QVERIFY(selection.selectedFilePaths().isEmpty());
}

void HistorySelectionTests::selectsEntriesAndReturnsOnlyCurrentMatches()
{
    QVector<HistoryEntry> all;
    all << entryWithPath(QStringLiteral("one.json"), QStringLiteral("one"));
    all << entryWithPath(QStringLiteral("two.json"), QStringLiteral("two"));
    all << entryWithPath(QStringLiteral("three.json"), QStringLiteral("three"));

    HistorySelectionState selection;
    selection.selectEntries(all.mid(0, 2));

    QCOMPARE(selection.count(), 2);
    QVERIFY(selection.containsFilePath(QStringLiteral("one.json")));
    QVERIFY(selection.containsEntry(all.at(1)));

    QVector<HistoryEntry> current;
    current << all.at(1) << all.at(2);
    const QVector<HistoryEntry> selected = selection.selectedEntriesFrom(current);

    QCOMPARE(selected.size(), 1);
    QCOMPARE(selected.first().filePath, QStringLiteral("two.json"));
}

void HistorySelectionTests::togglesSingleEntrySelection()
{
    HistorySelectionState selection;
    const HistoryEntry entry = entryWithPath(QStringLiteral("detail.json"));

    selection.setEntrySelected(entry, true);
    QVERIFY(selection.containsEntry(entry));
    QCOMPARE(selection.count(), 1);

    selection.setEntrySelected(entry, false);
    QVERIFY(!selection.containsEntry(entry));
    QVERIFY(selection.isEmpty());
}

void HistorySelectionTests::clearsSelection()
{
    HistorySelectionState selection;
    selection.setFileSelected(QStringLiteral("a.json"), true);
    selection.setFileSelected(QStringLiteral("b.json"), true);

    QCOMPARE(selection.count(), 2);
    selection.clear();
    QVERIFY(selection.isEmpty());
}

QTEST_MAIN(HistorySelectionTests)

#include "history_selection_tests.moc"
