#ifndef VOCEKIT_FUNCTION_CANVAS_VISUAL_STYLE_H
#define VOCEKIT_FUNCTION_CANVAS_VISUAL_STYLE_H

#include "../domain/function_flow_ports.h"

#include <QColor>
#include <QString>

struct FunctionFlowNode;
enum class FunctionFlowNodeState;

QString functionCanvasNodeDisplayName(FunctionFlowNodeType type);
QString functionCanvasNodeGlyph(FunctionFlowNodeType type);
QString functionCanvasNodeSummary(const FunctionFlowNode &node);
QColor functionCanvasNodeAccent(FunctionFlowNodeType type);
QColor functionCanvasPortColor(const QString &portId);
QColor functionCanvasRuntimeColor(FunctionFlowNodeState state);
QColor functionCanvasSurfaceColor();
QColor functionCanvasPanelBorderColor();

#endif // VOCEKIT_FUNCTION_CANVAS_VISUAL_STYLE_H
