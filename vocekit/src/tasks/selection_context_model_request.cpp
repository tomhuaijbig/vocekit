#include "selection_context_model_request.h"

#include "../config/app_settings_defaults.h"
#include "../domain/selection_context_actions.h"
#include "../providers/model_catalog.h"
#include "../result_flow_config.h"

namespace {

QString text8(const char *value)
{
    return QString::fromUtf8(value);
}

SelectionContextModelRequest baseResult(
    const SelectionContextModelRequestInput &input)
{
    SelectionContextModelRequest result;
    result.diagnosticSummary = QStringLiteral("action=%1;textLength=%2")
        .arg(input.actionId.trimmed())
        .arg(input.selectedText.size());
    return result;
}

SelectionContextModelRequest failure(
    const SelectionContextModelRequestInput &input,
    const QString &code,
    const QString &message)
{
    SelectionContextModelRequest result = baseResult(input);
    result.errorCode = code;
    result.errorMessage = message;
    return result;
}

const FunctionSettings *findFunction(
    const AppSettingsData &settings,
    const QString &id)
{
    const int index = settings.functionIndex(id);
    return index < 0 ? nullptr : &settings.functions.at(index);
}

QString configuredModel(
    const FunctionSettings &function,
    const QString &runtimeId)
{
    const QString model = function.modelId.trimmed();
    return model.isEmpty() ? defaultModelForFunction(runtimeId) : model;
}

bool containsModelId(
    const QVector<ModelOption> &options,
    const QString &modelId)
{
    for (const ModelOption &option : options) {
        if (option.id == modelId) {
            return true;
        }
    }
    return false;
}

void appendFollowUp(
    const SelectionContextModelRequestInput &input,
    QString *userPrompt)
{
    if (!userPrompt || input.followUpQuestion.trimmed().isEmpty()) {
        return;
    }
    *userPrompt += text8("\n\n此前回答：\n")
        + input.previousAnswer
        + text8("\n\n继续追问：\n")
        + input.followUpQuestion.trimmed();
}

} // namespace

SelectionContextModelRequest buildSelectionContextModelRequest(
    const SelectionContextModelRequestInput &input)
{
    const QString actionId = input.actionId.trimmed();
    if (input.selectedText.isEmpty()) {
        return failure(
            input,
            QStringLiteral("selection.text_missing"),
            text8("没有可处理的选中文字。")
        );
    }

    QString runtimeId;
    bool degraded = false;
    if (actionId == selectionContextActionTranslate()) {
        runtimeId = QStringLiteral("translate");
    } else if (actionId == selectionContextActionExplain()
               || actionId == selectionContextActionAiSearch()) {
        runtimeId = QStringLiteral("ask");
        degraded = actionId == selectionContextActionAiSearch();
    } else if (isSelectionContextFunctionAction(actionId)) {
        runtimeId = selectionContextFunctionId(actionId);
        const FunctionSettings *custom = findFunction(
            input.settings,
            runtimeId
        );
        if (!custom) {
            return failure(
                input,
                QStringLiteral("selection.function_missing"),
                text8("所选自定义功能不存在。")
            );
        }
        if (custom->builtIn) {
            return failure(
                input,
                QStringLiteral("selection.function_builtin_unsupported"),
                text8("内置功能不能作为自定义选区动作运行。")
            );
        }
        if (custom->executionMode != FunctionExecutionMode::Classic) {
            return failure(
                input,
                QStringLiteral("selection.function_canvas_unsupported"),
                text8("画布模式功能暂不支持选中文字快捷动作。")
            );
        }
    } else {
        return failure(
            input,
            QStringLiteral("selection.action_unsupported"),
            text8("不支持此选中文字动作。")
        );
    }

    const FunctionSettings *runtimeFunction = findFunction(
        input.settings,
        runtimeId
    );
    if (!runtimeFunction) {
        return failure(
            input,
            QStringLiteral("selection.function_missing"),
            text8("动作所需的功能配置不存在。")
        );
    }

    const bool builtInAiAction =
        actionId == selectionContextActionTranslate()
        || actionId == selectionContextActionExplain()
        || actionId == selectionContextActionAiSearch();
    const SelectionContextActionCustomization customization =
        input.settings.selectionContext.actionCustomizations.value(actionId);
    const QString customInstruction =
        customization.promptOverride.trimmed();
    const QString explicitModel = builtInAiAction
        ? normalizeExplicitModelId(customization.modelId)
        : QString();
    if (!explicitModel.isEmpty()
        && !containsModelId(input.modelOptions, explicitModel)) {
        return failure(
            input,
            QStringLiteral("selection.action_model_unavailable"),
            text8("所选模型当前不可用，请在设置中重新选择。")
        );
    }

    PromptRuntimeSnapshot promptSnapshot = input.prompts;
    promptSnapshot.settings = input.settings;
    SelectionContextModelRequest result = baseResult(input);
    result.valid = true;
    result.degraded = degraded;
    if (degraded) {
        result.degradedMessage =
            text8("未进行联网搜索，已使用普通 AI 解答");
    }
    result.modelRequest.modelId = explicitModel.isEmpty()
        ? configuredModel(*runtimeFunction, runtimeId)
        : explicitModel;
    result.modelRequest.systemPrompt = promptRuntimeForFunction(
        promptSnapshot,
        runtimeId,
        runtimeId == QStringLiteral("translate")
            ? text8("翻译为指定目标语言，只输出翻译结果。")
            : text8("基于选中文本回答用户问题。")
    );
    result.modelRequest.stream = true;
    result.modelRequest.useSystemProxy = input.settings.useSystemProxy;
    result.modelRequest.networkPolicy = normalizeNetworkPolicy(
        runtimeFunction->network.model
    );

    if (actionId == selectionContextActionTranslate()) {
        const QString configuredTarget =
            customization.targetLanguage.trimmed();
        const QString globalTarget = input.settings.targetLanguage.trimmed();
        const QString target = !configuredTarget.isEmpty()
            ? configuredTarget
            : (globalTarget.isEmpty() ? text8("简体中文") : globalTarget);
        result.modelRequest.userPrompt = text8("目标语言：")
            + target;
        if (!customInstruction.isEmpty()) {
            result.modelRequest.userPrompt += text8("\n\n用户要求：\n")
                + customInstruction;
        }
        result.modelRequest.userPrompt += text8("\n待翻译内容：\n")
            + input.selectedText;
    } else if (actionId == selectionContextActionExplain()) {
        result.modelRequest.userPrompt = text8("选中文本：\n")
            + input.selectedText
            + text8("\n\n用户问题：\n")
            + (customInstruction.isEmpty()
                ? text8("请解释这段文字的含义和关键信息。")
                : customInstruction);
    } else if (actionId == selectionContextActionAiSearch()) {
        result.modelRequest.systemPrompt += text8(
            "\n\n重要限制：没有进行实时网页检索，不得编造来源或时效性事实。"
            "请仅基于已有知识和所选文本作答。"
        );
        result.modelRequest.userPrompt = text8("选中文本：\n")
            + input.selectedText
            + text8("\n\n用户问题：\n")
            + (customInstruction.isEmpty()
                ? text8("请分析并回答与这段文字相关的问题。")
                : customInstruction);
    } else {
        result.modelRequest.userPrompt = text8("选中文本：\n")
            + input.selectedText
            + text8("\n\n用户要求：\n请直接按照提示词处理选中的文字。");
    }
    appendFollowUp(input, &result.modelRequest.userPrompt);
    return result;
}
