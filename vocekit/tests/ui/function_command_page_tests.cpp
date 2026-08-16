#include "../../src/ui/function_command_page.h"

#include "../../src/controllers/function_flow_editor_controller.h"
#include "../../src/ui/function_canvas_editor.h"
#include "../../src/ui/function_canvas_scene.h"
#include "../../src/ui/function_canvas_view.h"
#include "../../src/ui/hub_settings_state.h"
#include "../../src/ui/floating_bar_style_selector.h"
#include "../../src/ui/shortcut_display.h"
#include "../../src/config/app_settings_defaults.h"

#include <QComboBox>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QDir>
#include <QPointer>
#include <QPushButton>
#include <QFontMetrics>
#include <QRawFont>
#include <QLabel>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QWheelEvent>
#include <QtTest>

namespace {

FunctionSettings pageFunction(const QString &id, const QString &name)
{
    FunctionSettings function;
    function.id = id;
    function.name = name;
    function.shortcut = QStringLiteral("Ctrl+Alt+1");
    function.modelId = QStringLiteral("deepseek-v3");
    function.promptId = id;
    function.input.useSelection = true;
    function.flow.draft.graphHash =
        functionFlowGraphHash(function.flow.draft.graph);
    function.flow.published.graphHash =
        functionFlowGraphHash(function.flow.published.graph);
    return function;
}

struct FakePageFlows
{
    QHash<QString, FunctionFlowState> states;
    QHash<QString, FunctionExecutionMode> modes;
    QSet<QString> failedReads;
    AppSettingsData *settingsData = nullptr;
    int reads = 0;
    int draftSaves = 0;
    int editorSaves = 0;
    int modeChanges = 0;
    bool failDraftSave = false;
    bool failEditorSave = false;
    bool failModeChange = false;

    FunctionFlowSettingsAccess access()
    {
        FunctionFlowSettingsAccess result;
        result.readState = [this](
            const QString &id,
            FunctionFlowState *state,
            OperationError *error
        ) {
            ++reads;
            if (failedReads.contains(id)) {
                if (error) {
                    error->code = QStringLiteral("test_read_failed");
                    error->message = QStringLiteral("read failed");
                }
                return false;
            }
            if (state) {
                *state = states.value(id);
            }
            return true;
        };
        result.analyzeDraft = [](
            const QString &,
            const FunctionFlowGraph &graph
        ) {
            FunctionFlowDraftAnalysis analysis;
            analysis.graphHash = functionFlowGraphHash(graph);
            analysis.validation.ok = true;
            return analysis;
        };
        result.updateDraft = [this](
            const QString &id,
            int expectedRevision,
            const FunctionFlowGraph &graph,
            int *savedRevision,
            OperationError *error
        ) {
            ++draftSaves;
            if (failDraftSave) {
                if (error) {
                    error->code = QStringLiteral("test_draft_save_failed");
                    error->message = QStringLiteral("draft save failed");
                }
                return false;
            }
            FunctionFlowState &state = states[id];
            state.draft.graph = graph;
            state.draft.revision = expectedRevision + 1;
            state.draft.graphHash = functionFlowGraphHash(graph);
            if (savedRevision) {
                *savedRevision = state.draft.revision;
            }
            return true;
        };
        result.updateEditorState = [this](
            const QString &id,
            const FunctionFlowEditorState &editor,
            OperationError *error
        ) {
            ++editorSaves;
            if (failEditorSave) {
                if (error) {
                    error->code = QStringLiteral("test_editor_save_failed");
                    error->message = QStringLiteral("editor save failed");
                }
                return false;
            }
            states[id].editor = editor;
            return true;
        };
        result.publish = [](const QString &, int, bool) {
            FunctionFlowPublishResult publish;
            publish.ok = true;
            return publish;
        };
        result.setExecutionMode = [this](
            const QString &id,
            FunctionExecutionMode mode,
            OperationError *error
        ) {
            ++modeChanges;
            if (failModeChange) {
                if (error) {
                    error->code =
                        QStringLiteral("test_mode_change_failed");
                    error->message =
                        QStringLiteral("mode change failed");
                }
                return false;
            }
            modes[id] = mode;
            if (settingsData) {
                const int index = settingsData->functionIndex(id);
                if (index >= 0) {
                    FunctionSettings updated =
                        settingsData->functions.at(index);
                    updated.executionMode = mode;
                    updated = normalizeFunctionSettings(updated);
                    settingsData->functions[index] = updated;
                    states[id] = updated.flow;
                }
            }
            return true;
        };
        return result;
    }
};

struct PageEnvironment
{
    AppSettingsData data;
    FakePageFlows flows;
    HubWindowAccess hubAccess;
    QScopedPointer<HubSettingsState> settings;
    FunctionCommandPageAccess pageAccess;
    int reportedErrors = 0;
    int saves = 0;

