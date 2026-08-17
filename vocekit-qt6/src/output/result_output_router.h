#ifndef VOCEKIT_RESULT_OUTPUT_ROUTER_H
#define VOCEKIT_RESULT_OUTPUT_ROUTER_H

#include <QString>

enum class ResultOutputDestination
{
    AutoWrite,
    ResultPopup,
    ScreenshotPanel
};

struct ResultOutputRouteRequest
{
    QString outputMode;
    bool screenshotInput = false;
    bool hasSelectedText = false;
};

struct ResultOutputPlan
{
    ResultOutputDestination destination = ResultOutputDestination::ResultPopup;
    bool replaceSelectedText = false;
    QString progressTitle;
    QString progressMessage;
    QString doneTitle;
    QString doneMessage;
    QString logAction;
};

class ResultOutputRouter
{
public:
    static ResultOutputPlan plan(
        const ResultOutputRouteRequest &request
    );

    static ResultOutputDestination route(
        const ResultOutputRouteRequest &request
    );
};

#endif
