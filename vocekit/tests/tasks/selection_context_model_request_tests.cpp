#include <QtTest>

#include "../../src/tasks/selection_context_model_request.h"

namespace {

QString text8(const char *value)
{
    return QString::fromUtf8(value);
}

FunctionSettings function(
    const QString &id,
    const QString &model,
    const QString &promptId,
    bool builtIn,
    FunctionExecutionMode mode = FunctionExecutionMode::Classic)
{
    FunctionSettings value;
    value.id = id;
    value.name = id;
    value.modelId = model;
    value.promptId = promptId;
    value.builtIn = builtIn;
    value.executionMode = mode;
    value.network.model = QStringLiteral("systemProxy");
    return value;
}

PromptLibraryItem prompt(
    const QString &id,
    const QString &content)
{
    PromptLibraryItem value;
    value.id = id;
    value.name = id;
    value.content = content;
    return value;
}

SelectionContextModelRequestInput baseInput()
{
    SelectionContextModelRequestInput input;
    input.selectedText = text8("这是需要处理的原文");
    input.settings.targetLanguage = text8("日语");
    input.settings.useSystemProxy = true;
    input.settings.functions
        << function(
            QStringLiteral("translate"),
            QStringLiteral("openai:gpt-5.6-terra"),
            QStringLiteral("translate-test"),
            true
        )
        << function(
            QStringLiteral("ask"),
            QStringLiteral("claude:claude-sonnet-5"),
            QStringLiteral("ask-test"),
            true
        );
    input.prompts.libraryItems
        << prompt(QStringLiteral("translate-test"), text8("翻译测试提示"))
        << prompt(QStringLiteral("ask-test"), text8("问答测试提示"));
    return input;
}

} // namespace

class SelectionContextModelRequestTests : public QObject
{
    Q_OBJECT

private slots:
    void eachAiActionUsesOnlyItsOwnCustomization()
    {
        SelectionContextModelRequestInput input = baseInput();
        input.modelOptions = QVector<ModelOption>()
            << ModelOption{QStringLiteral("openai:gpt-5.6-sol"),
                           QStringLiteral("GPT-5.6 Sol"),
                           QStringLiteral("OpenAI")};
        SelectionContextActionCustomization explain;
        explain.modelId = QStringLiteral("openai:gpt-5.6-sol");
        explain.promptOverride = text8("分三层解释");
        input.settings.selectionContext.actionCustomizations.insert(
            QStringLiteral("explain"),
            explain
        );
        SelectionContextActionCustomization translate;
        translate.modelId = QStringLiteral("openai:gpt-5.6-terra");
        translate.promptOverride = text8("使用翻译动作指令");
        input.settings.selectionContext.actionCustomizations.insert(
            QStringLiteral("translate"),
            translate
        );
        SelectionContextActionCustomization aiSearch;
        aiSearch.modelId = QStringLiteral("gpt-5.4-mini");
        aiSearch.promptOverride = text8("使用搜索动作指令");
        input.settings.selectionContext.actionCustomizations.insert(
            QStringLiteral("ai-search"),
            aiSearch
        );
        input.actionId = QStringLiteral("explain");

        const SelectionContextModelRequest built =
            buildSelectionContextModelRequest(input);

        QVERIFY(built.valid);
        QCOMPARE(
            built.modelRequest.modelId,
            QStringLiteral("openai:gpt-5.6-sol")
        );
        QVERIFY(built.modelRequest.userPrompt.contains(text8("分三层解释")));
        QVERIFY(!built.modelRequest.userPrompt.contains(translate.promptOverride));
        QVERIFY(!built.modelRequest.userPrompt.contains(aiSearch.promptOverride));

        input.modelOptions = QVector<ModelOption>()
            << ModelOption{QStringLiteral("openai:gpt-5.6-terra"),
                           QStringLiteral("GPT-5.6 Terra"),
                           QStringLiteral("OpenAI")};
        input.actionId = QStringLiteral("translate");
        const SelectionContextModelRequest translated =
            buildSelectionContextModelRequest(input);
        QVERIFY(translated.valid);
        QCOMPARE(
            translated.modelRequest.modelId,
            QStringLiteral("openai:gpt-5.6-terra")
        );
        QVERIFY(translated.modelRequest.userPrompt.contains(
            translate.promptOverride
        ));
        QVERIFY(!translated.modelRequest.userPrompt.contains(
            explain.promptOverride
        ));

        input.modelOptions = QVector<ModelOption>()
            << ModelOption{QStringLiteral("openai:gpt-5.6-luna"),
                           QStringLiteral("GPT-5.6 Luna"),
                           QStringLiteral("OpenAI")};
        input.actionId = QStringLiteral("ai-search");
        const SelectionContextModelRequest searched =
            buildSelectionContextModelRequest(input);
        QVERIFY(searched.valid);
        QCOMPARE(
            searched.modelRequest.modelId,
            QStringLiteral("openai:gpt-5.6-luna")
        );
        QVERIFY(searched.modelRequest.userPrompt.contains(
            aiSearch.promptOverride
        ));
        QVERIFY(!searched.modelRequest.userPrompt.contains(
            explain.promptOverride
        ));
    }

