#include "../../src/domain/function_flow_errors.h"

#include <QtTest>

#include <QSet>

namespace {

QStringList requiredStableCodes()
{
    return QStringList()
        << QStringLiteral("flow_add_function_unavailable")
        << QStringLiteral("flow_auto_write_failed")
        << QStringLiteral("flow_builtin_function_immutable")
        << QStringLiteral("flow_cancelled")
        << QStringLiteral("flow_clipboard_unavailable")
        << QStringLiteral("flow_clipboard_wrong_thread")
        << QStringLiteral("flow_custom_function_builtin")
        << QStringLiteral("flow_cycle")
        << QStringLiteral("flow_dangling_edge")
        << QStringLiteral("flow_dependency_resolution_failed")
        << QStringLiteral("flow_draft_conflict")
        << QStringLiteral("flow_draft_revision_invalid")
        << QStringLiteral("flow_draft_stale")
        << QStringLiteral("flow_draft_unavailable")
        << QStringLiteral("flow_duplicate_connection")
        << QStringLiteral("flow_duplicate_edge_id")
        << QStringLiteral("flow_duplicate_node_id")
        << QStringLiteral("flow_edge_order_invalid")
        << QStringLiteral("flow_edge_type_unsupported")
        << QStringLiteral("flow_editor_save_failed")
        << QStringLiteral("flow_editor_save_unavailable")
        << QStringLiteral("flow_empty")
        << QStringLiteral("flow_enable_failed")
        << QStringLiteral("flow_enable_unavailable")
        << QStringLiteral("flow_enabled_node_unreachable")
        << QStringLiteral("flow_function_id_conflict")
        << QStringLiteral("flow_function_id_required")
        << QStringLiteral("flow_function_name_required")
        << QStringLiteral("flow_function_not_found")
        << QStringLiteral("flow_history_save_failed")
        << QStringLiteral("flow_input_injection_failed")
        << QStringLiteral("flow_input_role_invalid")
        << QStringLiteral("flow_json_invalid")
        << QStringLiteral("flow_model_failed")
        << QStringLiteral("flow_model_input_empty")
        << QStringLiteral("flow_model_reference_missing")
        << QStringLiteral("flow_node_config_invalid")
        << QStringLiteral("flow_node_failed")
        << QStringLiteral("flow_node_result_invalid")
        << QStringLiteral("flow_node_type_unsupported")
        << QStringLiteral("flow_ocr_engine_reference_missing")
        << QStringLiteral("flow_output_count")
        << QStringLiteral("flow_output_empty")
        << QStringLiteral("flow_popup_action_duplicate")
        << QStringLiteral("flow_popup_action_unsupported")
        << QStringLiteral("flow_port_cardinality")
        << QStringLiteral("flow_port_connection_missing")
        << QStringLiteral("flow_port_direction")
        << QStringLiteral("flow_prompt_reference_missing")
        << QStringLiteral("flow_publish_busy")
        << QStringLiteral("flow_publish_unavailable")
        << QStringLiteral("flow_published_hash_mismatch")
        << QStringLiteral("flow_published_repair_confirmation_required")
        << QStringLiteral("flow_published_unavailable")
        << QStringLiteral("flow_replace_selection_context_missing")
        << QStringLiteral("flow_replace_selection_unavailable")
        << QStringLiteral("flow_required_input_empty")
        << QStringLiteral("flow_result_action_failed")
        << QStringLiteral("flow_result_popup_failed")
        << QStringLiteral("flow_runtime_adapter_missing")
        << QStringLiteral("flow_save_failed")
        << QStringLiteral("flow_save_unavailable")
        << QStringLiteral("flow_scheduler_deadlock")
        << QStringLiteral("flow_scheduler_node_missing")
        << QStringLiteral("flow_scheduler_state_invalid")
        << QStringLiteral("flow_schema_newer")
        << QStringLiteral("flow_screenshot_context_missing")
        << QStringLiteral("flow_screenshot_failed")
        << QStringLiteral("flow_screenshot_panel_failed")
        << QStringLiteral("flow_selection_failed")
        << QStringLiteral("flow_self_edge")
        << QStringLiteral("flow_size_limit")
        << QStringLiteral("flow_speech_provider_reference_missing")
        << QStringLiteral("flow_state_read_failed")
        << QStringLiteral("flow_state_read_unavailable")
        << QStringLiteral("flow_state_target_missing")
        << QStringLiteral("flow_stream_topology_unsupported")
        << QStringLiteral("flow_target_window_activation_failed")
        << QStringLiteral("flow_target_window_unavailable")
        << QStringLiteral("flow_trigger_not_configured")
        << QStringLiteral("flow_trigger_shortcut_conflict")
        << QStringLiteral("flow_trigger_shortcut_missing")
        << QStringLiteral("flow_trigger_unavailable")
        << QStringLiteral("flow_unknown_port")
        << QStringLiteral("flow_validation_failed")
        << QStringLiteral("flow_voice_failed");
}

} // namespace

