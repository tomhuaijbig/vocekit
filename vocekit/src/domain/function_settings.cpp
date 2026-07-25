#include "function_settings.h"

#include <QtGlobal>

namespace {

QString normalizeRecordingTriggerMode(const QString &value)
{
    return value.trimmed() == QStringLiteral("hold")
        ? QStringLiteral("hold")
        : QStringLiteral("toggle");
}

QString normalizeScreenshotTriggerMode(const QString &value)
{
    const QString normalized = value.trimmed();
    if (normalized == QStringLiteral("same")
        || normalized == QStringLiteral("launcher")
        || normalized == QStringLiteral("separateAndLauncher")) {
        return normalized;
    }
    return QStringLiteral("separate");
}

QString normalizeOutputModeValue(const QString &value)
{
    const QString normalized = value.trimmed();
    if (normalized == QStringLiteral("autoWrite")
        || normalized == QStringLiteral("screenshotPanel")) {
        return normalized;
    }
    return QStringLiteral("resultPopup");
}

QString normalizeResultTemplateValue(const QString &value)
{
    const QString normalized = value.trimmed();
    if (normalized == QStringLiteral("detail")
        || normalized == QStringLiteral("compare")
        || normalized == QStringLiteral("outputOnly")) {
        return normalized;
    }
    return QStringLiteral("simple");
}

} // namespace

FunctionSettings normalizeFunctionSettings(
    const FunctionSettings &settings)
{
    FunctionSettings normalized = settings;
    normalized.id = normalized.id.trimmed();
    normalized.name = normalized.name.trimmed();
    normalized.shortcut = normalized.shortcut.trimmed();
    normalized.modelId = normalized.modelId.trimmed();
    normalized.promptId = normalized.promptId.trimmed();

    normalized.input.screenshotTriggerMode =
        normalizeScreenshotTriggerMode(
            normalized.input.screenshotTriggerMode
        );
    normalized.input.screenshotShortcut =
        normalized.input.screenshotShortcut.trimmed();

    normalized.recording.triggerMode =
        normalizeRecordingTriggerMode(
            normalized.recording.triggerMode
        );
    normalized.recording.segmentSeconds =
        qBound(20, normalized.recording.segmentSeconds, 55);
    normalized.recording.maximumMinutes =
        qBound(1, normalized.recording.maximumMinutes, 30);
    normalized.recording.countdownSeconds =
        qBound(0, normalized.recording.countdownSeconds, 60);
    normalized.recording.beepPath =
        normalized.recording.beepPath.trimmed();

    normalized.output.outputMode =
        normalizeOutputModeValue(normalized.output.outputMode);
    normalized.output.resultTemplate =
        normalizeResultTemplateValue(
            normalized.output.resultTemplate
        );
    normalized.output.resultActions =
        normalizeResultActionIds(normalized.output.resultActions);
    normalized.output.floatingBarSeconds =
        qBound(0, normalized.output.floatingBarSeconds, 60);
    normalized.output.resultPopupSeconds =
        qBound(0, normalized.output.resultPopupSeconds, 600);

    normalized.network.speech =
        normalizeNetworkPolicy(normalized.network.speech);
    normalized.network.ocr =
        normalizeNetworkPolicy(normalized.network.ocr);
    normalized.network.model =
        normalizeNetworkPolicy(normalized.network.model);
    return normalized;
}
