#include <QtTest>

#define private public
#include "../../src/capture/screenshot_result_window.h"
#undef private
#include "../../src/controllers/function_flow_result_controller.h"
#include "../../src/providers/model_catalog.h"
#include "../../src/ui/attention_message.h"
#include "../../src/ui/result_choice_popup.h"

#include <QApplication>
#include <QComboBox>
#include <QInputDialog>
#include <QPushButton>
#include <QTextEdit>
#include <QTimer>

QVector<ModelOption> modelOptions()
{
    return QVector<ModelOption>();
}

QString normalizeModelId(
    const QString &value,
    const QString &fallback)
{
    const QString trimmed = value.trimmed();
    if (trimmed == QStringLiteral("openai:gpt-5.4")) {
        return QStringLiteral("openai:gpt-5.6-terra");
    }
    return trimmed.isEmpty() ? fallback : trimmed;
}

void showAttentionInformation(
    QWidget *,
    const QString &,
    const QString &)
{
}

namespace {

ResultChoicePopup *visiblePopup()
{
    for (QWidget *widget : QApplication::topLevelWidgets()) {
        auto *popup = dynamic_cast<ResultChoicePopup *>(widget);
        if (popup && popup->isVisible()) {
            return popup;
        }
    }
    return nullptr;
}

ScreenshotResultWindow *visibleScreenshotWindow()
{
    for (QWidget *widget : QApplication::topLevelWidgets()) {
        auto *window =
            dynamic_cast<ScreenshotResultWindow *>(widget);
        if (window && window->isVisible()) {
            return window;
        }
    }
    return nullptr;
}

FunctionFlowRunContext runContext(
    const CancellationSource &cancellation,
    int inheritedOpacity = 81)
{
    FunctionFlowRunContext run;
    run.runId = cancellation.executionId();
    run.functionId = QStringLiteral("flow-function");
    run.targetWindow = reinterpret_cast<void *>(quintptr(44));
    run.cancellation = cancellation.token();
    QSharedPointer<FunctionFlowResolvedDependencies> deps(
        new FunctionFlowResolvedDependencies
    );
    deps->functionTitle = QString::fromUtf8("流程功能");
    deps->inheritedResultPopupOpacity = inheritedOpacity;
    run.dependencies = deps;
    return run;
}

FunctionFlowCompiledNode popupNode(
    const QString &templateId = QStringLiteral("simple"))
{
    FunctionFlowCompiledNode node;
    node.nodeId = QStringLiteral("popup");
    node.type = FunctionFlowNodeType::ResultPopup;
    node.config.popup.resultTemplate = templateId;
    node.config.popup.resultActions =
        QStringList()
            << QStringLiteral("copy")
            << QStringLiteral("write")
            << QStringLiteral("replace");
    node.config.popup.opacity = -1;
    return node;
}

FunctionFlowResultActionRequest actionRequest()
{
    FunctionFlowResultActionRequest request;
    request.canonicalInput =
        QString::fromUtf8("冻结的规范输入");
    request.output.text =
        QString::fromUtf8("冻结的最终输出");
    request.collectedSelection = true;
    return request;
}

void closeAllPopups()
{
    for (QWidget *widget : QApplication::topLevelWidgets()) {
        if (dynamic_cast<ResultChoicePopup *>(widget)
            || dynamic_cast<ScreenshotResultWindow *>(widget)) {
            widget->close();
        }
    }
    QApplication::processEvents();
}

} // namespace

class FunctionFlowResultControllerTests : public QObject
{
    Q_OBJECT

private slots:
    void cleanup();
    void templatesUseCanonicalInputAndOutput();
    void streamingPreviewIsReusedAndOnlyFinalIsEditable();
    void closingBusyPreviewRequestsCancellation();
    void checkedWriteFailureKeepsPopupOpen();
    void editedSurfaceClosesExactlyOnce();
    void inheritedOpacityIsFrozen();
    void previewDoesNotAutoCloseBeforeFinalTakeover();
    void autoWriteUsesFrozenSelectionAndShowsOneFallback();
    void screenshotPanelUsesPayloadAndCheckedWrites();
    void screenshotRetryDialogSelectsTheMigratedCurrentModel();
    void screenshotRetryDialogUsesTheSelectedDuplicateTitleIndex();
};

void FunctionFlowResultControllerTests::cleanup()
{
    closeAllPopups();
}

