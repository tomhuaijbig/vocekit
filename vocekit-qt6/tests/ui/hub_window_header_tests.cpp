#include <QtTest>

#include "../../src/ui/hub_window.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <type_traits>

class HubWindowHeaderTests : public QObject
{
    Q_OBJECT

private slots:
    void exposesNarrowWindowInterface();
    void bootstrapIsAThinCompatibilityEntry();
    void applicationRuntimeDoesNotDefineHubWindowImplementation();
    void applicationRuntimeUsesSettingsStoreAsThePersistenceBoundary();
    void everyApplicationExitUsesTheDraftFlushGate();
    void staleOrdinarySettingsAreReloadedWithoutAutomaticReplay();
    void doesNotEmbedPageMethodFragments();
    void doesNotKeepLegacySettingsRows();
};

void HubWindowHeaderTests::exposesNarrowWindowInterface()
{
    QVERIFY((std::is_base_of<QMainWindow, HubWindow>::value));
    QVERIFY((std::is_base_of<VoiceControllerHost, HubWindow>::value));
    QVERIFY(std::is_abstract<HubWindow>::value);
    QVERIFY(std::is_pointer<decltype(&createHubWindow)>::value);
}

void HubWindowHeaderTests::bootstrapIsAThinCompatibilityEntry()
{
    const QString sourcePath = QFINDTESTDATA("../../src/voiceassistant.cpp");
    QVERIFY2(!sourcePath.isEmpty(), "找不到程序启动源文件");
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();

    QVERIFY2(contents.count('\n') < 100,
             "voiceassistant.cpp 应只保留兼容入口");
    QVERIFY(contents.contains("runVocekitApplication(argc, argv)"));
    QVERIFY(!contents.contains("QApplication app"));
    QVERIFY(!contents.contains("AppSettingsStore settingsStore"));
    QVERIFY(!contents.contains("createHubWindow"));
}

void HubWindowHeaderTests::applicationRuntimeDoesNotDefineHubWindowImplementation()
{
    const QString sourcePath =
        QFINDTESTDATA("../../src/app/vocekit_application_runtime.cpp");
    QVERIFY2(!sourcePath.isEmpty(), "找不到应用运行组装源文件");
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();

    QVERIFY(!contents.contains("class HubWindow"));
    QVERIFY(contents.contains("createHubWindow"));
    QVERIFY(contents.contains(
        "int runVocekitApplication(int argc, char *argv[])"));
}

void HubWindowHeaderTests::applicationRuntimeUsesSettingsStoreAsThePersistenceBoundary()
{
    const QString sourcePath =
        QFINDTESTDATA("../../src/app/vocekit_application_runtime.cpp");
    QVERIFY2(!sourcePath.isEmpty(), "找不到应用运行组装源文件");
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();

    QVERIFY(contents.contains("AppSettingsStore settingsStore"));
    QVERIFY(contents.contains("settingsStore.loadOrCreateDefaults"));
    QVERIFY(contents.contains("settingsStore.replaceAndSave"));
    QCOMPARE(contents.count("settingsStore.replaceAndSave"), 1);
    QVERIFY(contents.contains("publicationAccess.replaceAndSave"));
    QVERIFY(contents.contains("replaceNonFlowSettingsAndSave"));
    QVERIFY(!contents.contains("hubAccess.applyAndSave"));
    QVERIFY(contents.contains("FunctionFlowPublicationService"));
    QVERIFY(contents.contains("hubAccess.functionFlows = functionFlows"));
    QVERIFY(contents.contains("promptRuntimeTargets"));
    QVERIFY(contents.contains("supportedSpeechProviderIds"));
    QVERIFY(contents.contains("supportedOcrEngineIds"));
    QVERIFY(
        contents.indexOf("ApplicationEvents events")
        < contents.indexOf("FunctionFlowPublicationService publicationService")
    );
    QVERIFY(
        contents.indexOf("FunctionFlowPublicationService publicationService")
        < contents.indexOf("HubWindowAccess hubAccess")
    );
    QVERIFY(!contents.contains("app_settings_json.h"));
    QVERIFY(!contents.contains("settingsStore.replaceSnapshot"));
    QVERIFY(!contents.contains("settingsStore.save("));
    QVERIFY(!contents.contains("legacy_app_settings"));
}