    PageEnvironment()
    {
        const FunctionSettings first = pageFunction(
            QStringLiteral("custom_1"),
            QString::fromUtf8("自定义功能 1")
        );
        const FunctionSettings second = pageFunction(
            QStringLiteral("custom_2"),
            QString::fromUtf8("自定义功能 2")
        );
        data.functions << first << second;
        data.functionOrder << first.id << second.id;
        flows.settingsData = &data;
        flows.states.insert(first.id, first.flow);
        flows.states.insert(second.id, second.flow);
        flows.modes.insert(first.id, first.executionMode);
        flows.modes.insert(second.id, second.executionMode);
        hubAccess.settingsSnapshotProvider = [this]() {
            return data;
        };
        settings.reset(new HubSettingsState(hubAccess));
        pageAccess.settings = settings.data();
        pageAccess.flows = flows.access();
        pageAccess.operationFailed = [this](const OperationError &) {
            ++reportedErrors;
        };
        pageAccess.saveSettings = [this]() { ++saves; };
    }
};

QPushButton *canvasToggle(FunctionCommandPage *page)
{
    const QList<QPushButton *> buttons =
        page->findChildren<QPushButton *>(
            QStringLiteral("functionCanvasButton")
        );
    for (QPushButton *button : buttons) {
        if (button->isVisibleTo(page)) {
            return button;
        }
    }
    return buttons.isEmpty() ? nullptr : buttons.last();
}

QPushButton *classicModeButton(FunctionCommandPage *page)
{
    const QList<QPushButton *> buttons =
        page->findChildren<QPushButton *>(
            QStringLiteral("functionClassicModeButton")
        );
    for (QPushButton *button : buttons) {
        if (button->isVisibleTo(page)) {
            return button;
        }
    }
    return buttons.isEmpty() ? nullptr : buttons.last();
}

QPushButton *canvasModeButton(FunctionCommandPage *page)
{
    const QList<QPushButton *> buttons =
        page->findChildren<QPushButton *>(
            QStringLiteral("functionCanvasModeButton")
        );
    for (QPushButton *button : buttons) {
        if (button->isVisibleTo(page)) {
            return button;
        }
    }
    return buttons.isEmpty() ? nullptr : buttons.last();
}

QWidget *executionModeSelector(FunctionCommandPage *page)
{
    const QList<QWidget *> selectors =
        page->findChildren<QWidget *>(
            QStringLiteral("functionExecutionModeSelector")
        );
    for (QWidget *selector : selectors) {
        if (selector->isVisibleTo(page)) {
            return selector;
        }
    }
    return selectors.isEmpty() ? nullptr : selectors.last();
}

QLabel *visibleLabelWithText(
    FunctionCommandPage *page,
    const QString &text)
{
    const QList<QLabel *> labels = page->findChildren<QLabel *>();
    for (QLabel *label : labels) {
        if (label->text() == text && label->isVisibleTo(page)) {
            return label;
        }
    }
    return nullptr;
}

void showPage(FunctionCommandPage *page)
{
    page->resize(980, 640);
    page->show();
    QCoreApplication::processEvents();
}

int countData(const QComboBox *box, const QString &value)
{
    int count = 0;
    for (int index = 0; index < box->count(); ++index) {
        if (box->itemData(index).toString() == value) {
            ++count;
        }
    }
    return count;
}

QComboBox *speechProviderCombo(QWidget *root)
{
    const QList<QComboBox *> boxes = root->findChildren<QComboBox *>();
    for (QComboBox *box : boxes) {
        if (box->findData(speechProviderBaidu()) >= 0
            && box->findData(speechProviderXfyun()) >= 0) {
            return box;
        }
    }
    return nullptr;
}

} // namespace

class FunctionCommandPageTests : public QObject
{
    Q_OBJECT

private slots:
    void executionModeSelectorStaysVisibleAcrossPureNavigation();
    void longHeaderTextWrapsWithoutPushingControlsOffscreen();
    void clickingTheSelectedModeIsAnIdempotentUiNoOp();
    void successfulModeChangeReloadsThePersistedSelection();
    void failedModeChangeRestoresThePersistedSelection();
    void canvasWorkspaceStaysOutsideSettingsScrollAcrossRefreshes();
    void returningToSettingsFlushesPendingCanvasSaves();
    void failedSaveKeepsTheUserOnTheCanvas();
    void failedFunctionSwitchKeepsPageAndEditorIdsAligned();
    void canvasOnlyRefreshDoesNotRebuildTheSettingsForm();
    void customFunctionPersistsFloatingBarStyleOverride();
    void builtInFunctionHasNoFloatingBarStyleOverride();
    void speechSelectorsContainCatalogProvidersExactlyOnce();
    void classicAiSectionEditsFunctionSamplingOverrides();
    void classicAiSamplingControlsDoNotClipAt100_125_150Percent();
};

