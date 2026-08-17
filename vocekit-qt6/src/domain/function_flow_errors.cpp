#include "function_flow_errors.h"

#include <QHash>

#include <algorithm>

namespace {

struct FunctionFlowErrorDefinition
{
    QString faqId;
    QString message;
};

QString flowErrorText(const char *text)
{
    return QString::fromUtf8(text);
}

void addDefinitions(
    QHash<QString, FunctionFlowErrorDefinition> *definitions,
    const QStringList &codes,
    const QString &faqId,
    const QString &message
)
{
    if (!definitions) {
        return;
    }
    for (const QString &code : codes) {
        FunctionFlowErrorDefinition definition;
        definition.faqId = faqId;
        definition.message = message;
        definitions->insert(code, definition);
    }
}

void addDefinition(
    QHash<QString, FunctionFlowErrorDefinition> *definitions,
    const QString &code,
    const QString &faqId,
    const QString &message
)
{
    addDefinitions(
        definitions,
        QStringList() << code,
        faqId,
        message
    );
}

const QHash<QString, FunctionFlowErrorDefinition> &errorDefinitions()
{
    static const QHash<QString, FunctionFlowErrorDefinition> definitions = [] {
        QHash<QString, FunctionFlowErrorDefinition> result;

        const QString schemaFaq = QStringLiteral("function-flow-schema");
        const QString draftFaq = QStringLiteral("function-flow-draft");
        const QString publishFaq = QStringLiteral("function-flow-publish");
        const QString inputFaq = QStringLiteral("function-flow-input");
        const QString modelFaq = QStringLiteral("function-flow-model");
        const QString outputFaq = QStringLiteral("function-flow-output");
        const QString historyFaq = QStringLiteral("function-flow-history");
        const QString runtimeFaq = QStringLiteral("function-flow-runtime");

        addDefinitions(
            &result,
            QStringList()
                << QStringLiteral("flow_json_invalid")
                << QStringLiteral("flow_node_type_unsupported")
                << QStringLiteral("flow_published_hash_mismatch")
                << QStringLiteral("flow_published_repair_confirmation_required")
                << QStringLiteral("flow_schema_newer"),
            schemaFaq,
            flowErrorText("流程配置版本或发布内容不兼容，请检查后重新发布。")
        );

        addDefinitions(
            &result,
            QStringList()
                << QStringLiteral("flow_add_function_unavailable")
                << QStringLiteral("flow_builtin_function_immutable")
                << QStringLiteral("flow_custom_function_builtin")
                << QStringLiteral("flow_draft_conflict")
                << QStringLiteral("flow_draft_revision_invalid")
                << QStringLiteral("flow_draft_stale")
                << QStringLiteral("flow_draft_unavailable")
                << QStringLiteral("flow_editor_save_failed")
                << QStringLiteral("flow_editor_save_unavailable")
                << QStringLiteral("flow_function_id_conflict")
                << QStringLiteral("flow_function_id_required")
                << QStringLiteral("flow_function_name_required")
                << QStringLiteral("flow_function_not_found")
                << QStringLiteral("flow_save_failed")
                << QStringLiteral("flow_save_unavailable")
                << QStringLiteral("flow_state_read_failed")
                << QStringLiteral("flow_state_read_unavailable")
                << QStringLiteral("flow_state_target_missing"),
            draftFaq,
            flowErrorText("流程草稿或功能设置无法保存，请重新打开编辑器后再试。")
        );

        addDefinitions(
            &result,
            QStringList()
                << QStringLiteral("flow_cycle")
                << QStringLiteral("flow_dangling_edge")
                << QStringLiteral("flow_duplicate_connection")
                << QStringLiteral("flow_duplicate_edge_id")
                << QStringLiteral("flow_duplicate_node_id")
                << QStringLiteral("flow_edge_order_invalid")
                << QStringLiteral("flow_edge_type_unsupported")
                << QStringLiteral("flow_empty")
                << QStringLiteral("flow_enable_failed")
                << QStringLiteral("flow_enable_unavailable")
                << QStringLiteral("flow_enabled_node_unreachable")
                << QStringLiteral("flow_input_role_invalid")
                << QStringLiteral("flow_node_config_invalid")
                << QStringLiteral("flow_output_count")
                << QStringLiteral("flow_popup_action_duplicate")
                << QStringLiteral("flow_popup_action_unsupported")
                << QStringLiteral("flow_port_cardinality")
                << QStringLiteral("flow_port_connection_missing")
                << QStringLiteral("flow_port_direction")
                << QStringLiteral("flow_publish_busy")
                << QStringLiteral("flow_publish_unavailable")
                << QStringLiteral("flow_published_unavailable")
                << QStringLiteral("flow_replace_selection_context_missing")
                << QStringLiteral("flow_screenshot_context_missing")
                << QStringLiteral("flow_self_edge")
                << QStringLiteral("flow_size_limit")
                << QStringLiteral("flow_stream_topology_unsupported")
                << QStringLiteral("flow_trigger_shortcut_conflict")
                << QStringLiteral("flow_trigger_shortcut_missing")
                << QStringLiteral("flow_unknown_port")
                << QStringLiteral("flow_validation_failed"),
            publishFaq,
            flowErrorText("流程图不满足发布要求，请根据编辑器提示修正节点、端口或触发设置。")
        );

        addDefinitions(
            &result,
            QStringList()
                << QStringLiteral("flow_model_failed")
                << QStringLiteral("flow_model_input_empty")
                << QStringLiteral("flow_model_reference_missing")
                << QStringLiteral("flow_ocr_engine_reference_missing")
                << QStringLiteral("flow_prompt_reference_missing")
                << QStringLiteral("flow_speech_provider_reference_missing"),
            modelFaq,
            flowErrorText("流程引用的模型、提示词或服务不可用，请在设置中重新选择。")
        );

        addDefinitions(
            &result,
            QStringList()
                << QStringLiteral("flow_required_input_empty")
                << QStringLiteral("flow_screenshot_failed")
                << QStringLiteral("flow_selection_failed")
                << QStringLiteral("flow_voice_failed"),
            inputFaq,
            flowErrorText("流程没有取得所需输入，请检查当前触发入口、选区、语音或截图。")
        );

        addDefinitions(
            &result,
            QStringList()
                << QStringLiteral("flow_auto_write_failed")
                << QStringLiteral("flow_clipboard_unavailable")
                << QStringLiteral("flow_clipboard_wrong_thread")
                << QStringLiteral("flow_input_injection_failed")
                << QStringLiteral("flow_output_empty")
                << QStringLiteral("flow_replace_selection_unavailable")
                << QStringLiteral("flow_result_action_failed")
                << QStringLiteral("flow_result_popup_failed")
                << QStringLiteral("flow_screenshot_panel_failed")
                << QStringLiteral("flow_target_window_activation_failed")
                << QStringLiteral("flow_target_window_unavailable"),
            outputFaq,
            flowErrorText("流程结果无法显示或写入原目标窗口，请保留结果并手动复制。")
        );

        addDefinitions(
            &result,
            QStringList() << QStringLiteral("flow_history_save_failed"),
            historyFaq,
            flowErrorText("流程结果的历史记录保存失败，请检查历史目录后重试。")
        );

        addDefinitions(
            &result,
            QStringList()
                << QStringLiteral("flow_cancelled")
                << QStringLiteral("flow_dependency_resolution_failed")
                << QStringLiteral("flow_node_failed")
                << QStringLiteral("flow_node_result_invalid")
                << QStringLiteral("flow_runtime_adapter_missing")
                << QStringLiteral("flow_scheduler_deadlock")
                << QStringLiteral("flow_scheduler_node_missing")
                << QStringLiteral("flow_scheduler_state_invalid")
                << QStringLiteral("flow_trigger_not_configured")
                << QStringLiteral("flow_trigger_unavailable"),
            runtimeFaq,
            flowErrorText("流程运行未能完成，请重试；若问题持续，请查看常见问题。")
        );

        addDefinition(
            &result,
            QStringLiteral("flow_schema_newer"),
            schemaFaq,
            flowErrorText("该流程由更高版本的软件创建，请升级后再使用。")
        );
        addDefinition(
            &result,
            QStringLiteral("flow_published_hash_mismatch"),
            schemaFaq,
            flowErrorText("已发布流程校验失败，已停止运行；请确认后修复并重新发布。")
        );
        addDefinition(
            &result,
            QStringLiteral("flow_published_repair_confirmation_required"),
            schemaFaq,
            flowErrorText("已发布流程需要修复，请确认替换后重新发布。")
        );
        addDefinition(
            &result,
            QStringLiteral("flow_draft_stale"),
            draftFaq,
            flowErrorText("流程草稿已被其它修改更新，请重新加载后再保存。")
        );
        addDefinition(
            &result,
            QStringLiteral("flow_draft_conflict"),
            draftFaq,
            flowErrorText("流程草稿发生版本冲突，请重新加载后再继续编辑。")
        );
        addDefinition(
            &result,
            QStringLiteral("flow_cycle"),
            publishFaq,
            flowErrorText("流程图中存在环路，请删除形成循环的连线后再发布。")
        );
        addDefinition(
            &result,
            QStringLiteral("flow_port_connection_missing"),
            publishFaq,
            flowErrorText("流程节点缺少必需端口连接，请补齐后再发布。")
        );
        addDefinition(
            &result,
            QStringLiteral("flow_screenshot_context_missing"),
            publishFaq,
            flowErrorText("当前触发入口无法提供截图，请调整截图来源或触发设置。")
        );
        addDefinition(
            &result,
            QStringLiteral("flow_replace_selection_context_missing"),
            publishFaq,
            flowErrorText("当前触发入口无法提供替换所需的选区，请调整输入或写入方式。")
        );
        addDefinition(
            &result,
            QStringLiteral("flow_model_reference_missing"),
            modelFaq,
            flowErrorText("流程引用的模型已失效，请重新选择模型并发布。")
        );
        addDefinition(
            &result,
            QStringLiteral("flow_prompt_reference_missing"),
            modelFaq,
            flowErrorText("流程引用的提示词已失效，请重新选择提示词并发布。")
        );
        addDefinition(
            &result,
            QStringLiteral("flow_replace_selection_unavailable"),
            outputFaq,
            flowErrorText("原目标窗口当前没有可替换的选区，请重新选择文字后再试。")
        );
        addDefinition(
            &result,
            QStringLiteral("flow_target_window_unavailable"),
            outputFaq,
            flowErrorText("原目标窗口已失效，为避免误写入其它窗口，本次自动写入已停止。")
        );
        addDefinition(
            &result,
            QStringLiteral("flow_auto_write_failed"),
            outputFaq,
            flowErrorText("自动写入失败，请在结果窗口中手动复制文字。")
        );
        addDefinition(
            &result,
            QStringLiteral("flow_cancelled"),
            runtimeFaq,
            flowErrorText("流程已取消。")
        );
        addDefinition(
            &result,
            QStringLiteral("flow_trigger_not_configured"),
            runtimeFaq,
            flowErrorText("当前画布未配置此入口。")
        );

        return result;
    }();
    return definitions;
}

} // namespace

QStringList functionFlowStableErrorCodes()
{
    QStringList codes = errorDefinitions().keys();
    std::sort(codes.begin(), codes.end());
    return codes;
}

bool isFunctionFlowStableErrorCode(const QString &code)
{
    return errorDefinitions().contains(code.trimmed());
}

QString functionFlowErrorFaqId(const QString &code)
{
    return errorDefinitions().value(code.trimmed()).faqId;
}

QString functionFlowUserMessage(const OperationError &error)
{
    const auto definition = errorDefinitions().constFind(error.code.trimmed());
    if (definition != errorDefinitions().constEnd()) {
        return definition.value().message;
    }
    return flowErrorText(
        "流程操作失败，请重试；若问题持续，请查看常见问题。"
    );
}