    void translateOverrideDoesNotModifyGlobalTargetLanguage()
    {
        SelectionContextModelRequestInput input = baseInput();
        const QString globalTarget = input.settings.targetLanguage;
        SelectionContextActionCustomization translate =
            input.settings.selectionContext.actionCustomizations.value(
                QStringLiteral("translate")
            );
        translate.targetLanguage = text8("韩语");
        translate.promptOverride = text8("使用自然口语翻译");
        input.settings.selectionContext.actionCustomizations.insert(
            QStringLiteral("translate"),
            translate
        );
        input.actionId = QStringLiteral("translate");

        const SelectionContextModelRequest built =
            buildSelectionContextModelRequest(input);

        QVERIFY(built.valid);
        QVERIFY(built.modelRequest.userPrompt.contains(text8("目标语言：韩语")));
        QVERIFY(built.modelRequest.userPrompt.contains(text8("使用自然口语翻译")));
        QCOMPARE(input.settings.targetLanguage, globalTarget);
    }

    void blankOverrideUsesExistingBuiltInPrompt()
    {
        SelectionContextModelRequestInput input = baseInput();
        SelectionContextActionCustomization explain =
            input.settings.selectionContext.actionCustomizations.value(
                QStringLiteral("explain")
            );
        explain.promptOverride = QStringLiteral("   ");
        input.settings.selectionContext.actionCustomizations.insert(
            QStringLiteral("explain"),
            explain
        );
        input.actionId = QStringLiteral("explain");

        const SelectionContextModelRequest built =
            buildSelectionContextModelRequest(input);

        QVERIFY(built.valid);
        QCOMPARE(built.modelRequest.systemPrompt, text8("问答测试提示"));
        QVERIFY(built.modelRequest.userPrompt.contains(
            text8("请解释这段文字的含义和关键信息。")
        ));
    }

    void unavailableExplicitModelReturnsSelectionModelUnavailable()
    {
        SelectionContextModelRequestInput input = baseInput();
        input.modelOptions = QVector<ModelOption>()
            << ModelOption{QStringLiteral("openai:gpt-5.6-sol"),
                           QStringLiteral("GPT-5.6 Sol"),
                           QStringLiteral("OpenAI")};
        SelectionContextActionCustomization explain =
            input.settings.selectionContext.actionCustomizations.value(
                QStringLiteral("explain")
            );
        explain.modelId = QStringLiteral("custom:removed-model");
        explain.promptOverride = text8("绝不应出现在诊断中");
        input.settings.selectionContext.actionCustomizations.insert(
            QStringLiteral("explain"),
            explain
        );
        input.actionId = QStringLiteral("explain");

        const SelectionContextModelRequest built =
            buildSelectionContextModelRequest(input);

        QVERIFY(!built.valid);
        QCOMPARE(
            built.errorCode,
            QStringLiteral("selection.action_model_unavailable")
        );
        QVERIFY(built.modelRequest.modelId.isEmpty());
        QVERIFY(!built.diagnosticSummary.contains(explain.promptOverride));
        QVERIFY(!built.diagnosticSummary.contains(input.selectedText));
    }

