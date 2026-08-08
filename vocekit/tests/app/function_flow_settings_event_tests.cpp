#include <QtTest>

#include "../../src/app/application_events.h"
#include "../../src/ui/hub_refresh_coordinator_bundle.h"

class FunctionFlowSettingsEventTests : public QObject
{
    Q_OBJECT

private slots:
    void draftAndEditorEventsDoNotReloadRuntimeOrHotkeys();
    void publishedEventRefreshesRuntimeAndHotkeysOnly();
    void executionModeEventRefreshesFunctionRuntimeAndHotkeysOnce();
};

void FunctionFlowSettingsEventTests::
draftAndEditorEventsDoNotReloadRuntimeOrHotkeys()
{
    int flowReloads = 0;
    int canvasRefreshes = 0;
    int activeFunctionRefreshes = 0;
    int runtimeRefreshes = 0;
    int hotkeyRefreshes = 0;
    HubRefreshCoordinatorBundleActions actions;
    actions.reloadFunctionFlows =
        [&](const QStringList &ids) {
            QCOMPARE(ids, QStringList() << QStringLiteral("custom_1"));
            ++flowReloads;
        };
    actions.refreshActiveCanvas =
        [&]() { ++canvasRefreshes; };
    actions.refreshActiveFunction =
        [&]() { ++activeFunctionRefreshes; };
    actions.refreshRuntime =
        [&](const QStringList &) { ++runtimeRefreshes; };
    actions.refreshHotkeys =
        [&](const QStringList &) { ++hotkeyRefreshes; };

    ApplicationEvents events;
    HubRefreshCoordinatorBundle bundle(actions);
    bundle.setApplicationEvents(&events);

    SettingsChangeSet draft;
    draft.keys << functionFlowDraftSettingsKey();
    draft.functionIds << QStringLiteral("custom_1");
    events.publishSettingsChanged(draft);

    SettingsChangeSet editor;
    editor.keys << functionFlowEditorStateSettingsKey();
    editor.functionIds << QStringLiteral("custom_1");
    events.publishSettingsChanged(editor);

    QCOMPARE(flowReloads, 2);
    QCOMPARE(canvasRefreshes, 2);
    QCOMPARE(activeFunctionRefreshes, 0);
    QCOMPARE(runtimeRefreshes, 0);
    QCOMPARE(hotkeyRefreshes, 0);
}

void FunctionFlowSettingsEventTests::
publishedEventRefreshesRuntimeAndHotkeysOnly()
{
    int flowReloads = 0;
    int canvasRefreshes = 0;
    int activeFunctionRefreshes = 0;
    int runtimeRefreshes = 0;
    int hotkeyRefreshes = 0;
    const QStringList expectedIds =
        QStringList() << QStringLiteral("custom_2");
    HubRefreshCoordinatorBundleActions actions;
    actions.reloadFunctionFlows =
        [&](const QStringList &ids) {
            QCOMPARE(ids, expectedIds);
            ++flowReloads;
        };
    actions.refreshActiveCanvas =
        [&]() { ++canvasRefreshes; };
    actions.refreshActiveFunction =
        [&]() { ++activeFunctionRefreshes; };
    actions.refreshRuntime =
        [&](const QStringList &ids) {
            QCOMPARE(ids, expectedIds);
            ++runtimeRefreshes;
        };
    actions.refreshHotkeys =
        [&](const QStringList &ids) {
            QCOMPARE(ids, expectedIds);
            ++hotkeyRefreshes;
        };

    ApplicationEvents events;
    HubRefreshCoordinatorBundle bundle(actions);
    bundle.setApplicationEvents(&events);

    SettingsChangeSet published;
    published.keys << functionFlowPublishedSettingsKey();
    published.functionIds = expectedIds;
    events.publishSettingsChanged(published);

    QCOMPARE(flowReloads, 1);
    QCOMPARE(canvasRefreshes, 1);
    QCOMPARE(activeFunctionRefreshes, 0);
    QCOMPARE(runtimeRefreshes, 1);
    QCOMPARE(hotkeyRefreshes, 1);
}

void FunctionFlowSettingsEventTests::
executionModeEventRefreshesFunctionRuntimeAndHotkeysOnce()
{
    int flowReloads = 0;
    int canvasRefreshes = 0;
    int activeFunctionRefreshes = 0;
    int runtimeRefreshes = 0;
    int hotkeyRefreshes = 0;
    const QStringList expectedIds =
        QStringList() << QStringLiteral("custom_3");
    HubRefreshCoordinatorBundleActions actions;
    actions.reloadFunctionFlows =
        [&](const QStringList &ids) {
            QCOMPARE(ids, expectedIds);
            ++flowReloads;
        };
    actions.refreshActiveCanvas =
        [&]() { ++canvasRefreshes; };
    actions.refreshActiveFunction =
        [&]() { ++activeFunctionRefreshes; };
    actions.refreshRuntime =
        [&](const QStringList &ids) {
            QCOMPARE(ids, expectedIds);
            ++runtimeRefreshes;
        };
    actions.refreshHotkeys =
        [&](const QStringList &ids) {
            QCOMPARE(ids, expectedIds);
            ++hotkeyRefreshes;
        };

    ApplicationEvents events;
    HubRefreshCoordinatorBundle bundle(actions);
    bundle.setApplicationEvents(&events);

    SettingsChangeSet mode;
    mode.keys << functionExecutionModeSettingsKey();
    mode.functionIds = expectedIds;
    events.publishSettingsChanged(mode);

    QCOMPARE(flowReloads, 1);
    QCOMPARE(canvasRefreshes, 1);
    QCOMPARE(activeFunctionRefreshes, 1);
    QCOMPARE(runtimeRefreshes, 1);
    QCOMPARE(hotkeyRefreshes, 1);
}

QTEST_MAIN(FunctionFlowSettingsEventTests)

#include "function_flow_settings_event_tests.moc"
