#include <QtTest>

#include "../../src/ui/history_entry_actions_controller.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

namespace {

QString createHistoryDetail(const QString &rootPath)
{
    const QString directory = QDir(rootPath).filePath(
        QStringLiteral("听写/2026-07-25/详细记录")
    );
    if (!QDir().mkpath(directory)) {
        return QString();
    }

    const QString path = QDir(directory).filePath(
        QStringLiteral("20260725-120000.json")
    );
    QJsonObject item;
    item.insert(QStringLiteral("mode"), QStringLiteral("dictate"));
    item.insert(QStringLiteral("modeTitle"), QStringLiteral("听写"));
    item.insert(
        QStringLiteral("timestamp"),
        QStringLiteral("2026-07-25T12:00:00")
    );
    item.insert(QStringLiteral("favorite"), false);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return QString();
    }
    file.write(QJsonDocument(item).toJson(QJsonDocument::Indented));
    file.close();
    return path;
}

} // namespace

class HistoryEntryActionsControllerTests : public QObject
{
    Q_OBJECT

private slots:
    void favoriteMutationPublishesHistoryChange();
};

void HistoryEntryActionsControllerTests::favoriteMutationPublishesHistoryChange()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString detailPath = createHistoryDetail(temporary.path());
    QVERIFY(!detailPath.isEmpty());

    QStringList receivedIds;
    bool receivedReset = true;

    HistoryEntryActionsController::Callbacks callbacks;
    callbacks.recordDirectoryPath = [&temporary]() {
        return temporary.path();
    };
    callbacks.historyChanged = [&receivedIds, &receivedReset](
        const QStringList &recordIds,
        bool resetRequired
    ) {
        receivedIds = recordIds;
        receivedReset = resetRequired;
    };

    HistoryEntryActionsController controller(nullptr, callbacks);
    controller.toggleHistoryFavorite(detailPath);

    QCOMPARE(receivedIds, QStringList() << detailPath);
    QVERIFY(!receivedReset);
}

QTEST_MAIN(HistoryEntryActionsControllerTests)

#include "history_entry_actions_controller_tests.moc"