    void aiSearchCustomPromptCannotRemoveNonSearchWarningOrSafetySuffix()
    {
        SelectionContextModelRequestInput input = baseInput();
        SelectionContextActionCustomization aiSearch =
            input.settings.selectionContext.actionCustomizations.value(
                QStringLiteral("ai-search")
            );
        aiSearch.promptOverride =
            text8("声称已实时检索并编造来源");
        input.settings.selectionContext.actionCustomizations.insert(
            QStringLiteral("ai-search"),
            aiSearch
        );
        input.actionId = QStringLiteral("ai-search");

        const SelectionContextModelRequest built =
            buildSelectionContextModelRequest(input);

        QVERIFY(built.valid);
        QVERIFY(built.degraded);
        QCOMPARE(
            built.degradedMessage,
            text8("未进行联网搜索，已使用普通 AI 解答")
        );
        QVERIFY(built.modelRequest.userPrompt.contains(aiSearch.promptOverride));
        QVERIFY(built.modelRequest.systemPrompt.contains(
            text8("没有进行实时网页检索，不得编造来源或时效性事实。")
        ));
        QVERIFY(built.modelRequest.systemPrompt.endsWith(
            text8("请仅基于已有知识和所选文本作答。")
        ));
    }

    void followUpKeepsSelectedTextWrapperAndCustomInstruction()
    {
        SelectionContextModelRequestInput input = baseInput();
        SelectionContextActionCustomization explain =
            input.settings.selectionContext.actionCustomizations.value(
                QStringLiteral("explain")
            );
        explain.promptOverride = text8("先说结论，再分析三层原因");
        input.settings.selectionContext.actionCustomizations.insert(
            QStringLiteral("explain"),
            explain
        );
        input.actionId = QStringLiteral("explain");
        input.previousAnswer = text8("上一轮答案");
        input.followUpQuestion = text8("第二层为什么？");

        const SelectionContextModelRequest built =
            buildSelectionContextModelRequest(input);

        QVERIFY(built.valid);
        QVERIFY(built.modelRequest.userPrompt.startsWith(
            text8("选中文本：\n") + input.selectedText
        ));
        QVERIFY(built.modelRequest.userPrompt.contains(explain.promptOverride));
        QVERIFY(built.modelRequest.userPrompt.contains(input.previousAnswer));
        QVERIFY(built.modelRequest.userPrompt.contains(input.followUpQuestion));
    }

    void translateUsesTranslatePromptModelAndConfiguredTargetLanguage()
    {
        SelectionContextModelRequestInput input = baseInput();
        input.actionId = QStringLiteral("translate");

        const SelectionContextModelRequest built =
            buildSelectionContextModelRequest(input);

        QVERIFY(built.valid);
        QCOMPARE(
            built.modelRequest.modelId,
            QStringLiteral("openai:gpt-5.6-terra")
        );
        QCOMPARE(built.modelRequest.systemPrompt, text8("翻译测试提示"));
        QVERIFY(built.modelRequest.userPrompt.contains(text8("目标语言：日语")));
        QVERIFY(built.modelRequest.userPrompt.contains(input.selectedText));
        QVERIFY(built.modelRequest.stream);
        QVERIFY(built.modelRequest.useSystemProxy);
        QCOMPARE(
            built.modelRequest.networkPolicy,
            QStringLiteral("systemProxy")
        );
    }

    void explainUsesAskPromptAndPreservesTheOriginalSelection()
    {
        SelectionContextModelRequestInput input = baseInput();
        input.actionId = QStringLiteral("explain");

        const SelectionContextModelRequest built =
            buildSelectionContextModelRequest(input);

        QVERIFY(built.valid);
        QCOMPARE(
            built.modelRequest.modelId,
            QStringLiteral("claude:claude-sonnet-5")
        );
        QCOMPARE(built.modelRequest.systemPrompt, text8("问答测试提示"));
        QVERIFY(built.modelRequest.userPrompt.contains(input.selectedText));
        QVERIFY(built.modelRequest.userPrompt.contains(text8("解释")));
    }

    void customFunctionUsesTheNamedFunctionRuntimeWithoutChangingSettings()
    {
        SelectionContextModelRequestInput input = baseInput();
        FunctionSettings custom = function(
            QStringLiteral("summarize"),
            QStringLiteral("deepseek-v4-pro"),
            QString(),
            false
        );
        custom.prompt = text8("只输出一句摘要");
        input.settings.functions.append(custom);
        input.actionId = QStringLiteral("function:summarize");

        const SelectionContextModelRequest built =
            buildSelectionContextModelRequest(input);

        QVERIFY(built.valid);
        QCOMPARE(built.modelRequest.modelId, custom.modelId);
        QCOMPARE(built.modelRequest.systemPrompt, custom.prompt);
        QVERIFY(built.modelRequest.userPrompt.contains(input.selectedText));
        QCOMPARE(input.settings.function(QStringLiteral("summarize")).prompt,
                 custom.prompt);
        QCOMPARE(input.settings.function(QStringLiteral("summarize")).modelId,
                 custom.modelId);
    }

