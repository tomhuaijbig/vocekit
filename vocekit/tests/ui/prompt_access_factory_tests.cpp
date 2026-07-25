#include <QtTest>

#include "../../src/ui/hub_settings_state.h"
#include "../../src/ui/prompt_access_factory.h"

#include <QFile>

class PromptAccessFactoryTests : public QObject
{
    Q_OBJECT

private slots:
    void providesRuntimeSnapshotAndSavesFunctionPrompt();
    void managesPromptLibraryItems();
    void restoresPromptLockAfterFailedSave();
    void handlesMissingDependencies();
    void utilityControllerUsesIndependentFactory();
};

namespace {

FunctionSettings customFunction()
{
    FunctionSettings function;
    function.id = QStringLiteral("custom_1");
    function.name = QStringLiteral("测试功能");
    function.builtIn = false;
    function.promptId = function.id;
    function.prompt = QStringLiteral("原提示词");
    return function;
}

PromptLibraryItem promptItem(const QString &id, const QString &name)
{
    PromptLibraryItem item;
    item.id = id;
    item.name = name;
    item.content = QStringLiteral("原内容");
    item.scope = QStringLiteral("通用");
    return item;
}

} // namespace

void PromptAccessFactoryTests::providesRuntimeSnapshotAndSavesFunctionPrompt()
{
    AppSettingsData initial;
    initial.functions.append(customFunction());
    QVector<PromptLibraryItem> library;
    library.append(promptItem(QStringLiteral("prompt_1"), QStringLiteral("正式语气")));
    AppSettingsData saved;

    HubWindowAccess settingsAccess;
    settingsAccess.settingsSnapshotProvider = [initial]() { return initial; };
    settingsAccess.promptLibraryProvider = [library]() { return library; };
    settingsAccess.applyAndSave = [&saved](const AppSettingsData &settings) {
        saved = settings;
        return true;
    };
    HubSettingsState settings(settingsAccess);

    PromptAccessFactoryDependencies dependencies;
    dependencies.settings = &settings;
    const PromptAccessAssembly assembly = createPromptAccessAssembly(dependencies);

    QVERIFY(assembly.settings.snapshotProvider);
    const PromptRuntimeSnapshot snapshot = assembly.settings.snapshotProvider();
    QCOMPARE(snapshot.settings.functions.size(), 1);
    QCOMPARE(snapshot.libraryItems.size(), 1);

    QString error;
    QVERIFY(assembly.settings.saveFunctionPrompt(
        QStringLiteral("custom_1"),
        QStringLiteral("新提示词"),
        &error
    ));
    QCOMPARE(saved.functions.first().prompt, QStringLiteral("新提示词"));
    QVERIFY(error.isEmpty());
}

void PromptAccessFactoryTests::managesPromptLibraryItems()
{
    QVector<PromptLibraryItem> library;
    library.append(promptItem(QStringLiteral("prompt_1"), QStringLiteral("正式语气")));
    QVector<PromptLibraryItem> savedLibrary;

    HubWindowAccess settingsAccess;
    settingsAccess.settingsSnapshotProvider = []() { return AppSettingsData(); };
    settingsAccess.promptLibraryProvider = [library]() { return library; };
    settingsAccess.applyAndSave = [](const AppSettingsData &) { return true; };
    settingsAccess.savePromptLibrary = [&savedLibrary](const QVector<PromptLibraryItem> &items) {
        savedLibrary = items;
        return true;
    };
    HubSettingsState settings(settingsAccess);

    PromptAccessFactoryDependencies dependencies;
    dependencies.settings = &settings;
    const PromptAccessAssembly assembly = createPromptAccessAssembly(dependencies);

    PromptLibraryItem changed = promptItem(
        QStringLiteral("prompt_1"),
        QStringLiteral("正式表达")
    );
    changed.content = QStringLiteral("使用正式表达。");
    QString error;
    QVERIFY(assembly.panel.saveLibraryPromptItem(changed, &error));
    QCOMPARE(savedLibrary.first().name, QStringLiteral("正式表达"));

    PromptLibraryItem created;
    created.name = QString::fromUtf8("新提示词");
    created.content = QStringLiteral("新增内容");
    QVERIFY(assembly.panel.createLibraryPromptItem(&created, &error));
    QCOMPARE(created.id, QStringLiteral("prompt_2"));
    QCOMPARE(created.name, QString::fromUtf8("新提示词 2"));
    QCOMPARE(savedLibrary.size(), 2);

    QVERIFY(assembly.panel.deleteLibraryPromptItem(
        QStringLiteral("prompt_1"),
        &error
    ));
    QCOMPARE(savedLibrary.size(), 1);
    QCOMPARE(savedLibrary.first().id, QStringLiteral("prompt_2"));
}

void PromptAccessFactoryTests::restoresPromptLockAfterFailedSave()
{
    AppSettingsData initial;
    initial.promptLocked = false;

    HubWindowAccess settingsAccess;
    settingsAccess.settingsSnapshotProvider = [initial]() { return initial; };
    settingsAccess.applyAndSave = [](const AppSettingsData &) { return false; };
    HubSettingsState settings(settingsAccess);

    PromptAccessFactoryDependencies dependencies;
    dependencies.settings = &settings;
    const PromptAccessAssembly assembly = createPromptAccessAssembly(dependencies);

    QString error;
    QVERIFY(!assembly.panel.setPromptLocked(true, &error));
    QVERIFY(!settings.promptLocked());
    QVERIFY(!error.isEmpty());
}

void PromptAccessFactoryTests::handlesMissingDependencies()
{
    const PromptAccessAssembly assembly = createPromptAccessAssembly(
        PromptAccessFactoryDependencies()
    );

    QVERIFY(assembly.settings.snapshotProvider);
    QVERIFY(assembly.settings.snapshotProvider().settings.functions.isEmpty());

    QString error;
    QVERIFY(!assembly.settings.saveFunctionPrompt(
        QStringLiteral("custom_1"),
        QStringLiteral("内容"),
        &error
    ));
    QVERIFY(!error.isEmpty());

    PromptLibraryItem item;
    item.name = QStringLiteral("测试");
    error.clear();
    QVERIFY(!assembly.panel.createLibraryPromptItem(&item, &error));
    QVERIFY(!error.isEmpty());
}

void PromptAccessFactoryTests::utilityControllerUsesIndependentFactory()
{
    const QString sourcePath = QFINDTESTDATA(
        "../../src/ui/hub_utility_pages_controller.cpp"
    );
    QVERIFY2(!sourcePath.isEmpty(), "找不到辅助页面控制器源文件");
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();

    QVERIFY(contents.contains("createPromptAccessAssembly(dependencies)"));
    QVERIFY(!contents.contains("PromptSettingsAccess access;"));
    QVERIFY(!contents.contains("PromptsPanelAccess access;"));
}

QTEST_APPLESS_MAIN(PromptAccessFactoryTests)

#include "prompt_access_factory_tests.moc"