void HubWindowHeaderTests::everyApplicationExitUsesTheDraftFlushGate()
{
    QVERIFY(std::is_member_function_pointer<
        decltype(&HubWindow::requestApplicationQuit)
    >::value);

    const QString sourcePath = QFINDTESTDATA("../../src/ui/hub_window.cpp");
    QVERIFY2(!sourcePath.isEmpty(), "Cannot find HubWindow source");
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();

    QVERIFY(contents.contains("bool requestApplicationQuit() override"));
    QVERIFY(contents.contains("flushAllPendingFlowDrafts"));
    QVERIFY(contents.contains("discardAllPendingFlowDrafts"));
    QVERIFY(contents.contains("requestApplicationQuit();"));
    QVERIFY(contents.contains("tr8(\"重试\")"));
    QVERIFY(contents.contains("tr8(\"取消退出\")"));
    QVERIFY(contents.contains("tr8(\"丢弃草稿并退出\")"));
    QCOMPARE(contents.count("qApp->quit()"), 1);
}

void HubWindowHeaderTests::staleOrdinarySettingsAreReloadedWithoutAutomaticReplay()
{
    const QString sourcePath = QFINDTESTDATA("../../src/ui/hub_window.cpp");
    QVERIFY2(!sourcePath.isEmpty(), "Cannot find HubWindow source");
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();

    QVERIFY(contents.contains("settings_function_set_stale"));
    QVERIFY(contents.contains("m_settings->load();"));
    QVERIFY(contents.contains("tr8(\"设置已更新\")"));
    QVERIFY(contents.contains("tr8(\"请检查最新设置后重新操作。\")"));
}

void HubWindowHeaderTests::doesNotEmbedPageMethodFragments()
{
    const QString sourcePath = QFINDTESTDATA("../../src/ui/hub_window.cpp");
    QVERIFY2(!sourcePath.isEmpty(), "找不到 HubWindow 源文件");
    QDir repositoryRoot(QFileInfo(sourcePath).absolutePath());
    QVERIFY(repositoryRoot.cdUp());
    QVERIFY(repositoryRoot.cdUp());

    const QStringList fragmentPaths = {
        QStringLiteral("src/pages/hub_history_page_methods.h"),
        QStringLiteral("src/pages/hub_vocabulary_page_methods.h"),
        QStringLiteral("src/pages/hub_ocr_page_methods.h")
    };
    for (const QString &path : fragmentPaths) {
        QVERIFY2(!QFileInfo::exists(repositoryRoot.filePath(path)),
                 qPrintable(path));
    }

    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();
    QVERIFY(!contents.contains("hub_history_page_methods.h"));
    QVERIFY(!contents.contains("hub_vocabulary_page_methods.h"));
    QVERIFY(!contents.contains("hub_ocr_page_methods.h"));

    const QString projectPath = QFINDTESTDATA("../../vocekit.pro");
    QVERIFY2(!projectPath.isEmpty(), "找不到项目文件");
    QFile project(projectPath);
    QVERIFY(project.open(QIODevice::ReadOnly));
    const QByteArray projectContents = project.readAll();
    QVERIFY(!projectContents.contains("hub_history_page_methods.h"));
    QVERIFY(!projectContents.contains("hub_vocabulary_page_methods.h"));
    QVERIFY(!projectContents.contains("hub_ocr_page_methods.h"));
}

void HubWindowHeaderTests::doesNotKeepLegacySettingsRows()
{
    const QString sourcePath = QFINDTESTDATA("../../src/ui/hub_window.cpp");
    QVERIFY2(!sourcePath.isEmpty(), "找不到 HubWindow 源文件");
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();
    QVERIFY(!contents.contains("hubToggleRow("));
    QVERIFY(!contents.contains("hubRecordDirectoryRow("));
    QVERIFY(!contents.contains("settingsLinkRow("));
    QVERIFY(!contents.contains("chooseRecordDirectoryFromHub("));
    QVERIFY(!contents.contains("refreshRecordDirectoryViews("));
    QVERIFY(!contents.contains("m_hubRecordDirectoryLabel"));
}

QTEST_APPLESS_MAIN(HubWindowHeaderTests)

#include "hub_window_header_tests.moc"