void FunctionFlowResultControllerTests::
screenshotRetryDialogSelectsTheMigratedCurrentModel()
{
    ScreenshotResultWindow window(
        QStringLiteral("test"),
        QImage(20, 10, QImage::Format_ARGB32),
        QVector<OcrTextBlock>(),
        QStringLiteral("recognized"),
        QStringLiteral("result"),
        QRect(),
        90,
        0
    );
    window.setAttribute(Qt::WA_DeleteOnClose, false);
    window.setModelOptions(
        QVector<QPair<QString, QString>>()
            << qMakePair(
                QStringLiteral("deepseek-v4-flash"),
                QStringLiteral("DeepSeek V4 Flash")
            )
            << qMakePair(
                QStringLiteral("openai:gpt-5.6-terra"),
                QStringLiteral("GPT-5.6 Terra")
            )
    );
    window.setCurrentModel(QStringLiteral("openai:gpt-5.4"));
    window.setActionCallbacks(
        std::function<void()>(),
        [](const QString &) {},
        std::function<void(const QString &)>(),
        std::function<void(const QString &)>(),
        std::function<void(const QString &)>()
    );

    QString selectedTitle;
    bool dialogFound = false;
    QTimer::singleShot(0, [&]() {
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            QInputDialog *dialog = qobject_cast<QInputDialog *>(widget);
            if (!dialog || !dialog->isVisible()) {
                continue;
            }
            dialogFound = true;
            QComboBox *models = dialog->findChild<QComboBox *>();
            if (models) {
                selectedTitle = models->currentText();
            }
            dialog->reject();
            break;
        }
    });

    window.chooseModelAndRetry();
    QVERIFY(dialogFound);
    QCOMPARE(selectedTitle, QStringLiteral("GPT-5.6 Terra"));
}

void FunctionFlowResultControllerTests::
screenshotRetryDialogUsesTheSelectedDuplicateTitleIndex()
{
    ScreenshotResultWindow window(
        QStringLiteral("test"),
        QImage(20, 10, QImage::Format_ARGB32),
        QVector<OcrTextBlock>(),
        QStringLiteral("recognized"),
        QStringLiteral("result"),
        QRect(),
        90,
        0
    );
    window.setAttribute(Qt::WA_DeleteOnClose, false);
    window.setModelOptions(
        QVector<QPair<QString, QString>>()
            << qMakePair(
                QStringLiteral("custom:first"),
                QStringLiteral("Same title")
            )
            << qMakePair(
                QStringLiteral("custom:second"),
                QStringLiteral("Same title")
            )
    );
    window.setCurrentModel(QStringLiteral("custom:first"));

    QStringList retriedModels;
    window.setActionCallbacks(
        std::function<void()>(),
        [&retriedModels](const QString &modelId) {
            retriedModels.append(modelId);
        },
        std::function<void(const QString &)>(),
        std::function<void(const QString &)>(),
        std::function<void(const QString &)>()
    );

    bool dialogFound = false;
    QTimer::singleShot(0, [&dialogFound]() {
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            QInputDialog *dialog = qobject_cast<QInputDialog *>(widget);
            if (!dialog || !dialog->isVisible()) {
                continue;
            }
            QComboBox *models = dialog->findChild<QComboBox *>();
            if (models) {
                QCOMPARE(models->count(), 2);
                QCOMPARE(models->itemText(0), models->itemText(1));
                models->setCurrentIndex(1);
                dialogFound = true;
            }
            dialog->accept();
            break;
        }
    });

    window.chooseModelAndRetry();
    QVERIFY(dialogFound);
    QCOMPARE(
        retriedModels,
        QStringList() << QStringLiteral("custom:second")
    );
}

void FunctionFlowResultControllerTests::
templatesUseCanonicalInputAndOutput()
{
    CancellationSource cancellation;
    const FunctionFlowRunContext run = runContext(cancellation);
    const FunctionFlowResultActionRequest request =
        actionRequest();

    QCOMPARE(
        buildFunctionFlowResultPopupText(
            run,
            popupNode(QStringLiteral("outputOnly")),
            request
        ),
        QString::fromUtf8("冻结的最终输出")
    );
    const QString compare =
        buildFunctionFlowResultPopupText(
            run,
            popupNode(QStringLiteral("compare")),
            request
        );
    QVERIFY(compare.contains(QString::fromUtf8("冻结的规范输入")));
    QVERIFY(compare.contains(QString::fromUtf8("冻结的最终输出")));
    const QString detail =
        buildFunctionFlowResultPopupText(
            run,
            popupNode(QStringLiteral("detail")),
            request
        );
    QVERIFY(detail.contains(QString::fromUtf8("流程功能")));
    QVERIFY(detail.contains(QString::fromUtf8("冻结的规范输入")));
    QVERIFY(detail.contains(QString::fromUtf8("冻结的最终输出")));
}

