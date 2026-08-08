#include <QtTest>

#include "../../src/ui/function_management_page_access_factory.h"
#include "../../src/ui/hub_settings_state.h"

#include <QFile>

namespace {

AppSettingsData sampleSettings()
{
    AppSettingsData settings;

    FunctionSettings dictate;
    dictate.id = QStringLiteral("dictate");
    dictate.name = QStringLiteral("听写");
    dictate.builtIn = true;
    dictate.shortcut = QStringLiteral("Alt+X");
    settings.functions.append(dictate);

    FunctionSettings custom;
    custom.id = QStringLiteral("custom_1");
    custom.name = QStringLiteral("润色");
    custom.shortcut = QStringLiteral("Ctrl+Alt+1");
    custom.builtIn = false;
    settings.functions.append(custom);
    settings.functionOrder << dictate.id << custom.id;
    return settings;
}

} // namespace

class FunctionManagementPageAccessFactoryTests : public QObject
{
    Q_OBJECT

private slots:
    void buildsBuiltInAndCustomItems();
    void forwardsAddAndEditActions();
    void removesCustomFunctionThroughTheTransaction();
    void failedRemovalLeavesTheStateUnchanged();
    void handlesMissingDependencies();
    void hubWindowUsesIndependentFactory();
};

void FunctionManagementPageAccessFactoryTests::buildsBuiltInAndCustomItems()
{
    const AppSettingsData source = sampleSettings();
    HubWindowAccess stateAccess;
    stateAccess.settingsSnapshotProvider = [source]() { return source; };
    HubSettingsState settings(stateAccess);

    FunctionManagementPageAccessDependencies dependencies;
    dependencies.settings = &settings;
    dependencies.summaryProvider = [](
        const QString &id,
        const QString &shortcut
    ) {
        return id + QStringLiteral(":") + shortcut;
    };

    const FunctionManagementPageAccess access =
        createFunctionManagementPageAccess(dependencies);
    QVERIFY(access.itemsProvider);
    const QVector<FunctionManagementItem> items = access.itemsProvider();

    QCOMPARE(items.size(), 4);
    QCOMPARE(items.at(0).id, QStringLiteral("dictate"));
    QCOMPARE(items.at(0).shortcut, QStringLiteral("Alt+X"));
    QCOMPARE(items.at(0).summary, QStringLiteral("dictate:Alt+X"));
    QVERIFY(!items.at(0).custom);

    const FunctionManagementItem custom = items.constLast();
    QCOMPARE(custom.id, QStringLiteral("custom_1"));
    QCOMPARE(custom.title, QStringLiteral("润色"));
    QCOMPARE(custom.shortcut, QStringLiteral("Ctrl+Alt+1"));
    QCOMPARE(custom.summary, QStringLiteral("custom_1:Ctrl+Alt+1"));
    QVERIFY(custom.custom);
    QCOMPARE(custom.function.id, custom.id);
}

void FunctionManagementPageAccessFactoryTests::forwardsAddAndEditActions()
{
    QStringList actions;
    FunctionManagementPageAccessDependencies dependencies;
    dependencies.addFunction = [&actions]() {
        actions.append(QStringLiteral("add"));
    };
    dependencies.editFunction = [&actions](
        const QString &id,
        const QString &,
        bool,
        const CustomFunctionDef &
    ) {
        actions.append(QStringLiteral("edit:") + id);
    };

    const FunctionManagementPageAccess access =
        createFunctionManagementPageAccess(dependencies);
    QVERIFY(access.addFunction);
    QVERIFY(access.editFunction);

    access.addFunction();
    FunctionManagementItem item;
    item.id = QStringLiteral("custom_2");
    item.title = QStringLiteral("总结");
    item.custom = true;
    item.function.id = item.id;
    access.editFunction(item);

    QCOMPARE(
        actions,
        QStringList()
            << QStringLiteral("add")
            << QStringLiteral("edit:custom_2")
    );
}

void FunctionManagementPageAccessFactoryTests::
removesCustomFunctionThroughTheTransaction()
{
    AppSettingsData source = sampleSettings();
    QStringList actions;
    HubWindowAccess stateAccess;
    stateAccess.settingsSnapshotProvider = [&source]() { return source; };
    HubSettingsState settings(stateAccess);

    FunctionManagementPageAccessDependencies dependencies;
    dependencies.settings = &settings;
    dependencies.flows.removeCustomFunction = [&](
        const QString &id,
        OperationError *
    ) {
        actions.append(QStringLiteral("remove:") + id);
        const int index = source.functionIndex(id);
        source.functions.remove(index);
        source.functionOrder.removeAll(id);
        return true;
    };

    const FunctionManagementPageAccess access =
        createFunctionManagementPageAccess(dependencies);
    QVERIFY(access.removeFunction);

    FunctionManagementItem item;
    item.id = QStringLiteral("custom_1");
    item.custom = true;
    access.removeFunction(item);

    QVERIFY(settings.customFunctions().isEmpty());
    QCOMPARE(
        actions,
        QStringList() << QStringLiteral("remove:custom_1")
    );
}

void FunctionManagementPageAccessFactoryTests::
failedRemovalLeavesTheStateUnchanged()
{
    AppSettingsData source = sampleSettings();
    HubWindowAccess stateAccess;
    stateAccess.settingsSnapshotProvider = [&source]() { return source; };
    HubSettingsState settings(stateAccess);
    int failures = 0;
    FunctionManagementPageAccessDependencies dependencies;
    dependencies.settings = &settings;
    dependencies.flows.removeCustomFunction = [](
        const QString &,
        OperationError *error
    ) {
        error->code = QStringLiteral("flow_save_failed");
        return false;
    };
    dependencies.operationFailed = [&failures](
        const OperationError &
    ) {
        ++failures;
    };
    const FunctionManagementPageAccess access =
        createFunctionManagementPageAccess(dependencies);
    FunctionManagementItem item;
    item.id = QStringLiteral("custom_1");
    item.custom = true;
    access.removeFunction(item);
    QCOMPARE(settings.customFunctions().size(), 1);
    QCOMPARE(source.functions.size(), 2);
    QCOMPARE(failures, 1);
}

void FunctionManagementPageAccessFactoryTests::handlesMissingDependencies()
{
    const FunctionManagementPageAccess access =
        createFunctionManagementPageAccess(
            FunctionManagementPageAccessDependencies()
        );

    QVERIFY(access.itemsProvider);
    QVERIFY(access.itemsProvider().isEmpty());
    QVERIFY(access.editFunction);
    QVERIFY(access.removeFunction);
    access.editFunction(FunctionManagementItem());
    access.removeFunction(FunctionManagementItem());
}

void FunctionManagementPageAccessFactoryTests::hubWindowUsesIndependentFactory()
{
    const QString sourcePath = QFINDTESTDATA(
        "../../src/ui/function_pages_access_factory.cpp"
    );
    QVERIFY2(!sourcePath.isEmpty(), "Cannot find function page assembly source");
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();

    QVERIFY(contents.contains("createFunctionManagementPageAccess("));
    QVERIFY(contents.contains("managementDependencies"));
    QVERIFY(!contents.contains("FunctionManagementPageAccess access;"));
    QVERIFY(!contents.contains("QVector<FunctionManagementItem> items;"));
}

QTEST_APPLESS_MAIN(FunctionManagementPageAccessFactoryTests)

#include "function_management_page_access_factory_tests.moc"
