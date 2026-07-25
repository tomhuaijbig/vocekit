#include <QtTest>

#include "../../src/ui/hub_utility_pages_controller.h"

#include <QFile>

#include <type_traits>

namespace {

QByteArray sourceSection(
    const QByteArray &source,
    const QByteArray &beginMarker,
    const QByteArray &endMarker
)
{
    const int begin = source.indexOf(beginMarker);
    if (begin < 0) {
        return QByteArray();
    }
    if (endMarker.isEmpty()) {
        return source.mid(begin);
    }
    const int end = source.indexOf(endMarker, begin + beginMarker.size());
    return end < 0 ? source.mid(begin) : source.mid(begin, end - begin);
}

} // namespace

class HubUtilityPagesControllerTests : public QObject
{
    Q_OBJECT

private slots:
    void exposesOneTypedPageController();
    void hubWindowDelegatesUtilityPages();
    void promptMutationsUseOneSettingsRefreshRoute();
};

void HubUtilityPagesControllerTests::exposesOneTypedPageController()
{
    QVERIFY((std::is_constructible<
        HubUtilityPagesController,
        const HubUtilityPagesControllerAccess &
    >::value));
}

void HubUtilityPagesControllerTests::hubWindowDelegatesUtilityPages()
{
    const QString hubPath = QFINDTESTDATA("../../src/ui/hub_window.cpp");
    const QString controllerPath = QFINDTESTDATA(
        "../../src/ui/hub_utility_pages_controller.cpp"
    );
    QVERIFY2(!hubPath.isEmpty(), "HubWindow source file not found");
    QVERIFY2(!controllerPath.isEmpty(), "Utility page controller source not found");

    QFile hubSource(hubPath);
    QVERIFY(hubSource.open(QIODevice::ReadOnly));
    const QByteArray hubContents = hubSource.readAll();

    QFile controllerSource(controllerPath);
    QVERIFY(controllerSource.open(QIODevice::ReadOnly));
    const QByteArray controllerContents = controllerSource.readAll();

    QVERIFY(hubContents.contains("HubUtilityPagesController"));
    QVERIFY(hubContents.contains("utilityPagesController()->promptsPage()"));
    QVERIFY(hubContents.contains("utilityPagesController()->logsPage()"));
    QVERIFY(hubContents.contains("utilityPagesController()->settingsPage()"));
    QVERIFY(!hubContents.contains("m_promptsPanel"));
    QVERIFY(!hubContents.contains("m_logsPanel"));
    QVERIFY(!hubContents.contains("m_diagnosticsPanel"));
    QVERIFY(!hubContents.contains("m_settingsPanel"));
    QVERIFY(!hubContents.contains("m_faqPanel"));
    QVERIFY(!hubContents.contains("new PromptsPanel"));
    QVERIFY(!hubContents.contains("new LogsPanel"));
    QVERIFY(!hubContents.contains("createDiagnosticsPanel"));
    QVERIFY(!hubContents.contains("new SettingsPanel"));
    QVERIFY(!hubContents.contains("new FaqPanel"));

    QVERIFY(controllerContents.contains("new PromptsPanel"));
    QVERIFY(controllerContents.contains("new LogsPanel"));
    QVERIFY(controllerContents.contains("createDiagnosticsPanel"));
    QVERIFY(controllerContents.contains("new SettingsPanel"));
    QVERIFY(controllerContents.contains("new FaqPanel"));
    QVERIFY(!controllerContents.contains("refreshActiveFunctionPage"));
    QVERIFY(!controllerContents.contains("refreshModeGrid"));
    QVERIFY(!hubContents.contains("access.refreshActiveFunctionPage"));
    QVERIFY(!hubContents.contains("access.refreshModeGrid"));
}

void HubUtilityPagesControllerTests::
promptMutationsUseOneSettingsRefreshRoute()
{
    const QString panelPath = QFINDTESTDATA(
        "../../src/ui/prompts_panel.cpp"
    );
    QVERIFY2(!panelPath.isEmpty(), "PromptsPanel source file not found");

    QFile panelSource(panelPath);
    QVERIFY(panelSource.open(QIODevice::ReadOnly));
    const QByteArray contents = panelSource.readAll();

    const QByteArray saveSection = sourceSection(
        contents,
        "void PromptsPanel::savePromptFromEditor()",
        "void PromptsPanel::addPromptLibraryItemFromUi()"
    );
    const QByteArray addSection = sourceSection(
        contents,
        "void PromptsPanel::addPromptLibraryItemFromUi()",
        "void PromptsPanel::duplicateCurrentPrompt()"
    );
    const QByteArray duplicateSection = sourceSection(
        contents,
        "void PromptsPanel::duplicateCurrentPrompt()",
        "void PromptsPanel::deleteCurrentPrompt()"
    );
    const QByteArray deleteSection = sourceSection(
        contents,
        "void PromptsPanel::deleteCurrentPrompt()",
        "void PromptsPanel::notifySettingsChanged()"
    );
    const QByteArray notifySection = sourceSection(
        contents,
        "void PromptsPanel::notifySettingsChanged()",
        QByteArray()
    );

    QVERIFY(!saveSection.isEmpty());
    QVERIFY(!addSection.isEmpty());
    QVERIFY(!duplicateSection.isEmpty());
    QVERIFY(!deleteSection.isEmpty());
    QVERIFY(!notifySection.isEmpty());

    const QList<QByteArray> mutationSections = {
        saveSection,
        addSection,
        duplicateSection,
        deleteSection
    };
    for (const QByteArray &section : mutationSections) {
        QVERIFY(section.contains("notifySettingsChanged();"));
        QVERIFY(!section.contains("refresh();"));
    }

    QVERIFY(notifySection.contains("if (m_settingsChanged)"));
    QVERIFY(notifySection.contains("} else {"));
    QVERIFY(notifySection.contains("refresh();"));
}

QTEST_MAIN(HubUtilityPagesControllerTests)

#include "hub_utility_pages_controller_tests.moc"