void FunctionFlowResultControllerTests::
streamingPreviewIsReusedAndOnlyFinalIsEditable()
{
    int opened = 0;
    int completed = 0;
    FunctionFlowResultControllerCallbacks callbacks;
    callbacks.editableSurfaceOpened =
        [&](const ExecutionId &) {
            ++opened;
        };
    FunctionFlowResultController controller(
        FunctionFlowResultControllerAccess(),
        callbacks
    );
    CancellationSource cancellation;
    FunctionFlowRunContext run = runContext(cancellation);
    QSharedPointer<FunctionFlowResolvedDependencies> deps(
        new FunctionFlowResolvedDependencies(*run.dependencies)
    );
    deps->nodeConfigs.insert(
        QStringLiteral("popup"),
        popupNode().config
    );
    run.dependencies = deps;

    controller.beginStreamingPreview(
        run,
        QStringLiteral("model"),
        QStringLiteral("popup")
    );
    ResultChoicePopup *preview = visiblePopup();
    QVERIFY(preview);
    QCOMPARE(opened, 0);
    controller.appendStreamingDelta(
        run.runId,
        QStringLiteral("model"),
        QStringLiteral("popup"),
        QString::fromUtf8("部分")
    );
    QCOMPARE(preview->resultText(), QString::fromUtf8("部分"));

    controller.runAction(
        run,
        popupNode(),
        actionRequest(),
        [&](const FunctionFlowNodeResult &result) {
            QCOMPARE(
                result.state,
                FunctionFlowNodeState::Succeeded
            );
            ++completed;
        }
    );
    QCOMPARE(visiblePopup(), preview);
    QCOMPARE(opened, 1);
    QCOMPARE(completed, 1);
    QCOMPARE(
        preview->resultText(),
        QString::fromUtf8("冻结的最终输出")
    );
}

void FunctionFlowResultControllerTests::
closingBusyPreviewRequestsCancellation()
{
    int cancelCount = 0;
    FunctionFlowResultControllerCallbacks callbacks;
    callbacks.requestCancel = [&](const ExecutionId &) {
        ++cancelCount;
    };
    FunctionFlowResultController controller(
        FunctionFlowResultControllerAccess(),
        callbacks
    );
    CancellationSource cancellation;
    FunctionFlowRunContext run = runContext(cancellation);
    controller.beginStreamingPreview(
        run,
        QStringLiteral("model"),
        QStringLiteral("popup")
    );
    QVERIFY(visiblePopup());
    visiblePopup()->close();
    QApplication::processEvents();
    QCOMPARE(cancelCount, 1);
}

void FunctionFlowResultControllerTests::
checkedWriteFailureKeepsPopupOpen()
{
    int writeCalls = 0;
    FunctionFlowResultControllerAccess access;
    access.isUsableExternalTargetWindow =
        [](FunctionFlowTargetWindowHandle) {
            return true;
        };
    access.hasCurrentSelection =
        [](FunctionFlowTargetWindowHandle) {
            return true;
        };
    access.writeText =
        [&](const QString &,
            FunctionFlowTargetWindowHandle,
            bool,
            bool) {
            ++writeCalls;
            ClipboardWriteResult result;
            result.errorCode =
                QStringLiteral("flow_input_injection_failed");
            return result;
        };
    FunctionFlowResultController controller(
        access,
        FunctionFlowResultControllerCallbacks()
    );
    CancellationSource cancellation;
    controller.runAction(
        runContext(cancellation),
        popupNode(),
        actionRequest(),
        [](const FunctionFlowNodeResult &) {}
    );
    ResultChoicePopup *popup = visiblePopup();
    QVERIFY(popup);
    QPushButton *write = popup->findChild<QPushButton *>(
        QStringLiteral("resultAction_write")
    );
    QVERIFY(write);
    QTest::mouseClick(write, Qt::LeftButton);
    QApplication::processEvents();
    QCOMPARE(writeCalls, 1);
    QVERIFY(popup->isVisible());
}

