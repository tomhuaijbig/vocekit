#include <QtTest>

#include "function_canvas_visual_style_tests.h"

#include "../../src/domain/function_flow_graph.h"
#include "../../src/domain/function_flow_runtime_types.h"
#include "../../src/ui/function_canvas_visual_style.h"

namespace {

bool containsChineseText(const QString &text)
{
    for (const QChar character : text) {
        if (character.unicode() >= 0x4e00
            && character.unicode() <= 0x9fff) {
            return true;
        }
    }
    return false;
}

void verifyNoInternalId(const QString &summary)
{
    const QStringList internalIds = QStringList()
        << QStringLiteral("text_in")
        << QStringLiteral("text_out")
        << QStringLiteral("action_in")
        << QStringLiteral("action_out")
        << QStringLiteral("source")
        << QStringLiteral("insert")
        << QStringLiteral("replace")
        << QStringLiteral("private-model-id")
        << QStringLiteral("private-prompt-id")
        << QStringLiteral("private-ocr-id");
    for (const QString &internalId : internalIds) {
        QVERIFY2(
            !summary.contains(internalId, Qt::CaseInsensitive),
            qPrintable(
                QStringLiteral("摘要暴露内部 ID：%1，摘要：%2")
                    .arg(internalId, summary)
            )
        );
    }
}

} // namespace

void FunctionCanvasVisualStyleTests::
allNineNodesHaveStableChinesePresentation()
{
    struct ExpectedPresentation
    {
        FunctionFlowNodeType type;
        QString name;
        QString glyph;
        QString accent;
    };

    const ExpectedPresentation expected[] = {
        {
            FunctionFlowNodeType::VoiceSource,
            QStringLiteral("语音采集"),
            QStringLiteral("声"),
            QStringLiteral("#2563eb")
        },
        {
            FunctionFlowNodeType::SelectionSource,
            QStringLiteral("选中文字"),
            QStringLiteral("选"),
            QStringLiteral("#0ea5e9")
        },
        {
            FunctionFlowNodeType::ScreenshotSource,
            QStringLiteral("截图识别"),
            QStringLiteral("图"),
            QStringLiteral("#0891b2")
        },
        {
            FunctionFlowNodeType::Input,
            QStringLiteral("输入节点"),
            QStringLiteral("入"),
            QStringLiteral("#7c3aed")
        },
        {
            FunctionFlowNodeType::Model,
            QStringLiteral("调用大模型"),
            QStringLiteral("模"),
            QStringLiteral("#8b5cf6")
        },
        {
            FunctionFlowNodeType::Output,
            QStringLiteral("输出节点"),
            QStringLiteral("出"),
            QStringLiteral("#a855f7")
        },
        {
            FunctionFlowNodeType::ResultPopup,
            QStringLiteral("结果小框"),
            QStringLiteral("显"),
            QStringLiteral("#16a34a")
        },
        {
            FunctionFlowNodeType::ScreenshotPanel,
            QStringLiteral("截图对照窗"),
            QStringLiteral("照"),
            QStringLiteral("#059669")
        },
        {
            FunctionFlowNodeType::AutoWrite,
            QStringLiteral("自动写入"),
            QStringLiteral("写"),
            QStringLiteral("#0d9488")
        }
    };

    for (const ExpectedPresentation &item : expected) {
        const QString name = functionCanvasNodeDisplayName(item.type);
        const QString glyph = functionCanvasNodeGlyph(item.type);
        const QColor accent = functionCanvasNodeAccent(item.type);

        QCOMPARE(name, item.name);
        QCOMPARE(glyph, item.glyph);
        QVERIFY(containsChineseText(name));
        QVERIFY(containsChineseText(glyph));
        QVERIFY(accent.isValid());
        QCOMPARE(accent.name(), item.accent);
        QCOMPARE(functionCanvasNodeDisplayName(item.type), name);
        QCOMPARE(functionCanvasNodeGlyph(item.type), glyph);
        QCOMPARE(functionCanvasNodeAccent(item.type), accent);
    }
}