void FunctionCommandPageTests::
classicAiSectionEditsFunctionSamplingOverrides()
{
    PageEnvironment environment;
    FunctionCommandPage page(environment.pageAccess);
    QVERIFY(page.setFunctionId(QStringLiteral("custom_1")));
    showPage(&page);

    QCheckBox *temperatureEnabled = page.findChild<QCheckBox *>(
        QStringLiteral("functionTemperatureEnabled")
    );
    QDoubleSpinBox *temperature = page.findChild<QDoubleSpinBox *>(
        QStringLiteral("functionTemperatureSpin")
    );
    QCheckBox *topPEnabled = page.findChild<QCheckBox *>(
        QStringLiteral("functionTopPEnabled")
    );
    QDoubleSpinBox *topP = page.findChild<QDoubleSpinBox *>(
        QStringLiteral("functionTopPSpin")
    );
    QVERIFY(temperatureEnabled);
    QVERIFY(temperature);
    QVERIFY(topPEnabled);
    QVERIFY(topP);
    QVERIFY(!temperatureEnabled->isChecked());
    QVERIFY(!temperature->isEnabled());
    QVERIFY(!topPEnabled->isChecked());
    QVERIFY(!topP->isEnabled());

    temperatureEnabled->setChecked(true);
    temperature->setValue(1.1);
    topPEnabled->setChecked(true);
    topP->setValue(0.65);

    const ModelSamplingSettings sampling =
        environment.settings->modelSamplingFor(
            QStringLiteral("custom_1")
        );
    QVERIFY(sampling.temperatureEnabled);
    QCOMPARE(sampling.temperature, 1.1);
    QVERIFY(sampling.topPEnabled);
    QCOMPARE(sampling.topP, 0.65);
    QVERIFY(environment.saves >= 4);
}

void FunctionCommandPageTests::
classicAiSamplingControlsDoNotClipAt100_125_150Percent()
{
    const QFont originalFont = QApplication::font();
    const QVector<int> scales = QVector<int>() << 100 << 125 << 150;
    const QString visualOutputDir = QString::fromLocal8Bit(
        qgetenv("VOCEKIT_VISUAL_OUTPUT_DIR")
    ).trimmed();

    for (int scale : scales) {
        QFont font(QStringLiteral("Microsoft YaHei UI"));
        font.setPixelSize(qMax(12, (14 * scale) / 100));
        QApplication::setFont(font);

        PageEnvironment environment;
        FunctionCommandPage page(environment.pageAccess);
        QVERIFY(page.setFunctionId(QStringLiteral("custom_1")));
        page.resize(1100, 860);
        page.show();
        QCoreApplication::processEvents();

        QCheckBox *temperatureEnabled = page.findChild<QCheckBox *>(
            QStringLiteral("functionTemperatureEnabled")
        );
        QDoubleSpinBox *temperature = page.findChild<QDoubleSpinBox *>(
            QStringLiteral("functionTemperatureSpin")
        );
        QCheckBox *topPEnabled = page.findChild<QCheckBox *>(
            QStringLiteral("functionTopPEnabled")
        );
        QDoubleSpinBox *topP = page.findChild<QDoubleSpinBox *>(
            QStringLiteral("functionTopPSpin")
        );
        QLabel *hint = page.findChild<QLabel *>(
            QStringLiteral("functionSamplingHint")
        );
        QVERIFY(temperatureEnabled);
        QVERIFY(temperature);
        QVERIFY(topPEnabled);
        QVERIFY(topP);
        QVERIFY(hint);

        QWidget *body = temperatureEnabled->parentWidget();
        while (body && body != &page
               && body->objectName() != QStringLiteral("commandAccordionBody")) {
            body = body->parentWidget();
        }
        QVERIFY(body);
        QCOMPARE(body->objectName(), QStringLiteral("commandAccordionBody"));
        QWidget *card = body->parentWidget();
        QVERIFY(card);
        QWidget *header = card->findChild<QWidget *>(
            QStringLiteral("commandMethodHeader"),
            Qt::FindDirectChildrenOnly
        );
        QVERIFY(header);
        QTest::mouseClick(header, Qt::LeftButton);
        QCoreApplication::processEvents();
        QVERIFY(body->isVisibleTo(&page));

        QScrollArea *scroll = page.findChild<QScrollArea *>();
        QVERIFY(scroll);
        scroll->ensureWidgetVisible(card, 0, 24);
        QCoreApplication::processEvents();

        if (QGuiApplication::platformName() == QStringLiteral("windows")) {
            const QRawFont rawFont = QRawFont::fromFont(page.font());
            QVERIFY2(rawFont.isValid(),
                     "Windows native visual gate requires a valid raw font");
            const QString requiredCjk = QString::fromUtf8("温度自定义默认关闭");
            for (const QChar character : requiredCjk) {
                QVERIFY2(rawFont.supportsCharacter(character),
                         qPrintable(QStringLiteral("missing CJK glyph U+%1")
                             .arg(character.unicode(), 4, 16, QLatin1Char('0'))));
            }
        }

        const QList<QCheckBox *> switches =
            QList<QCheckBox *>() << temperatureEnabled << topPEnabled;
        for (QCheckBox *control : switches) {
            QVERIFY(control->isVisibleTo(&page));
            QVERIFY2(control->height() >= control->sizeHint().height(),
                     qPrintable(QStringLiteral("scale=%1 checkbox height=%2 hint=%3")
                         .arg(scale).arg(control->height())
                         .arg(control->sizeHint().height())));
            QVERIFY(control->width() >= control->sizeHint().width());
        }
        const QList<QDoubleSpinBox *> spins =
            QList<QDoubleSpinBox *>() << temperature << topP;
        for (QDoubleSpinBox *control : spins) {
            QVERIFY(control->isVisibleTo(&page));
            QVERIFY2(control->height() >= control->sizeHint().height(),
                     qPrintable(QStringLiteral("scale=%1 spin height=%2 hint=%3")
                         .arg(scale).arg(control->height())
                         .arg(control->sizeHint().height())));
            QVERIFY(control->width() >= control->sizeHint().width());
        }
        QVERIFY(hint->isVisibleTo(&page));
        QVERIFY2(hint->height() >= hint->sizeHint().height(),
                 qPrintable(QStringLiteral("scale=%1 hint height=%2 sizeHint=%3 body=%4/%5 card=%6/%7")
                     .arg(scale).arg(hint->height())
                     .arg(hint->sizeHint().height())
                     .arg(body->height()).arg(body->sizeHint().height())
                     .arg(card->height()).arg(card->sizeHint().height())));

        if (!visualOutputDir.isEmpty()) {
            QVERIFY(QDir().mkpath(visualOutputDir));
            const QString imagePath = QDir(visualOutputDir).filePath(
                QStringLiteral("function-ai-sampling-%1.png").arg(scale)
            );
            QVERIFY(card->grab().save(imagePath));
        }
    }
    QApplication::setFont(originalFont);
}

