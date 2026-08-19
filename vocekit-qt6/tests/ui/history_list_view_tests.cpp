#include "../../src/ui/history_list_view.h"

#include <QtTest>

#include <QLabel>
#include <QListWidget>
#include <QPushButton>

namespace {

QVector<HistoryEntry> sampleEntries(int count)
{
    QVector<HistoryEntry> entries;
    entries.reserve(count);
    for (int i = 0; i < count; ++i) {
        HistoryEntry entry;
        entry.filePath = QStringLiteral("record-%1.json").arg(i);
        entry.modeId = QStringLiteral("ask");
        entry.mode = QStringLiteral("问答");
        entry.output = QStringLiteral("短内容 %1").arg(i);
        entries.append(entry);
    }
    return entries;
}

} // namespace

class HistoryListViewTests : public QObject
{
    Q_OBJECT

private slots:
    void materializesOnlyTheInitialPage();
};

void HistoryListViewTests::materializesOnlyTheInitialPage()
{
    int createdRows = 0;
    HistoryListViewCallbacks callbacks;
    callbacks.createRow = [&createdRows](
        const HistoryEntry &entry,
        QListWidget *list) -> QWidget * {
        ++createdRows;
        auto *label = new QLabel(entry.output, list);
        label->setMinimumHeight(86);
        return label;
    };
    HistoryListViewOptions options;
    options.modeId = QStringLiteral("__all");
    options.initialLoadCount = 18;
    options.loadMoreCount = 18;

    QWidget *view = createHistoryListView(
        sampleEntries(1000),
        options,
        callbacks
    );
    QVERIFY(view);
    QScopedPointer<QWidget> guard(view);
    auto *list = qobject_cast<QListWidget *>(view);
    QVERIFY(list);
    QCOMPARE(createdRows, 18);
    QCOMPARE(list->count(), 19);
    QCOMPARE(list->verticalScrollMode(), QAbstractItemView::ScrollPerPixel);
    QVERIFY(!list->uniformItemSizes());

    auto *loadMore = qobject_cast<QPushButton *>(list->itemWidget(list->item(18)));
    QVERIFY(loadMore);
    QTest::mouseClick(loadMore, Qt::LeftButton);
    QCOMPARE(createdRows, 36);
    QCOMPARE(list->count(), 37);
}

QTEST_MAIN(HistoryListViewTests)

#include "history_list_view_tests.moc"