void FunctionCanvasVisualStyleTests::
summariesAreChineseAndHideInternalIds()
{
    struct ExpectedSummary
    {
        FunctionFlowNodeType type;
        QString text;
    };

    const ExpectedSummary expected[] = {
        {
            FunctionFlowNodeType::VoiceSource,
            QStringLiteral("系统麦克风 · 按键说话")
        },
        {
            FunctionFlowNodeType::SelectionSource,
            QStringLiteral("读取当前选中文字")
        },
        {
            FunctionFlowNodeType::ScreenshotSource,
            QStringLiteral("自动识别 · 简体中文")
        },
        {
            FunctionFlowNodeType::Input,
            QStringLiteral("内容角色：原文 · 必需")
        },
        {
            FunctionFlowNodeType::Model,
            QStringLiteral("等待全部输入")
        },
        {
            FunctionFlowNodeType::Output,
            QStringLiteral("整理最终结果")
        },
        {
            FunctionFlowNodeType::ResultPopup,
            QStringLiteral("显示结果 · 手动关闭")
        },
        {
            FunctionFlowNodeType::ScreenshotPanel,
            QStringLiteral("显示截图与识别结果")
        },
        {
            FunctionFlowNodeType::AutoWrite,
            QStringLiteral("插入到当前光标位置")
        }
    };

    for (const ExpectedSummary &item : expected) {
        FunctionFlowNode node;
        node.type = item.type;
        node.config.voice.speechProviderId =
            QStringLiteral("text_in");
        node.config.screenshot.ocrEngineId =
            QStringLiteral("private-ocr-id-action_in");
        node.config.model.modelId =
            QStringLiteral("private-model-id-text_out");
        node.config.model.promptId =
            QStringLiteral("private-prompt-id-action_out");
        node.config.popup.resultTemplate =
            QStringLiteral("replace");

        const QString summary = functionCanvasNodeSummary(node);
        QCOMPARE(summary, item.text);
        QVERIFY(containsChineseText(summary));
        verifyNoInternalId(summary);
        QCOMPARE(functionCanvasNodeSummary(node), summary);
    }

    FunctionFlowNode replaceNode;
    replaceNode.type = FunctionFlowNodeType::AutoWrite;
    replaceNode.config.autoWrite.writeMode =
        QStringLiteral("replace");
    const QString replaceSummary =
        functionCanvasNodeSummary(replaceNode);
    QCOMPARE(
        replaceSummary,
        QStringLiteral("替换当前选中文字")
    );
    verifyNoInternalId(replaceSummary);
}

void FunctionCanvasVisualStyleTests::
summaryBranchesStayUserFacing()
{
    FunctionFlowNode longVoiceNode;
    longVoiceNode.type = FunctionFlowNodeType::VoiceSource;
    longVoiceNode.config.voice.recording.longRecordingEnabled =
        true;
    QCOMPARE(
        functionCanvasNodeSummary(longVoiceNode),
        QStringLiteral("系统麦克风 · 长语音")
    );

    FunctionFlowNode holdVoiceNode;
    holdVoiceNode.type = FunctionFlowNodeType::VoiceSource;
    holdVoiceNode.config.voice.recording.triggerMode =
        QStringLiteral("hold");
    QCOMPARE(
        functionCanvasNodeSummary(holdVoiceNode),
        QStringLiteral("系统麦克风 · 按住说话")
    );

    FunctionFlowNode optionalInputNode;
    optionalInputNode.type = FunctionFlowNodeType::Input;
    optionalInputNode.config.input.required = false;
    QCOMPARE(
        functionCanvasNodeSummary(optionalInputNode),
        QStringLiteral("内容角色：原文 · 可选")
    );

    FunctionFlowNode timedPopupNode;
    timedPopupNode.type = FunctionFlowNodeType::ResultPopup;
    timedPopupNode.config.popup.displaySeconds = 5;
    QCOMPARE(
        functionCanvasNodeSummary(timedPopupNode),
        QStringLiteral("显示结果 · 5 秒后关闭")
    );

    FunctionFlowNode timedScreenshotPanelNode;
    timedScreenshotPanelNode.type =
        FunctionFlowNodeType::ScreenshotPanel;
    timedScreenshotPanelNode.config.screenshotPanel.displaySeconds =
        8;
    QCOMPARE(
        functionCanvasNodeSummary(timedScreenshotPanelNode),
        QStringLiteral("显示截图与识别结果 · 8 秒")
    );

    FunctionFlowNode unknownWriteModeNode;
    unknownWriteModeNode.type = FunctionFlowNodeType::AutoWrite;
    unknownWriteModeNode.config.autoWrite.writeMode =
        QStringLiteral("append-internal");
    QCOMPARE(
        functionCanvasNodeSummary(unknownWriteModeNode),
        QStringLiteral("写入当前窗口")
    );

    const FunctionFlowNode nodes[] = {
        longVoiceNode,
        holdVoiceNode,
        optionalInputNode,
        timedPopupNode,
        timedScreenshotPanelNode,
        unknownWriteModeNode
    };
    for (const FunctionFlowNode &node : nodes) {
        const QString summary = functionCanvasNodeSummary(node);
        QVERIFY(containsChineseText(summary));
        verifyNoInternalId(summary);
    }
}

