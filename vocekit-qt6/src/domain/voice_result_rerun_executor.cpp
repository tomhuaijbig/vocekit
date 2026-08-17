#include "voice_result_rerun_executor.h"

namespace {

QString textUtf8(const char *text)
{
    return QString::fromUtf8(text);
}

QString resolvedModel(
    const QString &overrideModel,
    const QString &defaultModel
)
{
    const QString overrideTrimmed = overrideModel.trimmed();
    return overrideTrimmed.isEmpty() ? defaultModel.trimmed() : overrideTrimmed;
}

QString outputLengthText(const QString &output)
{
    return QString::number(output.size());
}

} // namespace

VoiceResultRerunResult VoiceResultRerunExecutor::run(
    const VoiceResultRerunRequest &request,
    const VoiceResultRerunHandlers &handlers
)
{
    VoiceResultRerunResult result;
    result.finalModel = resolvedModel(
        request.modelOverride,
        request.defaultModel
    );

    if (!handlers.runContext || !handlers.finalizeOutput) {
        result.error = textUtf8(
            "\xE7\xBB\x93\xE6\x9E\x9C\xE9\x87\x8D\xE8\xAF\x95\xE6\x89\xA7\xE8\xA1\x8C\xE5\x99\xA8\xE6\x9C\xAA\xE9\x85\x8D\xE7\xBD\xAE\xE5\xAE\x8C\xE6\x95\xB4\xE3\x80\x82"
        );
        result.logAction = textUtf8("\xE9\x87\x8D\xE8\xAF\x95\xE5\xA4\xB1\xE8\xB4\xA5");
        result.logDetail =
            textUtf8("\xE5\x8A\x9F\xE8\x83\xBD=") + request.context.modeId
            + textUtf8("\xEF\xBC\x8C\xE9\x94\x99\xE8\xAF\xAF=") + result.error;
        return result;
    }

    result.rawOutput = handlers.runContext(
        request.context,
        request.modelOverride,
        request.extraInstruction,
        &result.error,
        request.onDelta
    );

    if (handlers.wasCancelled && handlers.wasCancelled()) {
        result.cancelled = true;
        if (result.error.trimmed().isEmpty()) {
            result.error = textUtf8(
                "\xE8\xAF\xB7\xE6\xB1\x82\xE5\xB7\xB2\xE5\x8F\x96\xE6\xB6\x88\xE3\x80\x82"
            );
        }
        result.logAction = textUtf8(
            "\xE5\xB7\xB2\xE5\x8F\x96\xE6\xB6\x88"
        );
        result.logDetail =
            textUtf8("\xE5\x8A\x9F\xE8\x83\xBD=") + request.context.modeId
            + textUtf8("\xEF\xBC\x8C\xE6\xA8\xA1\xE5\x9E\x8B=")
            + result.finalModel;
        return result;
    }

    if (result.rawOutput.trimmed().isEmpty()) {
        if (result.error.trimmed().isEmpty()) {
            result.error = textUtf8(
                "\xE6\xA8\xA1\xE5\x9E\x8B\xE6\xB2\xA1\xE6\x9C\x89\xE8\xBF\x94\xE5\x9B\x9E\xE7\xBB\x93\xE6\x9E\x9C\xE3\x80\x82"
            );
        }
        result.logAction = textUtf8("\xE9\x87\x8D\xE8\xAF\x95\xE5\xA4\xB1\xE8\xB4\xA5");
        result.logDetail =
            textUtf8("\xE5\x8A\x9F\xE8\x83\xBD=") + request.context.modeId
            + textUtf8("\xEF\xBC\x8C\xE6\xA8\xA1\xE5\x9E\x8B=") + request.modelOverride
            + textUtf8("\xEF\xBC\x8C\xE9\x94\x99\xE8\xAF\xAF=") + result.error;
        return result;
    }

    result.finalOutput = handlers.finalizeOutput(
        request.context,
        result.rawOutput
    );
    result.ok = true;
    result.logAction = textUtf8("\xE9\x87\x8D\xE8\xAF\x95\xE5\xAE\x8C\xE6\x88\x90");
    result.logDetail =
        textUtf8("\xE5\x8A\x9F\xE8\x83\xBD=") + request.context.modeId
        + textUtf8("\xEF\xBC\x8C\xE6\xA8\xA1\xE5\x9E\x8B=") + result.finalModel
        + textUtf8("\xEF\xBC\x8C\xE8\xBE\x93\xE5\x87\xBA\xE5\xAD\x97\xE6\x95\xB0=")
        + outputLengthText(result.finalOutput);
    return result;
}
