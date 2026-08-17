#ifndef VOCEKIT_FUNCTION_CANVAS_VISUAL_STYLE_TESTS_H
#define VOCEKIT_FUNCTION_CANVAS_VISUAL_STYLE_TESTS_H

#include <QObject>

class FunctionCanvasVisualStyleTests : public QObject
{
    Q_OBJECT

private slots:
    void allNineNodesHaveStableChinesePresentation();
    void summariesAreChineseAndHideInternalIds();
    void summaryBranchesStayUserFacing();
    void inputRoleMatrixMatchesInspectorVocabulary();
    void portAndRuntimeColorsAreDeterministic();
    void surfaceColorsAreStable();
};

#endif // VOCEKIT_FUNCTION_CANVAS_VISUAL_STYLE_TESTS_H
