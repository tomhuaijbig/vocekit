#include <QtTest>

#include "../../src/ui/hub_settings_state.h"
#include "../../src/ui/vocabulary_page_access_factory.h"

#include <QFile>

namespace {

AppSettingsData sampleSettings()
{
    AppSettingsData settings;
    FunctionSettings custom;
    custom.id = QStringLiteral("custom_1");
    custom.name = QStringLiteral("润色");
    custom.shortcut = QStringLiteral("Ctrl+Alt+1");
    settings.functions.append(custom);
    return settings;
}

} // namespace

class VocabularyPageAccessFactoryTests : public QObject
{
    Q_OBJECT

private slots:
    void buildsSettingsSnapshot();
    void forwardsPageDependencies();
    void handlesMissingDependencies();
    void contentControllerUsesIndependentFactory();
};

void VocabularyPageAccessFactoryTests::buildsSettingsSnapshot()
{
    const AppSettingsData source = sampleSettings();
    HubWindowAccess stateAccess;
    stateAccess.settingsSnapshotProvider = [source]() { return source; };
    HubSettingsState settings(stateAccess);

    VocabularyPageAccessFactoryDependencies dependencies;
    dependencies.settings = &settings;
    const VocabularyPageAccess access = createVocabularyPageAccess(dependencies);

    QVERIFY(access.settingsSnapshotProvider);
    const VocabularyPageSettingsSnapshot snapshot =
        access.settingsSnapshotProvider();
    QCOMPARE(snapshot.customFunctions.size(), 1);
    QCOMPARE(snapshot.customFunctions.first().id, QStringLiteral("custom_1"));
    QCOMPARE(snapshot.customFunctions.first().name, QStringLiteral("润色"));
}

void VocabularyPageAccessFactoryTests::forwardsPageDependencies()
{
    bool aiCalled = false;
    bool historyCalled = false;
    QStringList changedIds;
    bool changedReset = false;

    VocabularyPageAccessFactoryDependencies dependencies;
    dependencies.vocabularyAi = [&aiCalled](
        const QString &source,
        const QString &,
        QString *,
        const QString &,
        const QString &
    ) {
        aiCalled = true;
        VocabularySuggestion suggestion;
        suggestion.valid = true;
        suggestion.entry.source = source;
        return suggestion;
    };
    dependencies.historyEntries = [&historyCalled]() {
        historyCalled = true;
        HistoryEntry entry;
        entry.mode = QStringLiteral("dictate");
        return QVector<HistoryEntry>() << entry;
    };
    dependencies.vocabularyChanged = [&changedIds, &changedReset](
        const QStringList &ids,
        bool resetRequired
    ) {
        changedIds = ids;
        changedReset = resetRequired;
    };

    const VocabularyPageAccess access = createVocabularyPageAccess(dependencies);
    const VocabularySuggestion suggestion = access.vocabularyAi(
        QStringLiteral("vocekit"),
        QString(),
        nullptr,
        QString(),
        QString()
    );
    const QVector<HistoryEntry> entries = access.historyEntries();
    access.vocabularyChanged(QStringList() << QStringLiteral("entry_1"), true);

    QVERIFY(aiCalled);
    QVERIFY(suggestion.valid);
    QCOMPARE(suggestion.entry.source, QStringLiteral("vocekit"));
    QVERIFY(historyCalled);
    QCOMPARE(entries.size(), 1);
    QCOMPARE(changedIds, QStringList() << QStringLiteral("entry_1"));
    QVERIFY(changedReset);
}

void VocabularyPageAccessFactoryTests::handlesMissingDependencies()
{
    const VocabularyPageAccess access = createVocabularyPageAccess(
        VocabularyPageAccessFactoryDependencies()
    );
    QVERIFY(access.settingsSnapshotProvider);
    QVERIFY(access.vocabularyAi);
    QVERIFY(access.historyEntries);
    QVERIFY(access.vocabularyChanged);
    QVERIFY(access.settingsSnapshotProvider().customFunctions.isEmpty());
    QVERIFY(access.historyEntries().isEmpty());
    QVERIFY(!access.vocabularyAi(
        QString(), QString(), nullptr, QString(), QString()
    ).valid);
    access.vocabularyChanged(QStringList(), false);
}

void VocabularyPageAccessFactoryTests::contentControllerUsesIndependentFactory()
{
    const QString sourcePath = QFINDTESTDATA(
        "../../src/ui/hub_content_pages_controller.cpp"
    );
    QVERIFY2(!sourcePath.isEmpty(), "找不到内容页面控制器源文件");
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();

    QVERIFY(contents.contains("createVocabularyPageAccess(dependencies)"));
    QVERIFY(!contents.contains("VocabularyPageAccess access;"));
    QVERIFY(!contents.contains("VocabularyPageSettingsSnapshot snapshot;"));
}

QTEST_APPLESS_MAIN(VocabularyPageAccessFactoryTests)

#include "vocabulary_page_access_factory_tests.moc"
