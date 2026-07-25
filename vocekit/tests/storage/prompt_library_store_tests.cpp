#include <QtTest>

#include "../../src/storage/prompt_library_store.h"

#include <QFile>
#include <QTemporaryDir>

class PromptLibraryStoreTests : public QObject
{
    Q_OBJECT

private slots:
    void missingFileStartsWithEmptyLibrary();
    void savesAndLoadsPromptItems();
    void reportsInvalidJson();
};

void PromptLibraryStoreTests::missingFileStartsWithEmptyLibrary()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    PromptLibraryStore store(directory.filePath(QStringLiteral("prompts.json")));
    OperationError error;
    QVERIFY(store.load(&error));
    QVERIFY(store.items().isEmpty());
    QVERIFY(error.code.isEmpty());
}

void PromptLibraryStoreTests::savesAndLoadsPromptItems()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("config/prompts.json"));

    PromptLibraryItem item;
    item.id = QStringLiteral("prompt_1");
    item.name = QStringLiteral("正式语气");
    item.scope = QStringLiteral("通用");
    item.content = QStringLiteral("使用正式语气改写。");

    PromptLibraryStore writer(path);
    QVERIFY(writer.save(QVector<PromptLibraryItem>() << item));
    QCOMPARE(writer.items().size(), 1);

    PromptLibraryStore reader(path);
    QVERIFY(reader.load());
    QCOMPARE(reader.items().size(), 1);
    QCOMPARE(reader.items().first().content, item.content);
}

void PromptLibraryStoreTests::reportsInvalidJson()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("prompts.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("{invalid");
    file.close();

    PromptLibraryStore store(path);
    OperationError error;
    QVERIFY(!store.load(&error));
    QCOMPARE(error.code, QStringLiteral("prompt_library.invalid_json"));
}

QTEST_APPLESS_MAIN(PromptLibraryStoreTests)

#include "prompt_library_store_tests.moc"
