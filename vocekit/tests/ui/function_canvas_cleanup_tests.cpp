#include <QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>

class FunctionCanvasCleanupTests : public QObject
{
    Q_OBJECT

private slots:
    void commandPageUsesInlineCanvasMode();
    void commandPageOpensDedicatedCanvasSurface();
    void commandPageUsesSavedOutputOrder();
    void obsoleteEditorDialogIsRemoved();
    void homeCardsOpenTheInlineFunctionPage();
};

namespace
{

QByteArray readSource(const QString &relativePath)
{
    const QString path = QFINDTESTDATA(relativePath.toUtf8().constData());
    if (path.isEmpty())
    {
        return QByteArray();
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return QByteArray();
    }
    return file.readAll();
}

} // namespace

void FunctionCanvasCleanupTests::commandPageUsesInlineCanvasMode()
{
    const QByteArray source = readSource(
        QStringLiteral("../../src/ui/function_command_page.cpp")
    );
    const QByteArray header = readSource(
        QStringLiteral("../../src/ui/function_command_page.h")
    );

    QVERIFY(!source.isEmpty());
    QVERIFY(!header.isEmpty());
    QVERIFY(QString::fromUtf8(source).contains(QStringLiteral("画布")));
    QVERIFY(source.contains("functionCanvasButton"));
    QVERIFY(source.contains("setCheckable(true)"));
    QVERIFY(source.contains("m_canvasMode"));
    QVERIFY(header.contains("m_canvasMode"));
    QVERIFY(!source.contains("manageCustomFunction"));
    QVERIFY(!header.contains("manageCustomFunction"));
}

void FunctionCanvasCleanupTests::commandPageOpensDedicatedCanvasSurface()
{
    const QByteArray pageSource = readSource(
        QStringLiteral("../../src/ui/function_command_page.cpp")
    );
    const QByteArray pageHeader = readSource(
        QStringLiteral("../../src/ui/function_command_page.h")
    );
    const QByteArray editorSource = readSource(
        QStringLiteral("../../src/ui/function_canvas_editor.cpp")
    );
    const QByteArray editorHeader = readSource(
        QStringLiteral("../../src/ui/function_canvas_editor.h")
    );
    const QByteArray canvasSource = readSource(
        QStringLiteral("../../src/ui/function_canvas_view.cpp")
    );
    const QByteArray canvasHeader = readSource(
        QStringLiteral("../../src/ui/function_canvas_view.h")
    );
    const QByteArray project = readSource(QStringLiteral("../../vocekit.pro"));

    QVERIFY(!pageSource.isEmpty());
    QVERIFY(!pageHeader.isEmpty());
    QVERIFY(!editorSource.isEmpty());
    QVERIFY(!editorHeader.isEmpty());
    QVERIFY(!canvasSource.isEmpty());
    QVERIFY(!canvasHeader.isEmpty());
    QVERIFY(pageSource.contains("FunctionCanvasEditor"));
    QVERIFY(pageSource.contains("ensureCanvasEditor"));
    QVERIFY(pageHeader.contains("FunctionCanvasEditor *m_canvasEditor"));
    QVERIFY(editorSource.contains("FunctionCanvasScene"));
    QVERIFY(editorSource.contains("FunctionCanvasView"));
    QVERIFY(editorSource.contains("FunctionCanvasPalette"));
    QVERIFY(editorSource.contains("FunctionCanvasInspector"));
    QVERIFY(canvasSource.contains("functionCanvasSurface"));
    QVERIFY(QString::fromUtf8(pageSource).contains(QStringLiteral("返回设置")));
    QVERIFY(!pageSource.contains("m_canvasMode, voiceBody"));
    QVERIFY(!pageSource.contains("m_canvasMode, selectionBody"));
    QVERIFY(!pageSource.contains("m_canvasMode, screenshotBody"));
    QVERIFY(canvasHeader.contains("QGraphicsView"));
    QVERIFY(canvasSource.contains("setCanvasScene"));
    QVERIFY(canvasSource.contains("drawBackground"));
    QVERIFY(!pageSource.contains("FunctionCanvasScene"));
    QVERIFY(!pageSource.contains("FunctionCanvasNodeItem"));
    QVERIFY(!pageSource.contains("FunctionCanvasEdgeItem"));
    QVERIFY(!pageSource.contains("FunctionFlowPublicationService"));
    QVERIFY(!pageSource.contains("function_flow_json"));
    QVERIFY(!pageHeader.contains("FunctionCanvasScene"));
    QVERIFY(!pageHeader.contains("FunctionCanvasNodeItem"));
    QVERIFY(!pageHeader.contains("FunctionCanvasEdgeItem"));
    QVERIFY(project.contains("src/ui/function_canvas_view.cpp"));
    QVERIFY(project.contains("src/ui/function_canvas_view.h"));
    QVERIFY(project.contains("src/ui/function_canvas_editor.cpp"));
    QVERIFY(project.contains("src/ui/function_canvas_editor.h"));
    QVERIFY(project.contains("src/ui/function_canvas_scene.cpp"));
    QVERIFY(project.contains("src/ui/function_canvas_node_item.cpp"));
    QVERIFY(project.contains("src/ui/function_canvas_edge_item.cpp"));
}

