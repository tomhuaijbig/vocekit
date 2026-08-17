#include <QtTest>

#include "../../src/ui/hub_settings_state.h"
#include "../../src/ui/ocr_page_access_factory.h"

#include <QFile>

class OcrPageAccessFactoryTests : public QObject
{
    Q_OBJECT

private slots:
    void buildsTypedSettingsSnapshot();
    void forwardsHistoryNotification();
    void handlesMissingDependencies();
    void contentControllerUsesIndependentFactory();
};

void OcrPageAccessFactoryTests::buildsTypedSettingsSnapshot()
{
    AppSettingsData source;
    source.ocrEngine = QStringLiteral("rapidocr");
    source.ocrTimeoutMs = 32000;
    source.useSystemProxy = true;
    HubWindowAccess stateAccess;
    stateAccess.settingsSnapshotProvider = [source]() { return source; };
    HubSettingsState settings(stateAccess);

    OcrPageAccessFactoryDependencies dependencies;
    dependencies.settings = &settings;
    const OcrPageAccess access = createOcrPageAccess(dependencies);

    QVERIFY(access.settingsSnapshotProvider);
    const AppSettingsData snapshot = access.settingsSnapshotProvider();
    QCOMPARE(snapshot.ocrEngine, QStringLiteral("rapidocr"));
    QCOMPARE(snapshot.ocrTimeoutMs, 32000);
    QVERIFY(snapshot.useSystemProxy);
}

void OcrPageAccessFactoryTests::forwardsHistoryNotification()
{
    QString savedPath;
    OcrPageAccessFactoryDependencies dependencies;
    dependencies.historyRecordSaved = [&savedPath](const QString &filePath) {
        savedPath = filePath;
    };

    const OcrPageAccess access = createOcrPageAccess(dependencies);
    access.historyRecordSaved(QStringLiteral("records/ocr/detail.json"));
    QCOMPARE(savedPath, QStringLiteral("records/ocr/detail.json"));
}

void OcrPageAccessFactoryTests::handlesMissingDependencies()
{
    const OcrPageAccess access = createOcrPageAccess(
        OcrPageAccessFactoryDependencies()
    );
    QVERIFY(access.settingsSnapshotProvider);
    QVERIFY(access.historyRecordSaved);
    QCOMPARE(access.settingsSnapshotProvider().ocrEngine, QStringLiteral("automatic"));
    access.historyRecordSaved(QString());
}

void OcrPageAccessFactoryTests::contentControllerUsesIndependentFactory()
{
    const QString sourcePath = QFINDTESTDATA(
        "../../src/ui/hub_content_pages_controller.cpp"
    );
    QVERIFY2(!sourcePath.isEmpty(), "找不到内容页面控制器源文件");
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();

    QVERIFY(contents.contains("createOcrPageAccess(dependencies)"));
    QVERIFY(!contents.contains("OcrPageAccess access;"));
    QVERIFY(!contents.contains(
        "return m_settings ? m_settings->toData() : AppSettingsData();"
    ));
}

QTEST_APPLESS_MAIN(OcrPageAccessFactoryTests)

#include "ocr_page_access_factory_tests.moc"
