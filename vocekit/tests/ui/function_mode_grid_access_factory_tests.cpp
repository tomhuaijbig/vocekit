#include <QtTest>

#include "../../src/ui/function_mode_grid_access_factory.h"
#include "../../src/ui/hub_settings_state.h"

#include <QFile>

namespace {

FunctionSettings functionSettings(
    const QString &id,
    const QString &name,
    bool builtIn
)
{
    FunctionSettings settings;
    settings.id = id;
    settings.name = name;
    settings.builtIn = builtIn;
    settings.shortcut = QStringLiteral("Alt+X");
    settings.modelId = QStringLiteral("test-model");
    settings.input.useSelection = id != QStringLiteral("dictate");
    settings.input.useVoice = id == QStringLiteral("dictate");
    settings.input.useScreenshot = id == QStringLiteral("custom_1");
    settings.output.outputMode = QStringLiteral("resultPopup");
    return settings;
}

AppSettingsData sampleSettings()
{
    AppSettingsData settings;
    settings.functions = {
        functionSettings(QStringLiteral("dictate"), QStringLiteral("听写"), true),
        functionSettings(QStringLiteral("translate"), QStringLiteral("翻译"), true),
        functionSettings(QStringLiteral("ask"), QStringLiteral("问答"), true),
        functionSettings(QStringLiteral("custom_1"), QStringLiteral("润色"), false)
    };
    settings.functionOrder = QStringList()
        << QStringLiteral("custom_1")
        << QStringLiteral("dictate")
        << QStringLiteral("translate")
        << QStringLiteral("ask");
    return settings;
}

const FunctionModeCardSnapshot *cardById(
    const FunctionModeGridSnapshot &snapshot,
    const QString &id
)
{
    for (const FunctionModeCardSnapshot &card : snapshot.cards) {
        if (card.id == id) {
            return &card;
        }
    }
    return nullptr;
}

} // namespace

class FunctionModeGridAccessFactoryTests : public QObject
{
    Q_OBJECT

private slots:
    void buildsSnapshotFromTypedSettings();
    void savesOrderAndRollsBackWhenPersistenceFails();
    void handlesMissingSettingsState();
    void hubWindowUsesIndependentFactory();
};

void FunctionModeGridAccessFactoryTests::buildsSnapshotFromTypedSettings()
{
    const AppSettingsData source = sampleSettings();
    HubWindowAccess stateAccess;
    stateAccess.settingsSnapshotProvider = [source]() { return source; };
    HubSettingsState state(stateAccess);

    const FunctionModeGridAccess access =
        createFunctionModeGridAccess(&state);
    QVERIFY(access.snapshotProvider);

    const FunctionModeGridSnapshot snapshot = access.snapshotProvider();
    QCOMPARE(snapshot.cards.size(), 4);
    QCOMPARE(snapshot.order, source.functionOrder);

    const FunctionModeCardSnapshot *dictate =
        cardById(snapshot, QStringLiteral("dictate"));
    QVERIFY(dictate != nullptr);
    QCOMPARE(dictate->title, QStringLiteral("听写"));
    QCOMPARE(dictate->shortcut, QStringLiteral("Alt+X"));
    QCOMPARE(dictate->model, QStringLiteral("test-model"));
    QVERIFY(dictate->useVoice);
    QVERIFY(!dictate->custom);

    const FunctionModeCardSnapshot *custom =
        cardById(snapshot, QStringLiteral("custom_1"));
    QVERIFY(custom != nullptr);
    QCOMPARE(custom->title, QStringLiteral("润色"));
    QVERIFY(custom->useSelection);
    QVERIFY(custom->useScreenshot);
    QVERIFY(custom->custom);
    QCOMPARE(custom->customFunction.id, QStringLiteral("custom_1"));
}

void FunctionModeGridAccessFactoryTests::savesOrderAndRollsBackWhenPersistenceFails()
{
    AppSettingsData source = sampleSettings();
    AppSettingsData persisted;
    bool allowSave = true;

    HubWindowAccess stateAccess;
    stateAccess.settingsSnapshotProvider = [source]() { return source; };
    stateAccess.applyAndSave = [&persisted, &allowSave](const AppSettingsData &data) {
        persisted = data;
        return allowSave;
    };
    HubSettingsState state(stateAccess);
    const FunctionModeGridAccess access =
        createFunctionModeGridAccess(&state);

    QString error;
    const QStringList savedOrder = {
        QStringLiteral("dictate"),
        QStringLiteral("translate"),
        QStringLiteral("ask"),
        QStringLiteral("custom_1")
    };
    QVERIFY(access.saveOrder(savedOrder, &error));
    QVERIFY(error.isEmpty());
    QCOMPARE(persisted.functionOrder, savedOrder);
    QCOMPARE(state.functionOrderIds(), savedOrder);

    allowSave = false;
    const QStringList rejectedOrder = {
        QStringLiteral("custom_1"),
        QStringLiteral("ask"),
        QStringLiteral("translate"),
        QStringLiteral("dictate")
    };
    QVERIFY(!access.saveOrder(rejectedOrder, &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(state.functionOrderIds(), savedOrder);
}

void FunctionModeGridAccessFactoryTests::handlesMissingSettingsState()
{
    const FunctionModeGridAccess access =
        createFunctionModeGridAccess(nullptr);
    QVERIFY(access.snapshotProvider);
    QVERIFY(access.saveOrder);
    QVERIFY(access.snapshotProvider().cards.isEmpty());

    QString error;
    QVERIFY(!access.saveOrder(QStringList() << QStringLiteral("dictate"), &error));
    QVERIFY(!error.isEmpty());
}

void FunctionModeGridAccessFactoryTests::hubWindowUsesIndependentFactory()
{
    const QString hubPath = QFINDTESTDATA("../../src/ui/hub_window.cpp");
    const QString factoryPath =
        QFINDTESTDATA("../../src/ui/home_page_access_factory.cpp");
    QVERIFY2(!hubPath.isEmpty(), "找不到 HubWindow 源文件");
    QVERIFY2(!factoryPath.isEmpty(), "找不到主页访问工厂源文件");

    QFile hub(hubPath);
    QVERIFY(hub.open(QIODevice::ReadOnly));
    const QByteArray hubContents = hub.readAll();
    QVERIFY(hubContents.contains("createHomePageAccess(dependencies)"));
    QVERIFY(!hubContents.contains("createFunctionModeGridAccess"));
    QVERIFY(!hubContents.contains("FunctionModeGridAccess functionModeGridAccess()"));
    QVERIFY(!hubContents.contains("FunctionModeCardSnapshot card"));

    QFile factory(factoryPath);
    QVERIFY(factory.open(QIODevice::ReadOnly));
    const QByteArray factoryContents = factory.readAll();
    QVERIFY(factoryContents.contains(
        "createFunctionModeGridAccess(dependencies.settings)"
    ));
}

QTEST_APPLESS_MAIN(FunctionModeGridAccessFactoryTests)

#include "function_mode_grid_access_factory_tests.moc"