void FunctionCommandPageTests::
speechSelectorsContainCatalogProvidersExactlyOnce()
{
    PageEnvironment environment;
    FunctionCommandPage page(environment.pageAccess);
    QVERIFY(page.setFunctionId(QStringLiteral("custom_1")));
    showPage(&page);

    QComboBox *globalSpeech = speechProviderCombo(&page);
    QVERIFY(globalSpeech);
    for (const QString &providerId : supportedSpeechProviderIds()) {
        QCOMPARE(countData(globalSpeech, providerId), 1);
    }

    QPushButton *toggle = canvasToggle(&page);
    QVERIFY(toggle);
    toggle->click();
    QCoreApplication::processEvents();
    FunctionCanvasEditor *editor = page.canvasEditor();
    QVERIFY(editor);
    const QString voiceId = editor->controller()->placeNode(
        FunctionFlowNodeType::VoiceSource,
        QPointF(120.0, 120.0)
    );
    QVERIFY(!voiceId.isEmpty());
    editor->inspector()->setGraphAndSelection(
        editor->controller()->graph(),
        voiceId
    );
    QComboBox *inspectorSpeech = editor->inspector()->findChild<QComboBox *>(
        QStringLiteral("flowVoiceProvider")
    );
    QVERIFY(inspectorSpeech);
    for (const QString &providerId : supportedSpeechProviderIds()) {
        QCOMPARE(countData(inspectorSpeech, providerId), 1);
    }
}

void FunctionCommandPageTests::
customFunctionPersistsFloatingBarStyleOverride()
{
    PageEnvironment environment;
    FunctionCommandPage page(environment.pageAccess);
    QVERIFY(page.setFunctionId(QStringLiteral("custom_1")));
    showPage(&page);
    auto *selector = page.findChild<FloatingBarStyleSelector *>(
        QStringLiteral("functionFloatingBarStyleSelector")
    );
    QVERIFY(selector);
    QCOMPARE(selector->currentStyle(), QStringLiteral("inherit"));
    QAbstractButton *card = selector->findChild<QAbstractButton *>(
        QStringLiteral("floatingBarStyleCard_liveTranscriptCard")
    );
    QVERIFY(card);
    card->click();
    QCOMPARE(
        environment.settings->floatingBarStyleOverrideFor(
            QStringLiteral("custom_1")
        ),
        QStringLiteral("liveTranscriptCard")
    );
    QCOMPARE(environment.saves, 1);

    page.refresh();
    QCoreApplication::processEvents();
    selector = page.findChild<FloatingBarStyleSelector *>(
        QStringLiteral("functionFloatingBarStyleSelector")
    );
    QVERIFY(selector);
    QCOMPARE(selector->currentStyle(),
             QStringLiteral("liveTranscriptCard"));
    QCOMPARE(environment.saves, 1);
}

void FunctionCommandPageTests::
builtInFunctionHasNoFloatingBarStyleOverride()
{
    PageEnvironment environment;
    FunctionSettings builtIn = pageFunction(
        QStringLiteral("dictate"),
        QString::fromUtf8("听写")
    );
    builtIn.builtIn = true;
    environment.data.functions.append(builtIn);
    environment.settings->load();
    FunctionCommandPage page(environment.pageAccess);
    QVERIFY(page.setFunctionId(QStringLiteral("dictate")));
    showPage(&page);
    QVERIFY(!page.findChild<FloatingBarStyleSelector *>(
        QStringLiteral("functionFloatingBarStyleSelector")
    ));
}