void FunctionCanvasVisualStyleTests::
inputRoleMatrixMatchesInspectorVocabulary()
{
    struct ExpectedRole
    {
        QString id;
        QString displayName;
    };
    const ExpectedRole expected[] = {
        {QStringLiteral("source"), QStringLiteral("原文")},
        {QStringLiteral("instruction"), QStringLiteral("指令")},
        {QStringLiteral("screenshot"), QStringLiteral("截图文字")},
        {QStringLiteral("system"), QStringLiteral("系统消息")},
        {QStringLiteral("user"), QStringLiteral("自定义")},
        {QStringLiteral("assistant"), QStringLiteral("自定义")},
        {QStringLiteral("custom-role"), QStringLiteral("自定义")}
    };

    for (const ExpectedRole &item : expected) {
        FunctionFlowNode node;
        node.type = FunctionFlowNodeType::Input;
        node.config.input.role = item.id;

        const QString summary = functionCanvasNodeSummary(node);
        QCOMPARE(
            summary,
            QStringLiteral("内容角色：%1 · 必需")
                .arg(item.displayName)
        );
        QVERIFY(containsChineseText(summary));
        QVERIFY(!summary.contains(item.id, Qt::CaseInsensitive));
        verifyNoInternalId(summary);
    }
}

void FunctionCanvasVisualStyleTests::
portAndRuntimeColorsAreDeterministic()
{
    const QColor textColor(QStringLiteral("#0891b2"));
    const QColor actionColor(QStringLiteral("#7c3aed"));
    const QColor unknownColor(QStringLiteral("#64748b"));

    QCOMPARE(
        functionCanvasPortColor(QStringLiteral("text_in")),
        textColor
    );
    QCOMPARE(
        functionCanvasPortColor(QStringLiteral("text_out")),
        textColor
    );
    QCOMPARE(
        functionCanvasPortColor(QStringLiteral("action_in")),
        actionColor
    );
    QCOMPARE(
        functionCanvasPortColor(QStringLiteral("action_out")),
        actionColor
    );
    QCOMPARE(
        functionCanvasPortColor(QStringLiteral("unknown")),
        unknownColor
    );

    struct ExpectedRuntimeColor
    {
        FunctionFlowNodeState state;
        QString color;
    };
    const ExpectedRuntimeColor runtimeColors[] = {
        {FunctionFlowNodeState::Pending, QStringLiteral("#94a3b8")},
        {FunctionFlowNodeState::Ready, QStringLiteral("#64748b")},
        {FunctionFlowNodeState::Running, QStringLiteral("#2563eb")},
        {FunctionFlowNodeState::Cancelling, QStringLiteral("#f59e0b")},
        {FunctionFlowNodeState::Succeeded, QStringLiteral("#16a34a")},
        {FunctionFlowNodeState::Skipped, QStringLiteral("#64748b")},
        {FunctionFlowNodeState::Failed, QStringLiteral("#dc2626")},
        {FunctionFlowNodeState::Blocked, QStringLiteral("#ea580c")},
        {FunctionFlowNodeState::Cancelled, QStringLiteral("#64748b")}
    };

    for (const ExpectedRuntimeColor &item : runtimeColors) {
        const QColor color = functionCanvasRuntimeColor(item.state);
        QVERIFY(color.isValid());
        QCOMPARE(color.name(), item.color);
        QCOMPARE(functionCanvasRuntimeColor(item.state), color);
    }
}

void FunctionCanvasVisualStyleTests::surfaceColorsAreStable()
{
    const QColor surface = functionCanvasSurfaceColor();
    const QColor panelBorder = functionCanvasPanelBorderColor();

    QVERIFY(surface.isValid());
    QVERIFY(panelBorder.isValid());
    QCOMPARE(surface.name(), QStringLiteral("#f8fafc"));
    QCOMPARE(panelBorder.name(), QStringLiteral("#d8dee8"));
    QCOMPARE(functionCanvasSurfaceColor(), surface);
    QCOMPARE(functionCanvasPanelBorderColor(), panelBorder);
    QVERIFY(surface != panelBorder);
}

QTEST_APPLESS_MAIN(FunctionCanvasVisualStyleTests)
