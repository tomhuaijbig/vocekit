#ifndef VOCEKIT_FUNCTION_FLOW_PORTS_H
#define VOCEKIT_FUNCTION_FLOW_PORTS_H

#include <QString>
#include <QVector>

enum class FunctionFlowNodeType
{
    VoiceSource,
    SelectionSource,
    ScreenshotSource,
    Input,
    Model,
    Output,
    ResultPopup,
    ScreenshotPanel,
    AutoWrite
};

enum class FunctionFlowPortDirection
{
    Input,
    Output
};

enum class FunctionFlowPortCardinality
{
    One,
    Many
};

struct FunctionFlowPortSpec
{
    QString id;
    FunctionFlowPortDirection direction =
        FunctionFlowPortDirection::Input;
    FunctionFlowPortCardinality cardinality =
        FunctionFlowPortCardinality::One;
    bool connectionRequired = false;
};

QString functionFlowNodeTypeId(FunctionFlowNodeType type);
FunctionFlowNodeType functionFlowNodeTypeFromId(
    const QString &id,
    bool *ok = nullptr
);
QVector<FunctionFlowPortSpec> functionFlowPortSpecs(
    FunctionFlowNodeType type
);
bool hasFunctionFlowPort(
    FunctionFlowNodeType type,
    const QString &portId,
    FunctionFlowPortDirection direction
);
bool isFunctionFlowConnectionAllowed(
    FunctionFlowNodeType fromType,
    const QString &fromPortId,
    FunctionFlowNodeType toType,
    const QString &toPortId
);

#endif // VOCEKIT_FUNCTION_FLOW_PORTS_H