void FunctionCommandPageTests::
longHeaderTextWrapsWithoutPushingControlsOffscreen()
{
    PageEnvironment environment;
    const QString functionId = QStringLiteral("custom_1");
    const QString longTitle = QString::fromUtf8(
        "这是一个用于验证高分屏和窄窗口布局的超长中文自定义功能名称"
    );
    const QString longShortcut =
        QStringLiteral("Ctrl+Alt+Shift+F12");
    const int index = environment.data.functionIndex(functionId);
    QVERIFY(index >= 0);
    environment.data.functions[index].name = longTitle;
    environment.data.functions[index].shortcut = longShortcut;
    environment.settings->load();

    FunctionCommandPage page(environment.pageAccess);
    QVERIFY(page.setFunctionId(functionId));
    page.resize(760, 640);
    page.show();
    QCoreApplication::processEvents();
    QCOMPARE(page.width(), 760);
    QCOMPARE(page.height(), 640);

    QWidget *selector = executionModeSelector(&page);
    QPushButton *classic = classicModeButton(&page);
    QPushButton *canvasMode = canvasModeButton(&page);
    QPushButton *navigation = canvasToggle(&page);
    QLabel *title = visibleLabelWithText(&page, longTitle);
    QLabel *shortcut = visibleLabelWithText(
        &page,
        displayShortcut(longShortcut)
    );
    QVERIFY(selector);
    QVERIFY(classic);
    QVERIFY(canvasMode);
    QVERIFY(navigation);
    QVERIFY(title);
    QVERIFY(shortcut);
    QCOMPARE(
        title->objectName(),
        QStringLiteral("functionCommandTitleLabel")
    );
    QCOMPARE(
        shortcut->objectName(),
        QStringLiteral("functionCommandShortcutLabel")
    );

    const QList<QWidget *> controls =
        QList<QWidget *>()
        << selector << classic << canvasMode << navigation;
    for (QWidget *control : controls) {
        const QRect bounds(
            control->mapTo(&page, QPoint()),
            control->size()
        );
        QVERIFY2(
            page.rect().contains(bounds),
            qPrintable(QStringLiteral(
                "%1 at %2,%3 %4x%5 outside %6x%7"
            )
                .arg(control->objectName())
                .arg(bounds.x())
                .arg(bounds.y())
                .arg(bounds.width())
                .arg(bounds.height())
                .arg(page.width())
                .arg(page.height()))
        );
    }

    const QList<QLabel *> textLabels =
        QList<QLabel *>() << title << shortcut;
    for (QLabel *label : textLabels) {
        const QRect bounds(
            label->mapTo(&page, QPoint()),
            label->size()
        );
        QVERIFY(page.rect().contains(bounds));
        QVERIFY(label->wordWrap());
        const int requiredHeight =
            label->fontMetrics().boundingRect(
                QRect(0, 0, label->width(), 10000),
                Qt::TextWordWrap,
                label->text()
            ).height();
        QVERIFY2(
            label->height() >= requiredHeight,
            qPrintable(QStringLiteral("%1 needs height %2, has %3")
                .arg(label->objectName())
                .arg(requiredHeight)
                .arg(label->height()))
        );
    }
    QCOMPARE(title->text(), longTitle);
    QCOMPARE(shortcut->text(), displayShortcut(longShortcut));
    QVERIFY(title->font().pointSize() >= 22);
    QVERIFY(shortcut->font().pointSize() >= 10);
}

void FunctionCommandPageTests::
clickingTheSelectedModeIsAnIdempotentUiNoOp()
{
    PageEnvironment environment;
    FunctionCommandPage page(environment.pageAccess);
    QVERIFY(page.setFunctionId(QStringLiteral("custom_1")));
    showPage(&page);
    QStackedWidget *stack = page.findChild<QStackedWidget *>();
    QWidget *settingsPage = stack->currentWidget();
    QPushButton *classic = classicModeButton(&page);
    QVERIFY(classic);
    QVERIFY(classic->isChecked());

    classic->click();
    QCoreApplication::processEvents();

    QCOMPARE(environment.flows.modeChanges, 0);
    QCOMPARE(environment.reportedErrors, 0);
    QCOMPARE(stack->currentWidget(), settingsPage);
    QCOMPARE(classicModeButton(&page), classic);
    QVERIFY(classic->isChecked());
}

