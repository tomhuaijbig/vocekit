#ifndef VOCEKIT_FUNCTION_FLOW_VALIDATION_H
#define VOCEKIT_FUNCTION_FLOW_VALIDATION_H

#include "function_flow_graph.h"

#include <QMap>
#include <QStringList>
#include <QVector>

struct FunctionFlowValidationIssue
{
    QString code;
    QString nodeId;
    QString edgeId;
    QString message;
};

struct FunctionFlowValidationResult
{
    bool ok = false;
    QStringList issueCodes;
    QVector<FunctionFlowValidationIssue> issues;
};

struct FunctionFlowReferenceCatalog
{
    QStringList modelIds;
    QStringList promptIds;
    QStringList speechProviderIds;
    QStringList ocrEngineIds;
    QString defaultSpeechProviderId;
    QString defaultOcrEngineId;
};

struct FunctionFlowValidationContext
{
    FunctionFlowReferenceCatalog references;
    QString functionId;
    QString mainShortcut;
    QMap<QString, QString> occupiedShortcutOwners;
};

class FunctionFlowValidator
{
public:
    static FunctionFlowValidationResult validateForPublish(
        const FunctionFlowGraph &graph,
        const FunctionFlowValidationContext &context
    );
};

#endif // VOCEKIT_FUNCTION_FLOW_VALIDATION_H