void FunctionFlowResultControllerTests::
editedSurfaceClosesExactlyOnce()
{
    int opened = 0;
    int closed = 0;
    QString committed;
    FunctionFlowResultControllerCallbacks callbacks;
    callbacks.editableSurfaceOpened =
        [&](const ExecutionId &) {
            ++opened;
        };
    callbacks.editedTextCommitted =
        [&](const ExecutionId &, const QString &text) {
            committed = text;
        };
    callbacks.editableSurfaceClosed =
        [&](const ExecutionId &) {
            ++closed;
        };
    FunctionFlowResultController controller(
        FunctionFlowResultControllerAccess(),
        callbacks
    );
    CancellationSource cancellation;
    controller.runAction(
        runContext(cancellation),
        popupNode(),
        actionRequest(),
        [](const FunctionFlowNodeResult &) {}
    );
    ResultChoicePopup *popup = visiblePopup();
    QVERIFY(popup);
    QTextEdit *editor = popup->findChild<QTextEdit *>();
    QVERIFY(editor);
    editor->setPlainText(QString::fromUtf8("用户编辑结果"));
    popup->close();
    QApplication::processEvents();
    QCOMPARE(opened, 1);
    QCOMPARE(closed, 1);
    QCOMPARE(committed, QString::fromUtf8("用户编辑结果"));
    QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCOMPARE(closed, 1);
}

void FunctionFlowResultControllerTests::
inheritedOpacityIsFrozen()
{
    FunctionFlowResultControllerAccess access;
    FunctionFlowResultControllerCallbacks callbacks;
    FunctionFlowResultController controller(access, callbacks);
    CancellationSource cancellation;
    controller.runAction(
        runContext(cancellation, 73),
        popupNode(),
        actionRequest(),
        [](const FunctionFlowNodeResult &) {}
    );
    QVERIFY(visiblePopup());
    QVERIFY(qAbs(visiblePopup()->windowOpacity() - 0.73) < 0.01);
}

void FunctionFlowResultControllerTests::
previewDoesNotAutoCloseBeforeFinalTakeover()
{
    FunctionFlowResultControllerAccess access;
    FunctionFlowResultControllerCallbacks callbacks;
    FunctionFlowResultController controller(access, callbacks);
    CancellationSource cancellation;
    FunctionFlowRunContext run = runContext(cancellation);
    FunctionFlowCompiledNode node = popupNode();
    node.config.popup.displaySeconds = 1;
    QSharedPointer<FunctionFlowResolvedDependencies> deps(
        new FunctionFlowResolvedDependencies(*run.dependencies)
    );
    deps->nodeConfigs.insert(
        QStringLiteral("popup"),
        node.config
    );
    run.dependencies = deps;
    controller.beginStreamingPreview(
        run,
        QStringLiteral("model"),
        QStringLiteral("popup")
    );
    QTest::qWait(1100);
    QVERIFY(visiblePopup());

    controller.runAction(
        run,
        node,
        actionRequest(),
        [](const FunctionFlowNodeResult &) {}
    );
    QTRY_VERIFY_WITH_TIMEOUT(!visiblePopup(), 1800);
}

void FunctionFlowResultControllerTests::
autoWriteUsesFrozenSelectionAndShowsOneFallback()
{
    int selectionChecks = 0;
    int writes = 0;
    FunctionFlowResultControllerAccess access;
    access.isUsableExternalTargetWindow =
        [](FunctionFlowTargetWindowHandle) {
            return true;
        };
    access.hasCurrentSelection =
        [&selectionChecks](FunctionFlowTargetWindowHandle) {
            ++selectionChecks;
            return false;
        };
    access.writeText =
        [&writes](
            const QString &,
            FunctionFlowTargetWindowHandle,
            bool,
            bool) {
            ++writes;
            ClipboardWriteResult result;
            result.ok = true;
            return result;
        };
    FunctionFlowResultController controller(
        access,
        FunctionFlowResultControllerCallbacks()
    );
    CancellationSource cancellation;
    FunctionFlowCompiledNode node;
    node.nodeId = QStringLiteral("auto");
    node.type = FunctionFlowNodeType::AutoWrite;
    node.config.autoWrite.writeMode =
        QStringLiteral("replace");
    node.config.autoWrite.fallbackToPopup = true;

    FunctionFlowNodeResult completion;
    controller.runAction(
        runContext(cancellation),
        node,
        actionRequest(),
        [&completion](const FunctionFlowNodeResult &result) {
            completion = result;
        }
    );
    QCOMPARE(
        completion.state,
        FunctionFlowNodeState::Failed
    );
    QCOMPARE(
        completion.error.code,
        QStringLiteral("flow_replace_selection_unavailable")
    );
    QCOMPARE(selectionChecks, 1);
    QCOMPARE(writes, 0);
    QVERIFY(visiblePopup());

    closeAllPopups();
    node.autoWriteFallbackCoveredByExplicitPopup = true;
    controller.runAction(
        runContext(cancellation),
        node,
        actionRequest(),
        [](const FunctionFlowNodeResult &) {}
    );
    QVERIFY(!visiblePopup());
}

