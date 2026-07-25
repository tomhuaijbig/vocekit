#include "voice_result_output_dispatcher.h"

namespace {

QString textUtf8(const char *text)
{
    return QString::fromUtf8(text);
}

} // namespace

VoiceResultOutputDispatch VoiceResultOutputDispatcher::plan(
    const VoiceResultOutputRequest &request
)
{
    VoiceResultOutputDispatch dispatch;

    ResultOutputRouteRequest routeRequest;
    routeRequest.outputMode = request.outputMode;
    routeRequest.screenshotInput = request.screenshotInput;
    routeRequest.hasSelectedText = request.hasSelectedText;
    dispatch.routePlan = ResultOutputRouter::plan(routeRequest);

    const QString outputLength = QString::number(request.finalOutput.size());
    dispatch.completionLogDetail =
        textUtf8("\xE5\x8A\x9F\xE8\x83\xBD=") + request.modeId
        + textUtf8("\xEF\xBC\x8C\xE5\xB1\x95\xE7\x8E\xB0=") + request.outputMode
        + textUtf8("\xEF\xBC\x8C\xE8\xBE\x93\xE5\x87\xBA\xE5\xAD\x97\xE6\x95\xB0=") + outputLength;
    dispatch.autoWriteLogDetail =
        textUtf8("\xE5\x8A\x9F\xE8\x83\xBD=") + request.modeId
        + textUtf8("\xEF\xBC\x8C\xE5\xAD\x97\xE6\x95\xB0=") + outputLength;

    return dispatch;
}
