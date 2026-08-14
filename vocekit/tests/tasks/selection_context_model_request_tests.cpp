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
