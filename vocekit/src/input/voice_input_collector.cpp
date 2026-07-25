#include "voice_input_collector.h"

bool SelectedTextReadResult::hasText() const
{
    return !text.trimmed().isEmpty();
}

int SelectedTextReadResult::characterCount() const
{
    return text.size();
}

SelectedTextReadResult VoiceInputCollector::readSelectedText(
    const SelectedTextReadRequest &request,
    const VocabularyPreCorrectionCallback &preCorrect
)
{
    SelectedTextReadResult result;
    const QString rawText = SelectedTextReader::read(
        request.strongSelectionEnabled,
        request.targetWindow
    ).trimmed();
    if (rawText.isEmpty()) {
        return result;
    }
    result.text = preCorrect
        ? preCorrect(
            rawText,
            request.modeId,
            request.sourceLabel,
            request.useVoice
        )
        : rawText;
    result.text = result.text.trimmed();
    return result;
}