class FunctionFlowErrorsTests : public QObject
{
    Q_OBJECT

private slots:
    void coversEveryStableCode();
    void groupsFaqIdsByRecoveryPath();
    void usesExactTriggerNotConfiguredMessage();
    void returnsSafeUserMessages();
    void handlesUnknownCodesWithoutLeakingDetails();
};

void FunctionFlowErrorsTests::coversEveryStableCode()
{
    const QStringList actual = functionFlowStableErrorCodes();
    QCOMPARE(QSet<QString>::fromList(actual).size(), actual.size());

    const QStringList required = requiredStableCodes();
    for (const QString &code : required) {
        QVERIFY2(actual.contains(code), qPrintable(QStringLiteral("missing code: ") + code));
        QVERIFY2(
            isFunctionFlowStableErrorCode(code),
            qPrintable(QStringLiteral("not recognized: ") + code)
        );
        QVERIFY2(
            !functionFlowErrorFaqId(code).isEmpty(),
            qPrintable(QStringLiteral("missing FAQ id: ") + code)
        );

        OperationError error;
        error.code = code;
        const QString message = functionFlowUserMessage(error);
        QVERIFY2(
            !message.trimmed().isEmpty(),
            qPrintable(QStringLiteral("missing user message: ") + code)
        );
    }
}

void FunctionFlowErrorsTests::groupsFaqIdsByRecoveryPath()
{
    QCOMPARE(
        functionFlowErrorFaqId(QStringLiteral("flow_schema_newer")),
        QStringLiteral("function-flow-schema")
    );
    QCOMPARE(
        functionFlowErrorFaqId(QStringLiteral("flow_draft_stale")),
        QStringLiteral("function-flow-draft")
    );
    QCOMPARE(
        functionFlowErrorFaqId(QStringLiteral("flow_cycle")),
        QStringLiteral("function-flow-publish")
    );
    QCOMPARE(
        functionFlowErrorFaqId(QStringLiteral("flow_selection_failed")),
        QStringLiteral("function-flow-input")
    );
    QCOMPARE(
        functionFlowErrorFaqId(QStringLiteral("flow_model_failed")),
        QStringLiteral("function-flow-model")
    );
    QCOMPARE(
        functionFlowErrorFaqId(QStringLiteral("flow_auto_write_failed")),
        QStringLiteral("function-flow-output")
    );
    QCOMPARE(
        functionFlowErrorFaqId(QStringLiteral("flow_cancelled")),
        QStringLiteral("function-flow-runtime")
    );
    QCOMPARE(
        functionFlowErrorFaqId(
            QStringLiteral("flow_trigger_not_configured")
        ),
        QStringLiteral("function-flow-runtime")
    );
}

void FunctionFlowErrorsTests::
usesExactTriggerNotConfiguredMessage()
{
    OperationError error;
    error.code = QStringLiteral("flow_trigger_not_configured");
    QCOMPARE(
        functionFlowUserMessage(error),
        QString::fromUtf8("当前画布未配置此入口。")
    );
}

void FunctionFlowErrorsTests::returnsSafeUserMessages()
{
    OperationError error;
    error.message = QStringLiteral("完整选中文字");
    error.detail = QStringLiteral("data:image/png;base64,secret sk-private");

    const QStringList codes = functionFlowStableErrorCodes();
    for (const QString &code : codes) {
        error.code = code;
        const QString message = functionFlowUserMessage(error);
        QVERIFY(!message.contains(QStringLiteral("完整选中文字")));
        QVERIFY(!message.contains(QStringLiteral("data:image")));
        QVERIFY(!message.contains(QStringLiteral("sk-private")));
    }
}

void FunctionFlowErrorsTests::handlesUnknownCodesWithoutLeakingDetails()
{
    OperationError error;
    error.code = QStringLiteral("provider_raw_error");
    error.message = QStringLiteral("完整用户正文");
    error.detail = QStringLiteral("Authorization: Bearer secret");

    QVERIFY(functionFlowErrorFaqId(error.code).isEmpty());
    QVERIFY(!isFunctionFlowStableErrorCode(error.code));

    const QString message = functionFlowUserMessage(error);
    QVERIFY(!message.trimmed().isEmpty());
    QVERIFY(!message.contains(error.message));
    QVERIFY(!message.contains(error.detail));
}

QTEST_APPLESS_MAIN(FunctionFlowErrorsTests)

#include "function_flow_errors_tests.moc"
