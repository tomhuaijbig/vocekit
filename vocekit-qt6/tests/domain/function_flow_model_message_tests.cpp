#include <QtTest>

#include "../../src/domain/function_flow_model_message.h"

namespace {

FunctionFlowValue value(
    const QString &text,
    const QString &role,
    int sequence,
    const QString &sourceNodeId)
{
    FunctionFlowValue result;
    result.text = text;
    result.role = role;
    result.sequence = sequence;
    result.sourceNodeId = sourceNodeId;
    return result;
}

} // namespace

class FunctionFlowModelMessageTests : public QObject
{
    Q_OBJECT

private slots:
    void buildsRoleBlocks();
    void sortsDuplicateSequencesBySourceNodeId();
    void ignoresEmptyValuesAndUsesDefaultRole();
};

void FunctionFlowModelMessageTests::buildsRoleBlocks()
{
    QList<FunctionFlowValue> values;
    values
        << value(
            QString::fromUtf8("请翻译"),
            QString::fromUtf8("用户要求"),
            0,
            QStringLiteral("instruction"))
        << value(
            QStringLiteral("Hello"),
            QString::fromUtf8("待处理原文"),
            1,
            QStringLiteral("selection"));

    QCOMPARE(
        buildFunctionFlowUserPrompt(values),
        QString::fromUtf8(
            "[用户要求]\n请翻译\n\n"
            "[待处理原文]\nHello"
        )
    );
}

void FunctionFlowModelMessageTests::
sortsDuplicateSequencesBySourceNodeId()
{
    QList<FunctionFlowValue> values;
    values
        << value(
            QStringLiteral("B"),
            QStringLiteral("second"),
            3,
            QStringLiteral("source-b"))
        << value(
            QStringLiteral("A"),
            QStringLiteral("first"),
            3,
            QStringLiteral("source-a"))
        << value(
            QStringLiteral("A2"),
            QStringLiteral("first-again"),
            3,
            QStringLiteral("source-a"));

    QCOMPARE(
        buildFunctionFlowUserPrompt(values),
        QStringLiteral(
            "[first]\nA\n\n"
            "[first-again]\nA2\n\n"
            "[second]\nB"
        )
    );
}

void FunctionFlowModelMessageTests::
ignoresEmptyValuesAndUsesDefaultRole()
{
    QList<FunctionFlowValue> values;
    values
        << value(
            QStringLiteral("  "),
            QStringLiteral("optional"),
            0,
            QStringLiteral("empty"))
        << value(
            QStringLiteral("正文"),
            QStringLiteral(" "),
            1,
            QStringLiteral("content"));

    QCOMPARE(
        buildFunctionFlowUserPrompt(values),
        QString::fromUtf8("[输入]\n正文")
    );
}

QTEST_APPLESS_MAIN(FunctionFlowModelMessageTests)

#include "function_flow_model_message_tests.moc"
