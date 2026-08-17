#include "voice_run_context.h"

bool VoiceRunContext::hasSelectedText() const
{
    return !selectedText.trimmed().isEmpty();
}

bool VoiceRunContext::hasVoiceText() const
{
    return !voiceText.trimmed().isEmpty();
}

bool VoiceRunContext::hasTextOnlyInput() const
{
    return !textOnlyInput.trimmed().isEmpty();
}

bool VoiceRunContext::hasScreenshotText() const
{
    return screenshotInput && !screenshotRecognizedText.trimmed().isEmpty();
}
