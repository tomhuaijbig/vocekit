#ifndef VOCEKIT_VOICE_INPUT_COLLECTOR_H
#define VOCEKIT_VOICE_INPUT_COLLECTOR_H

#include "selected_text_reader.h"

#include <functional>

struct SelectedTextReadRequest
{
    QString modeId;
    QString sourceLabel;
    bool strongSelectionEnabled = false;
    bool useVoice = false;
    SelectedTextNativeWindowHandle targetWindow = nullptr;
};

struct SelectedTextReadResult
{
    QString text;

    bool hasText() const;
    int characterCount() const;
};

using VocabularyPreCorrectionCallback = std::function<QString(
    const QString &text,
    const QString &modeId,
    const QString &sourceLabel,
    bool hasVoiceInput
)>;

class VoiceInputCollector
{
public:
    static SelectedTextReadResult readSelectedText(
        const SelectedTextReadRequest &request,
        const VocabularyPreCorrectionCallback &preCorrect =
            VocabularyPreCorrectionCallback()
    );

};

#endif // VOCEKIT_VOICE_INPUT_COLLECTOR_H