    void canvasModeCustomFunctionReturnsStableUnsupportedWithoutProviderCall()
    {
        SelectionContextModelRequestInput input = baseInput();
        input.settings.functions.append(function(
            QStringLiteral("canvas-only"),
            QStringLiteral("deepseek-v4-pro"),
            QString(),
            false,
            FunctionExecutionMode::Canvas
        ));
        input.actionId = QStringLiteral("function:canvas-only");

        SelectionContextModelRequest built =
            buildSelectionContextModelRequest(input);
        QVERIFY(!built.valid);
        QCOMPARE(
            built.errorCode,
            QStringLiteral("selection.function_canvas_unsupported")
        );
        QVERIFY(built.modelRequest.modelId.isEmpty());

        input.actionId = QStringLiteral("function:missing");
        built = buildSelectionContextModelRequest(input);
        QVERIFY(!built.valid);
        QCOMPARE(
            built.errorCode,
            QStringLiteral("selection.function_missing")
        );

        input.actionId = QStringLiteral("function:translate");
        built = buildSelectionContextModelRequest(input);
        QVERIFY(!built.valid);
        QCOMPARE(
            built.errorCode,
            QStringLiteral("selection.function_builtin_unsupported")
        );
    }

    void aiSearchExplicitlyDegradesToOrdinaryAiWithoutSources()
    {
        SelectionContextModelRequestInput input = baseInput();
        input.actionId = QStringLiteral("ai-search");

        const SelectionContextModelRequest built =
            buildSelectionContextModelRequest(input);
        QVERIFY(built.valid);
        QVERIFY(built.degraded);
        QCOMPARE(
            built.degradedMessage,
            text8("未进行联网搜索，已使用普通 AI 解答")
        );
        const QString completePrompt = built.modelRequest.systemPrompt
            + QLatin1Char('\n') + built.modelRequest.userPrompt;
        QVERIFY(!completePrompt.contains(text8("来源：")));
        QVERIFY(!completePrompt.contains(text8("已联网")));
        QVERIFY(!completePrompt.contains(text8("搜索结果")));
        QVERIFY(completePrompt.contains(
            text8("没有进行实时网页检索，不得编造来源或时效性事实")
        ));
    }

    void followUpIncludesOriginalSelectionPreviousAnswerAndQuestion()
    {
        SelectionContextModelRequestInput input = baseInput();
        input.actionId = QStringLiteral("explain");
        input.previousAnswer = text8("上一轮回答");
        input.followUpQuestion = text8("为什么？");

        const SelectionContextModelRequest built =
            buildSelectionContextModelRequest(input);

        QVERIFY(built.valid);
        QVERIFY(built.modelRequest.userPrompt.contains(input.selectedText));
        QVERIFY(built.modelRequest.userPrompt.contains(input.previousAnswer));
        QVERIFY(built.modelRequest.userPrompt.contains(input.followUpQuestion));
    }

    void unknownActionReturnsAStableLocalErrorWithoutCallingAProvider()
    {
        SelectionContextModelRequestInput input = baseInput();
        input.actionId = QStringLiteral("unknown-action");

        const SelectionContextModelRequest built =
            buildSelectionContextModelRequest(input);

        QVERIFY(!built.valid);
        QCOMPARE(
            built.errorCode,
            QStringLiteral("selection.action_unsupported")
        );
        QVERIFY(!built.errorMessage.isEmpty());
        QVERIFY(built.modelRequest.modelId.isEmpty());
    }

    void selectedTextNeverAppearsInTheRequestDiagnosticSummary()
    {
        SelectionContextModelRequestInput input = baseInput();
        input.actionId = QStringLiteral("explain");
        input.selectedText = text8("绝不能写入诊断的私密正文");

        const SelectionContextModelRequest built =
            buildSelectionContextModelRequest(input);

        QVERIFY(built.valid);
        QVERIFY(!built.diagnosticSummary.contains(input.selectedText));
        QVERIFY(built.diagnosticSummary.contains(input.actionId));
        QVERIFY(built.diagnosticSummary.contains(
            QString::number(input.selectedText.size())
        ));
    }
};

QTEST_APPLESS_MAIN(SelectionContextModelRequestTests)
#include "selection_context_model_request_tests.moc"