void FunctionFlowResultControllerTests::
screenshotPanelUsesPayloadAndCheckedWrites()
{
    int opened = 0;
    int closed = 0;
    int writes = 0;
    QString committed;
    FunctionFlowResultControllerAccess access;
    access.isUsableExternalTargetWindow =
        [](FunctionFlowTargetWindowHandle) {
            return true;
        };
    access.hasCurrentSelection =
        [](FunctionFlowTargetWindowHandle) {
            return true;
        };
    access.writeText =
        [&writes](
            const QString &,
            FunctionFlowTargetWindowHandle,
            bool,
            bool) {
            ++writes;
            ClipboardWriteResult result;
            result.errorCode =
                QStringLiteral("flow_input_injection_failed");
            return result;
        };
    FunctionFlowResultControllerCallbacks callbacks;
    callbacks.editableSurfaceOpened =
        [&opened](const ExecutionId &) {
            ++opened;
        };
    callbacks.editedTextCommitted =
        [&committed](
            const ExecutionId &,
            const QString &text) {
            committed = text;
        };
    callbacks.editableSurfaceClosed =
        [&closed](const ExecutionId &) {
            ++closed;
        };
    FunctionFlowResultController controller(access, callbacks);

    CancellationSource cancellation;
    FunctionFlowRunContext run = runContext(cancellation);
    QSharedPointer<FunctionFlowResolvedDependencies> deps(
        new FunctionFlowResolvedDependencies(*run.dependencies)
    );
    deps->inheritedScreenshotPanelOpacity = 76;
    run.dependencies = deps;
    FunctionFlowCompiledNode node;
    node.nodeId = QStringLiteral("screenshot_panel");
    node.type = FunctionFlowNodeType::ScreenshotPanel;
    node.config.screenshotPanel.opacity = -1;
    FunctionFlowResultActionRequest request = actionRequest();
    FunctionFlowScreenshotPayload *payload =
        new FunctionFlowScreenshotPayload;
    payload->image = QImage(20, 10, QImage::Format_ARGB32);
    OcrTextBlock block;
    block.text = QStringLiteral("source");
    payload->blocks << block;
    payload->recognizedText = QStringLiteral("source");
    request.output.screenshot =
        QSharedPointer<const FunctionFlowScreenshotPayload>(
            payload
        );

    FunctionFlowNodeResult completion;
    controller.runAction(
        run,
        node,
        request,
        [&completion](const FunctionFlowNodeResult &result) {
            completion = result;
        }
    );
    QCOMPARE(
        completion.state,
        FunctionFlowNodeState::Succeeded
    );
    ScreenshotResultWindow *window =
        visibleScreenshotWindow();
    QVERIFY(window);
    QCOMPARE(opened, 1);
    QVERIFY(qAbs(window->windowOpacity() - 0.76) < 0.01);

    QPushButton *write = window->findChild<QPushButton *>(
        QStringLiteral("screenshotAction_write")
    );
    QVERIFY(write);
    QTest::mouseClick(write, Qt::LeftButton);
    QApplication::processEvents();
    QCOMPARE(writes, 1);
    QVERIFY(window->isVisible());

    QTextEdit *editor = window->findChild<QTextEdit *>();
    QVERIFY(editor);
    editor->setPlainText(QStringLiteral("edited screenshot"));
    window->close();
    QApplication::processEvents();
    QCOMPARE(committed, QStringLiteral("edited screenshot"));
    QCOMPARE(closed, 1);
    QApplication::sendPostedEvents(
        nullptr,
        QEvent::DeferredDelete
    );
    QCOMPARE(closed, 1);
}

QTEST_MAIN(FunctionFlowResultControllerTests)

#include "function_flow_result_controller_tests.moc"