void FunctionCommandPageTests::
executionModeSelectorStaysVisibleAcrossPureNavigation()
{
    PageEnvironment environment;
    FunctionCommandPage page(environment.pageAccess);
    QVERIFY(page.setFunctionId(QStringLiteral("custom_1")));
    showPage(&page);

    QWidget *selector = executionModeSelector(&page);
    QPushButton *classic = classicModeButton(&page);
    QPushButton *canvasMode = canvasModeButton(&page);
    QPushButton *navigation = canvasToggle(&page);
    QLabel *modeLabel = visibleLabelWithText(
        &page,
        QString::fromUtf8("当前执行")
    );
    QVERIFY(selector);
    QVERIFY(classic);
    QVERIFY(canvasMode);
    QVERIFY(navigation);
    QVERIFY(modeLabel);
    QVERIFY(classic->isCheckable());
    QVERIFY(canvasMode->isCheckable());
    QVERIFY(classic->isChecked());
    QVERIFY(!canvasMode->isChecked());
    QCOMPARE(classic->text(), QString::fromUtf8("普通模式"));
    QCOMPARE(canvasMode->text(), QString::fromUtf8("画布模式"));
    QCOMPARE(navigation->text(), QString::fromUtf8("编辑画布"));
    page.resize(760, 640);
    QCoreApplication::processEvents();
    QVERIFY(selector->isVisibleTo(&page));
    QVERIFY(modeLabel->isVisibleTo(&page));
    QVERIFY(classic->isVisibleTo(&page));
    QVERIFY(canvasMode->isVisibleTo(&page));
    QVERIFY(navigation->isVisibleTo(&page));
    const QList<QPushButton *> textButtons =
        QList<QPushButton *>() << classic << canvasMode << navigation;
    for (QPushButton *button : textButtons) {
        QVERIFY2(
            button->fontMetrics().width(button->text()) + 20
                <= button->width(),
            qPrintable(QStringLiteral("%1 needs %2, has %3")
                .arg(button->objectName())
                .arg(button->fontMetrics().width(button->text()) + 20)
                .arg(button->width()))
        );
        const QRect bounds(
            button->mapTo(&page, QPoint()),
            button->size()
        );
        QVERIFY2(
            page.rect().contains(bounds),
            qPrintable(QStringLiteral("%1 at %2,%3 %4x%5 outside %6x%7")
                .arg(button->objectName())
                .arg(bounds.x())
                .arg(bounds.y())
                .arg(bounds.width())
                .arg(bounds.height())
                .arg(page.width())
                .arg(page.height()))
        );
    }
    QVERIFY2(
        modeLabel->fontMetrics().width(modeLabel->text())
            <= modeLabel->width(),
        qPrintable(QStringLiteral("mode label needs %1, has %2")
            .arg(modeLabel->fontMetrics().width(modeLabel->text()))
            .arg(modeLabel->width()))
    );
    const QRect compactModeLabelBounds(
        modeLabel->mapTo(&page, QPoint()),
        modeLabel->size()
    );
    QVERIFY(page.rect().contains(compactModeLabelBounds));

    page.resize(1320, 760);
    QCoreApplication::processEvents();
    QVERIFY(selector->isVisibleTo(&page));
    QVERIFY(modeLabel->isVisibleTo(&page));
    QVERIFY(classic->isVisibleTo(&page));
    QVERIFY(canvasMode->isVisibleTo(&page));
    QVERIFY(navigation->isVisibleTo(&page));
    const QList<QWidget *> wideControls =
        QList<QWidget *>()
        << modeLabel << classic << canvasMode << navigation;
    for (QWidget *control : wideControls) {
        const QRect bounds(
            control->mapTo(&page, QPoint()),
            control->size()
        );
        QVERIFY2(
            page.rect().contains(bounds),
            qPrintable(QStringLiteral(
                "%1 outside wide page at %2,%3 %4x%5"
            )
                .arg(control->objectName())
                .arg(bounds.x())
                .arg(bounds.y())
                .arg(bounds.width())
                .arg(bounds.height()))
        );
    }

    navigation->click();
    QCoreApplication::processEvents();

    QCOMPARE(environment.flows.modeChanges, 0);
    selector = executionModeSelector(&page);
    classic = classicModeButton(&page);
    canvasMode = canvasModeButton(&page);
    QPushButton *returnButton = canvasToggle(&page);
    QVERIFY(selector);
    QVERIFY(classic);
    QVERIFY(canvasMode);
    QVERIFY(returnButton);
    QVERIFY(classic->isChecked());
    QCOMPARE(
        returnButton->text(),
        QString::fromUtf8("返回设置")
    );
    QVERIFY(selector->isVisibleTo(&page));
    QVERIFY(classic->isVisibleTo(&page));
    QVERIFY(canvasMode->isVisibleTo(&page));
    QVERIFY(returnButton->isVisibleTo(&page));
    QVERIFY2(
        returnButton->fontMetrics().width(returnButton->text()) + 20
            <= returnButton->width(),
        qPrintable(QStringLiteral("return button needs %1, has %2")
            .arg(returnButton->fontMetrics().width(
                returnButton->text()
            ) + 20)
            .arg(returnButton->width()))
    );
    const QRect returnBounds(
        returnButton->mapTo(&page, QPoint()),
        returnButton->size()
    );
    QVERIFY(page.rect().contains(returnBounds));
}

