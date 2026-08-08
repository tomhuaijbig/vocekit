#include "function_flow_ports.h"

namespace {

FunctionFlowPortSpec port(
    const QString &id,
    FunctionFlowPortDirection direction,
    FunctionFlowPortCardinality cardinality,
    bool connectionRequired)
{
    FunctionFlowPortSpec spec;
    spec.id = id;
    spec.direction = direction;
    spec.cardinality = cardinality;
    spec.connectionRequired = connectionRequired;
    return spec;
}

bool isSourceType(FunctionFlowNodeType type)
{
    return type == FunctionFlowNodeType::VoiceSource
        || type == FunctionFlowNodeType::SelectionSource
        || type == FunctionFlowNodeType::ScreenshotSource;
}

bool isActionType(FunctionFlowNodeType type)
{
    return type == FunctionFlowNodeType::ResultPopup
        || type == FunctionFlowNodeType::ScreenshotPanel
        || type == FunctionFlowNodeType::AutoWrite;
}

} // namespace

QString functionFlowNodeTypeId(FunctionFlowNodeType type)
{
    switch (type) {
    case FunctionFlowNodeType::VoiceSource:
        return QStringLiteral("voiceSource");
    case FunctionFlowNodeType::SelectionSource:
        return QStringLiteral("selectionSource");
    case FunctionFlowNodeType::ScreenshotSource:
        return QStringLiteral("screenshotSource");
    case FunctionFlowNodeType::Input:
        return QStringLiteral("input");
    case FunctionFlowNodeType::Model:
        return QStringLiteral("model");
    case FunctionFlowNodeType::Output:
        return QStringLiteral("output");
    case FunctionFlowNodeType::ResultPopup:
        return QStringLiteral("resultPopup");
    case FunctionFlowNodeType::ScreenshotPanel:
        return QStringLiteral("screenshotPanel");
    case FunctionFlowNodeType::AutoWrite:
        return QStringLiteral("autoWrite");
    }
    return QString();
}

FunctionFlowNodeType functionFlowNodeTypeFromId(
    const QString &id,
    bool *ok)
{
    const QString normalized = id.trimmed();
    const FunctionFlowNodeType types[] = {
        FunctionFlowNodeType::VoiceSource,
        FunctionFlowNodeType::SelectionSource,
        FunctionFlowNodeType::ScreenshotSource,
        FunctionFlowNodeType::Input,
        FunctionFlowNodeType::Model,
        FunctionFlowNodeType::Output,
        FunctionFlowNodeType::ResultPopup,
        FunctionFlowNodeType::ScreenshotPanel,
        FunctionFlowNodeType::AutoWrite
    };
    for (FunctionFlowNodeType type : types) {
        if (functionFlowNodeTypeId(type) == normalized) {
            if (ok) {
                *ok = true;
            }
            return type;
        }
    }
    if (ok) {
        *ok = false;
    }
    return FunctionFlowNodeType::Input;
}

QVector<FunctionFlowPortSpec> functionFlowPortSpecs(
    FunctionFlowNodeType type)
{
    const FunctionFlowPortDirection input =
        FunctionFlowPortDirection::Input;
    const FunctionFlowPortDirection output =
        FunctionFlowPortDirection::Output;
    const FunctionFlowPortCardinality one =
        FunctionFlowPortCardinality::One;
    const FunctionFlowPortCardinality many =
        FunctionFlowPortCardinality::Many;

    switch (type) {
    case FunctionFlowNodeType::VoiceSource:
    case FunctionFlowNodeType::SelectionSource:
    case FunctionFlowNodeType::ScreenshotSource:
        return QVector<FunctionFlowPortSpec>()
            << port(QStringLiteral("text_out"), output, many, false);
    case FunctionFlowNodeType::Input:
        return QVector<FunctionFlowPortSpec>()
            << port(QStringLiteral("text_in"), input, many, true)
            << port(QStringLiteral("text_out"), output, many, false);
    case FunctionFlowNodeType::Model:
        return QVector<FunctionFlowPortSpec>()
            << port(QStringLiteral("text_in"), input, many, true)
            << port(QStringLiteral("text_out"), output, many, false);
    case FunctionFlowNodeType::Output:
        return QVector<FunctionFlowPortSpec>()
            << port(QStringLiteral("text_in"), input, one, true)
            << port(QStringLiteral("action_out"), output, many, true);
    case FunctionFlowNodeType::ResultPopup:
    case FunctionFlowNodeType::ScreenshotPanel:
    case FunctionFlowNodeType::AutoWrite:
        return QVector<FunctionFlowPortSpec>()
            << port(QStringLiteral("action_in"), input, one, true);
    }
    return QVector<FunctionFlowPortSpec>();
}

bool hasFunctionFlowPort(
    FunctionFlowNodeType type,
    const QString &portId,
    FunctionFlowPortDirection direction)
{
    const QVector<FunctionFlowPortSpec> ports =
        functionFlowPortSpecs(type);
    for (const FunctionFlowPortSpec &spec : ports) {
        if (spec.id == portId && spec.direction == direction) {
            return true;
        }
    }
    return false;
}

bool isFunctionFlowConnectionAllowed(
    FunctionFlowNodeType fromType,
    const QString &fromPortId,
    FunctionFlowNodeType toType,
    const QString &toPortId)
{
    if (!hasFunctionFlowPort(
            fromType,
            fromPortId,
            FunctionFlowPortDirection::Output)
        || !hasFunctionFlowPort(
            toType,
            toPortId,
            FunctionFlowPortDirection::Input)) {
        return false;
    }

    if (isSourceType(fromType)) {
        return fromPortId == QStringLiteral("text_out")
            && toType == FunctionFlowNodeType::Input
            && toPortId == QStringLiteral("text_in");
    }
    if (fromType == FunctionFlowNodeType::Input) {
        return fromPortId == QStringLiteral("text_out")
            && (toType == FunctionFlowNodeType::Model
                || toType == FunctionFlowNodeType::Output)
            && toPortId == QStringLiteral("text_in");
    }
    if (fromType == FunctionFlowNodeType::Model) {
        return fromPortId == QStringLiteral("text_out")
            && (toType == FunctionFlowNodeType::Input
                || toType == FunctionFlowNodeType::Output)
            && toPortId == QStringLiteral("text_in");
    }
    return fromType == FunctionFlowNodeType::Output
        && fromPortId == QStringLiteral("action_out")
        && isActionType(toType)
        && toPortId == QStringLiteral("action_in");
}