void FunctionCanvasCleanupTests::commandPageUsesSavedOutputOrder()
{
    const QByteArray source = readSource(
        QStringLiteral("../../src/ui/function_command_page.cpp")
    );

    QVERIFY(!source.isEmpty());
    QVERIFY(source.contains("outputOrderFor"));
    QVERIFY(source.contains("setOutputOrderFor"));
    QVERIFY(source.contains("ReorderableCardColumn"));
}

void FunctionCanvasCleanupTests::obsoleteEditorDialogIsRemoved()
{
    const QString projectPath = QFINDTESTDATA("../../vocekit.pro");
    QVERIFY(!projectPath.isEmpty());
    const QDir projectRoot(QFileInfo(projectPath).absolutePath());
    const QString dialogSource =
        projectRoot.filePath(QStringLiteral("src/ui/function_editor_dialog.cpp"));
    const QString dialogHeader =
        projectRoot.filePath(QStringLiteral("src/ui/function_editor_dialog.h"));
    const QByteArray project = readSource(QStringLiteral("../../vocekit.pro"));
    const QByteArray hubWorkspace = readSource(
        QStringLiteral("../../src/ui/hub_function_workspace_controller.cpp")
    );

    QVERIFY(!QFileInfo::exists(dialogSource));
    QVERIFY(!QFileInfo::exists(dialogHeader));
    QVERIFY(!project.contains("function_editor_dialog.cpp"));
    QVERIFY(!project.contains("function_editor_dialog.h"));
    QVERIFY(!project.contains("function_editor_dialog_access_factory"));
    QVERIFY(!hubWorkspace.contains("runFunctionEditorDialog"));
    QVERIFY(!hubWorkspace.contains("openEditorDialog"));
}

void FunctionCanvasCleanupTests::homeCardsOpenTheInlineFunctionPage()
{
    const QByteArray homeHeader = readSource(
        QStringLiteral("../../src/ui/home_page.h")
    );
    const QByteArray factoryHeader = readSource(
        QStringLiteral("../../src/ui/home_page_access_factory.h")
    );
    const QByteArray modeHeader = readSource(
        QStringLiteral("../../src/ui/function_mode_grid.h")
    );

    QVERIFY(homeHeader.contains("openFunction"));
    QVERIFY(!homeHeader.contains("editFunction"));
    QVERIFY(factoryHeader.contains("openFunction"));
    QVERIFY(!factoryHeader.contains("editFunction"));
    QVERIFY(modeHeader.contains("OpenCallback"));
    QVERIFY(!modeHeader.contains("EditCallback"));
}

QTEST_APPLESS_MAIN(FunctionCanvasCleanupTests)

#include "function_canvas_cleanup_tests.moc"