void FunctionCommandPageTests::
successfulModeChangeReloadsThePersistedSelection()
{
    PageEnvironment environment;
    FunctionCommandPage page(environment.pageAccess);
    QVERIFY(page.setFunctionId(QStringLiteral("custom_1")));
    showPage(&page);
    QStackedWidget *stack = page.findChild<QStackedWidget *>();
    QWidget *settingsPage = stack->currentWidget();

    QPushButton *canvasMode = canvasModeButton(&page);
    QVERIFY(canvasMode);
    canvasMode->click();
    QCoreApplication::processEvents();

    QCOMPARE(environment.flows.modeChanges, 1);
    QCOMPARE(
        environment.flows.modes.value(QStringLiteral("custom_1")),
        FunctionExecutionMode::Canvas
    );
    QVERIFY(canvasModeButton(&page)->isChecked());
    QVERIFY(!classicModeButton(&page)->isChecked());
    QCOMPARE(stack->currentWidget(), settingsPage);
    QCOMPARE(canvasToggle(&page)->text(), QString::fromUtf8("编辑画布"));

    QPushButton *classicMode = classicModeButton(&page);
    QVERIFY(classicMode);
    classicMode->click();
    QCoreApplication::processEvents();

    QCOMPARE(environment.flows.modeChanges, 2);
    QCOMPARE(
        environment.flows.modes.value(QStringLiteral("custom_1")),
        FunctionExecutionMode::Classic
    );
    QVERIFY(classicModeButton(&page)->isChecked());
    QVERIFY(!canvasModeButton(&page)->isChecked());
    QCOMPARE(stack->currentWidget(), settingsPage);
    QCOMPARE(canvasToggle(&page)->text(), QString::fromUtf8("编辑画布"));
}

void FunctionCommandPageTests::
failedModeChangeRestoresThePersistedSelection()
{
    PageEnvironment environment;
    environment.flows.failModeChange = true;
    FunctionCommandPage page(environment.pageAccess);
    QVERIFY(page.setFunctionId(QStringLiteral("custom_1")));
    showPage(&page);
    QStackedWidget *stack = page.findChild<QStackedWidget *>();
    QWidget *settingsPage = stack->currentWidget();

    QPushButton *canvasMode = canvasModeButton(&page);
    QVERIFY(canvasMode);
    canvasMode->click();
    QCoreApplication::processEvents();

    QCOMPARE(environment.flows.modeChanges, 1);
    QCOMPARE(environment.reportedErrors, 1);
    QVERIFY(classicModeButton(&page)->isChecked());
    QVERIFY(!canvasModeButton(&page)->isChecked());
    QCOMPARE(stack->currentWidget(), settingsPage);
}

void FunctionCommandPageTests::
canvasWorkspaceStaysOutsideSettingsScrollAcrossRefreshes()
{
    PageEnvironment environment;
    FunctionCommandPage page(environment.pageAccess);
    QVERIFY(page.setFunctionId(QStringLiteral("custom_1")));
    showPage(&page);

    QScrollArea *settingsScroll = page.findChild<QScrollArea *>();
    QStackedWidget *stack = page.findChild<QStackedWidget *>();
    QWidget *canvasHost = page.findChild<QWidget *>(
        QStringLiteral("functionCanvasPageHost")
    );
    QVERIFY(settingsScroll);
    QVERIFY(stack);
    QVERIFY(canvasHost);
    QPushButton *toggle = canvasToggle(&page);
    QVERIFY(toggle);
    toggle->click();
    QCoreApplication::processEvents();

    FunctionCanvasEditor *editor = page.canvasEditor();
    QPointer<FunctionCanvasEditor> editorGuard(editor);
    QVERIFY(editor);
    QCOMPARE(stack->currentWidget(), canvasHost);
    QVERIFY(!settingsScroll->isAncestorOf(editor));
    QCOMPARE(
        editor->canvasView()->horizontalScrollBarPolicy(),
        Qt::ScrollBarAlwaysOff
    );
    QCOMPARE(
        editor->canvasView()->verticalScrollBarPolicy(),
        Qt::ScrollBarAlwaysOff
    );

    for (int refresh = 0; refresh < 4; ++refresh) {
        page.refresh();
        QCoreApplication::processEvents();
        QCOMPARE(page.canvasEditor(), editorGuard.data());
        QCOMPARE(
            page.findChildren<FunctionCanvasEditor *>().size(),
            1
        );
        QCOMPARE(stack->currentWidget(), canvasHost);
        QVERIFY(!settingsScroll->isAncestorOf(editorGuard.data()));
        QVERIFY(editorGuard->isVisible());
    }

    const int settingsScrollBefore =
        settingsScroll->verticalScrollBar()->value();
    QWheelEvent wheel(
        QPointF(120.0, 120.0),
        120,
        Qt::NoButton,
        Qt::NoModifier,
        Qt::Vertical
    );
    QApplication::sendEvent(editor->canvasView()->viewport(), &wheel);
    QCOMPARE(
        settingsScroll->verticalScrollBar()->value(),
        settingsScrollBefore
    );
}

