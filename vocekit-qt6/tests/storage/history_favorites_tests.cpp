#include "../../src/file_utils.h"
#include "../../src/storage/history_favorites.h"

#include <QtTest>

#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

namespace {

QJsonObject readObject(const QString &path)
{
    QJsonObject object;
    readJsonObjectFile(path, &object);
    return object;
}

bool writeObject(const QString &path, const QJsonObject &object)
{
    return writeBytesAtomically(path, QJsonDocument(object).toJson(QJsonDocument::Indented));
}

} // namespace

class HistoryFavoritesTests : public QObject
{
    Q_OBJECT

private slots:
    void marksFavoriteAndWritesMirrorFile();
    void removesFolderWhenFavoriteDisabled();
    void succeedsWhenMirrorFileIsMissing();
    void failsWhenMainFileIsMissing();
};

void HistoryFavoritesTests::marksFavoriteAndWritesMirrorFile()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString mainPath = QDir(temp.path()).filePath(QStringLiteral("main.json"));
    const QString mirrorPath = QDir(temp.path()).filePath(QStringLiteral("mirror.json"));

    QJsonObject item;
    item.insert(QStringLiteral("output"), QStringLiteral("hello"));
    item.insert(QStringLiteral("allDetailFile"), mirrorPath);
    QVERIFY(writeObject(mainPath, item));
    QVERIFY(writeObject(mirrorPath, item));

    const HistoryFavoriteUpdateResult result = updateHistoryFavoriteFiles(
        mainPath,
        true,
        QStringLiteral("work")
    );

    QVERIFY(result.ok);
    QVERIFY(result.wroteMain);
    QVERIFY(result.wroteMirror);

    const QJsonObject main = readObject(mainPath);
    const QJsonObject mirror = readObject(mirrorPath);
    QVERIFY(main.value(QStringLiteral("favorite")).toBool());
    QCOMPARE(main.value(QStringLiteral("favoriteFolder")).toString(), QStringLiteral("work"));
    QVERIFY(mirror.value(QStringLiteral("favorite")).toBool());
    QCOMPARE(mirror.value(QStringLiteral("favoriteFolder")).toString(), QStringLiteral("work"));
}

void HistoryFavoritesTests::removesFolderWhenFavoriteDisabled()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString mainPath = QDir(temp.path()).filePath(QStringLiteral("main.json"));
    QJsonObject item;
    item.insert(QStringLiteral("favorite"), true);
    item.insert(QStringLiteral("favoriteFolder"), QStringLiteral("work"));
    QVERIFY(writeObject(mainPath, item));

    const HistoryFavoriteUpdateResult result = updateHistoryFavoriteFiles(mainPath, false);

    QVERIFY(result.ok);
    const QJsonObject updated = readObject(mainPath);
    QVERIFY(!updated.value(QStringLiteral("favorite")).toBool());
    QVERIFY(!updated.contains(QStringLiteral("favoriteFolder")));
}

void HistoryFavoritesTests::succeedsWhenMirrorFileIsMissing()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString mainPath = QDir(temp.path()).filePath(QStringLiteral("main.json"));
    QJsonObject item;
    item.insert(QStringLiteral("allDetailFile"), QDir(temp.path()).filePath(QStringLiteral("missing.json")));
    QVERIFY(writeObject(mainPath, item));

    const HistoryFavoriteUpdateResult result = updateHistoryFavoriteFiles(mainPath, true);

    QVERIFY(result.ok);
    QVERIFY(result.wroteMain);
    QVERIFY(!result.wroteMirror);
}

void HistoryFavoritesTests::failsWhenMainFileIsMissing()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const HistoryFavoriteUpdateResult result = updateHistoryFavoriteFiles(
        QDir(temp.path()).filePath(QStringLiteral("missing.json")),
        true
    );

    QVERIFY(!result.ok);
    QVERIFY(!result.error.isEmpty());
}

QTEST_MAIN(HistoryFavoritesTests)

#include "history_favorites_tests.moc"
