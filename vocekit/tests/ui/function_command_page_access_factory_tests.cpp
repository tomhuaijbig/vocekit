#include <QtTest>

#include "../../src/ui/function_command_page_access_factory.h"
#include "../../src/ui/hub_settings_state.h"

#include <QFile>

class FunctionCommandPageAccessFactoryTests : public QObject
{
    Q_OBJECT

private slots:
    void buildsTypedPageAccess();
    void composesCustomFunctionRefreshActions();
    void handlesMissingDependencies();
    void hubWindowUsesIndependentFactory();
};

void FunctionCommandPageAccessFactoryTests::buildsTypedPageAccess()
{
    HubSettingsState settings;
    bool saved = false;

    PromptSettingsAccess prompts;
    prompts.snapshotProvider = []() {
        PromptRuntimeSnapshot snapshot;
        snapshot.settings.promptLocked = true;
        return snapshot;
    };

    FunctionCommandPageAccessDependencies dependencies;
    dependencies.settings = &settings;
    dependencies.prompts = prompts;
    dependencies.saveSettings = [&saved]() {
        saved = true;
    };

    const FunctionCommandPageAccess access =
        createFunctionCommandPageAccess(dependencies);

    QCOMPARE(access.settings, &settings);
    QVERIFY(access.prompts.snapshotProvider);
    QVERIFY(access.prompts.snapshotProvider().settings.promptLocked);
    QVERIFY(access.saveSettings);
    access.saveSettings();
    QVERIFY(saved);
}

void FunctionCommandPageAccessFactoryTests::composesCustomFunctionRefreshActions()
{
    QStringList actions;
    QString editedId;
    QString editedTitle;
    CustomFunctionDef editedFunction;

    FunctionCommandPageAccessDependencies dependencies;
    dependencies.editCustomFunction = [
        &actions,
        &editedId,
        &editedTitle,
        &editedFunction
    ](
        const QString &id,
        const QString &title,
        const CustomFunctionDef &function
    ) {
        actions.append(QStringLiteral("edit"));
        editedId = id;
        editedTitle = title;
        editedFunction = function;
    };
    const FunctionCommandPageAccess access =
        createFunctionCommandPageAccess(dependencies);
    QVERIFY(access.manageCustomFunction);

    CustomFunctionDef function;
    function.id = QStringLiteral("custom_1");
    function.name = QStringLiteral("润色");
    access.manageCustomFunction(function.id, function.name, function);

    QCOMPARE(actions, QStringList() << QStringLiteral("edit"));
    QCOMPARE(editedId, function.id);
    QCOMPARE(editedTitle, function.name);
    QCOMPARE(editedFunction.id, function.id);
}

void FunctionCommandPageAccessFactoryTests::handlesMissingDependencies()
{
    const FunctionCommandPageAccess access =
        createFunctionCommandPageAccess(FunctionCommandPageAccessDependencies());

    QVERIFY(access.settings == nullptr);
    QVERIFY(!access.saveSettings);
    QVERIFY(access.manageCustomFunction);
    access.manageCustomFunction(QString(), QString(), CustomFunctionDef());
}

void FunctionCommandPageAccessFactoryTests::hubWindowUsesIndependentFactory()
{
    const QString sourcePath = QFINDTESTDATA(
        "../../src/ui/function_pages_access_factory.cpp"
    );
    QVERIFY2(!sourcePath.isEmpty(), "Cannot find function page assembly source");
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();

    QVERIFY(contents.contains(
        "createFunctionCommandPageAccess(commandDependencies)"
    ));
    QVERIFY(!contents.contains("FunctionCommandPageAccess access;"));
}

QTEST_APPLESS_MAIN(FunctionCommandPageAccessFactoryTests)

#include "function_command_page_access_factory_tests.moc"