void FunctionCommandPageTests::
returningToSettingsFlushesPendingCanvasSaves()
{
    PageEnvironment environment;
    FunctionCommandPage page(environment.pageAccess);
    QVERIFY(page.setFunctionId(QStringLiteral("custom_1")));
    showPage(&page);
    canvasToggle(&page)->click();
    QCoreApplication::processEvents();

    FunctionCanvasEditor *editor = page.canvasEditor();
    QVERIFY(editor);
    editor->canvasScene()->requestNodePlacement(
        FunctionFlowNodeType::Input,
        QPointF(100.0, 120.0)
    );
    FunctionFlowEditorState viewport;
    viewport.viewportCenter = QPointF(700.0, 500.0);
    viewport.zoom = 1.4;
    editor->controller()->updateEditorState(viewport);
    QCOMPARE(environment.flows.draftSaves, 0);
    QCOMPARE(environment.flows.editorSaves, 0);

    canvasToggle(&page)->click();
    QCoreApplication::processEvents();

    QCOMPARE(environment.flows.draftSaves, 1);
    QCOMPARE(environment.flows.editorSaves, 1);
    QCOMPARE(
        page.findChild<QStackedWidget *>()->currentWidget(),
        static_cast<QWidget *>(page.findChild<QScrollArea *>())
    );
    QTest::qWait(550);
    QCOMPARE(environment.flows.draftSaves, 1);
    QCOMPARE(environment.flows.editorSaves, 1);
}

void FunctionCommandPageTests::failedSaveKeepsTheUserOnTheCanvas()
{
    PageEnvironment environment;
    FunctionCommandPage page(environment.pageAccess);
    QVERIFY(page.setFunctionId(QStringLiteral("custom_1")));
    showPage(&page);
    canvasToggle(&page)->click();
    QCoreApplication::processEvents();

    page.canvasEditor()->canvasScene()->requestNodePlacement(
        FunctionFlowNodeType::Input,
        QPointF()
    );
    environment.flows.failDraftSave = true;
    QPushButton *toggle = canvasToggle(&page);
    QVERIFY(toggle);
    toggle->click();
    QCoreApplication::processEvents();

    QVERIFY(canvasToggle(&page)->isChecked());
    QCOMPARE(
        page.findChild<QStackedWidget *>()->currentWidget(),
        page.findChild<QWidget *>(
            QStringLiteral("functionCanvasPageHost")
        )
    );
    QCOMPARE(environment.reportedErrors, 1);
}

void FunctionCommandPageTests::
failedFunctionSwitchKeepsPageAndEditorIdsAligned()
{
    PageEnvironment environment;
    FunctionCommandPage page(environment.pageAccess);
    QVERIFY(page.setFunctionId(QStringLiteral("custom_1")));
    showPage(&page);
    canvasToggle(&page)->click();
    QCoreApplication::processEvents();
    QVERIFY(page.canvasEditor());
    QCOMPARE(
        page.canvasEditor()->functionId(),
        QStringLiteral("custom_1")
    );

    environment.flows.failedReads.insert(
        QStringLiteral("custom_2")
    );
    QVERIFY(!page.setFunctionId(QStringLiteral("custom_2")));

    QCOMPARE(page.functionId(), QStringLiteral("custom_1"));
    QCOMPARE(
        page.canvasEditor()->functionId(),
        QStringLiteral("custom_1")
    );
    QCOMPARE(environment.reportedErrors, 1);
}

void FunctionCommandPageTests::
canvasOnlyRefreshDoesNotRebuildTheSettingsForm()
{
    PageEnvironment environment;
    FunctionCommandPage page(environment.pageAccess);
    QVERIFY(page.setFunctionId(QStringLiteral("custom_1")));
    page.resize(980, 420);
    page.show();
    QCoreApplication::processEvents();
    QScrollArea *settingsScroll = page.findChild<QScrollArea *>();
    QVERIFY(settingsScroll);
    canvasToggle(&page)->click();
    QCoreApplication::processEvents();
    canvasToggle(&page)->click();
    QCoreApplication::processEvents();

    QPointer<QPushButton> settingsToggle(canvasToggle(&page));
    QVERIFY(settingsToggle);
    QScrollBar *bar = settingsScroll->verticalScrollBar();
    bar->setRange(0, 200);
    bar->setValue(120);
    const int scrollBefore = bar->value();
    FunctionFlowState remote =
        environment.flows.states.value(QStringLiteral("custom_1"));
    remote.draft.revision = 7;
    environment.settings->replaceFunctionFlowState(
        QStringLiteral("custom_1"),
        remote
    );

    page.refreshCanvasState();
    QCoreApplication::processEvents();

    QCOMPARE(canvasToggle(&page), settingsToggle.data());
    QCOMPARE(bar->value(), scrollBefore);
    QCOMPARE(
        page.canvasEditor()->controller()
            ->flowState().draft.revision,
        7
    );
}

QTEST_MAIN(FunctionCommandPageTests)

#include "function_command_page_tests.moc"
