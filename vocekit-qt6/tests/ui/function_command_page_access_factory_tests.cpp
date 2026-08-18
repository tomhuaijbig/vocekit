#include <QtTest>

#include "../../src/ui/function_command_page_access_factory.h"
#include "../../src/ui/hub_settings_state.h"

#include <QFile>

class FunctionCommandPageAccessFactoryTests : public QObject
{
    Q_OBJECT

private slots:
    void buildsTypedPageAccess();
    void handlesMissingDependencies();
    void hubWindowUsesIndependentFactory();
};

void FunctionCommandPageAccessFactoryTests::buildsTypedPageAccess()
{
    HubSettingsState settings;
    bool saved = false;
    QStringList functionActions;

    PromptSettingsAccess prompts;
    prompts.snapshotProvider = []() {
        PromptRuntimeSnapshot snapshot;
        snapshot.settings.promptLocked = true;
        return snapshot;
    };

    FunctionCommandPageAccessDependencies dependencies;
    dependencies.settings = &settings;
    dependencies.prompts = prompts;
    dependencies.flows.readState = [](
        const QString &,
        FunctionFlowState *,
        OperationError *
    ) {
        return true;
    };
    dependencies.saveSettings = [&saved]() {
        saved = true;
    };
    dependencies.functionRenamed = [&functionActions](const QString &id) {
        functionActions.append(QStringLiteral("renamed:") + id);
    };
    dependencies.functionRemoved = [&functionActions](const QString &id) {
        functionActions.append(QStringLiteral("removed:") + id);
    };

    const FunctionCommandPageAccess access =
        createFunctionCommandPageAccess(dependencies);

    QCOMPARE(access.settings, &settings);
    QVERIFY(access.prompts.snapshotProvider);
    QVERIFY(access.prompts.snapshotProvider().settings.promptLocked);
    QVERIFY(access.flows.readState);
    QVERIFY(access.saveSettings);
    QVERIFY(access.functionRenamed);
    QVERIFY(access.functionRemoved);
    access.saveSettings();
    access.functionRenamed(QStringLiteral("custom_1"));
    access.functionRemoved(QStringLiteral("custom_2"));
    QVERIFY(saved);
    QCOMPARE(
        functionActions,
        QStringList()
            << QStringLiteral("renamed:custom_1")
            << QStringLiteral("removed:custom_2")
    );
}

void FunctionCommandPageAccessFactoryTests::handlesMissingDependencies()
{
    const FunctionCommandPageAccess access =
        createFunctionCommandPageAccess(FunctionCommandPageAccessDependencies());

    QVERIFY(access.settings == nullptr);
    QVERIFY(!access.flows.readState);
    QVERIFY(!access.saveSettings);
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
