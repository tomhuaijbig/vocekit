#include "voice_result_popup_builder.h"

#include "../domain/voice_run_formatter.h"

VoiceResultPopupPresentation VoiceResultPopupBuilder::build(
    const VoiceResultPopupBuildRequest &request
)
{
    ResultPopupFormatRequest formatRequest;
    formatRequest.context = request.context;
    formatRequest.output = request.output;
    formatRequest.templateId = request.templateId;
    formatRequest.functionTitle = request.functionTitle;
    formatRequest.modelTitle = request.modelTitle;
    formatRequest.elapsedMs = request.elapsedMs;

    VoiceResultPopupPresentation presentation;
    presentation.title = request.functionTitle;
    presentation.text = VoiceRunFormatter::resultPopupText(formatRequest);
    presentation.currentModel = request.modelId.trimmed();
    presentation.hasSelectedText = request.context.hasSelectedText();
    presentation.timeoutMs = request.timeoutMs;
    return presentation;
}
