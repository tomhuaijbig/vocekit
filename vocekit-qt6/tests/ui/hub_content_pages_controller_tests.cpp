#include <QtTest>

#include "../../src/ui/hub_content_pages_controller.h"

#include <QFile>

#include <type_traits>

class HubContentPagesControllerTests : public QObject
{
    Q_OBJECT

private slots:
    void exposesTypedContentPageInterface();
    void hubWindowDelegatesContentPageOwnership();
};

void HubContentPagesControllerTests::exposesTypedContentPageInterface()
{
    QVERIFY((std::is_constructible<
        HubContentPagesController,
        const HubContentPagesControllerAccess &
    >::value));
}

void HubContentPagesControllerTests::hubWindowDelegatesContentPageOwnership()
{
    const QString hubPath = QFINDTESTDATA("../../src/ui/hub_window.cpp");
    const QString controllerPath = QFINDTESTDATA(
        "../../src/ui/hub_content_pages_controller.cpp"
    );
    QVERIFY2(!hubPath.isEmpty(), "HubWindow source file not found");
    QVERIFY2(!controllerPath.isEmpty(), "Content page controller source not found");

    QFile hubSource(hubPath);
    QVERIFY(hubSource.open(QIODevice::ReadOnly));
    const QByteArray hubContents = hubSource.readAll();

    QFile controllerSource(controllerPath);
    QVERIFY(controllerSource.open(QIODevice::ReadOnly));
    const QByteArray controllerContents = controllerSource.readAll();

    QVERIFY(hubContents.contains("HubContentPagesController"));
    QVERIFY(hubContents.contains("contentPagesController()->historyPage()"));
    QVERIFY(hubContents.contains("contentPagesController()->vocabularyPage()"));
    QVERIFY(hubContents.contains("contentPagesController()->ocrPage()"));
    QVERIFY(hubContents.contains("contentPagesController()->historyRefreshDataAccess()"));
    QVERIFY(!hubContents.contains("QScopedPointer<HistoryPageController>"));
    QVERIFY(!hubContents.contains("QScopedPointer<VocabularyPageController>"));
    QVERIFY(!hubContents.contains("QScopedPointer<OcrPageController>"));
    QVERIFY(!hubContents.contains("new HistoryPageController"));
    QVERIFY(!hubContents.contains("new VocabularyPageController"));
    QVERIFY(!hubContents.contains("new OcrPageController"));
    QVERIFY(!hubContents.contains("createHistoryPageAccess"));
    QVERIFY(!hubContents.contains("createVocabularyPageAccess"));
    QVERIFY(!hubContents.contains("createOcrPageAccess"));

    QVERIFY(controllerContents.contains("new HistoryPageController"));
    QVERIFY(controllerContents.contains("new VocabularyPageController"));
    QVERIFY(controllerContents.contains("new OcrPageController"));
    QVERIFY(controllerContents.contains(
        "createHistoryPageAccess(access.settings, access.historyChanged)"
    ));
    QVERIFY(controllerContents.contains("createVocabularyPageAccess"));
    QVERIFY(controllerContents.contains("createOcrPageAccess"));
}

QTEST_MAIN(HubContentPagesControllerTests)

#include "hub_content_pages_controller_tests.moc"
