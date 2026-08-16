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

QString normalizeFloatingBarStyleOverrideValue(const QString &value)
{
    const QString normalized = value.trimmed();
    if (normalized == QStringLiteral("statusPill")
        || normalized == QStringLiteral("liveTranscriptCard")) {
        return normalized;
    }
    return QStringLiteral("inherit");
}

} // namespace

QString functionExecutionModeId(FunctionExecutionMode mode)
{
    return mode == FunctionExecutionMode::Canvas
        ? QStringLiteral("canvas")
        : QStringLiteral("classic");
}

FunctionExecutionMode functionExecutionModeFromId(
    const QString &id,
    bool *known)
{
    const QString normalizedId = id.trimmed();
    if (normalizedId == QStringLiteral("classic")) {
        if (known) {
            *known = true;
        }
        return FunctionExecutionMode::Classic;
    }
    if (normalizedId == QStringLiteral("canvas")) {
        if (known) {
            *known = true;
        }
        return FunctionExecutionMode::Canvas;
    }
    if (known) {
        *known = false;
    }
    return FunctionExecutionMode::Classic;
}

QString functionInputVoiceId()
{
    return QStringLiteral("voice");
}

QString functionInputSelectionId()
{
    return QStringLiteral("selection");
}

QString functionInputScreenshotId()
{
    return QStringLiteral("screenshot");
}

QStringList defaultFunctionInputOrder()
{
    return QStringList()
        << functionInputVoiceId()
        << functionInputSelectionId()
        << functionInputScreenshotId();
}

QStringList normalizeFunctionInputOrder(const QStringList &order)
{
    const QStringList allowed = defaultFunctionInputOrder();
    QStringList normalized;
    for (const QString &rawId : order) {
        const QString id = rawId.trimmed();
        if (allowed.contains(id) && !normalized.contains(id)) {
            normalized.append(id);
        }
    }
    for (const QString &id : allowed) {
        if (!normalized.contains(id)) {
            normalized.append(id);
        }
    }
    return normalized;
}

QString functionOutputAiId()
{
    return QStringLiteral("ai");
}

QString functionOutputAutoWriteId()
{
    return QStringLiteral("autoWrite");
}

QString functionOutputPopupId()
{
    return QStringLiteral("resultPopup");
}

QString functionOutputScreenshotPanelId()
{
    return QStringLiteral("screenshotPanel");
}

QStringList defaultFunctionOutputOrder()
{
    return QStringList()
        << functionOutputAiId()
        << functionOutputAutoWriteId()
        << functionOutputPopupId()
        << functionOutputScreenshotPanelId();
}

QStringList normalizeFunctionOutputOrder(const QStringList &order)
{
    const QStringList allowed = defaultFunctionOutputOrder();
    QStringList normalized;
    for (const QString &rawId : order) {
        const QString id = rawId.trimmed();
        if (allowed.contains(id) && !normalized.contains(id)) {
            normalized.append(id);
        }
    }
    for (const QString &id : allowed) {
        if (!normalized.contains(id)) {
            normalized.append(id);
        }
    }
    return normalized;
}

FunctionSettings normalizeFunctionSettings(
    const FunctionSettings &settings)
{
    FunctionSettings normalized = settings;
    normalized.id = normalized.id.trimmed();
    normalized.name = normalized.name.trimmed();
    normalized.shortcut = normalized.shortcut.trimmed();
    normalized.modelId = normalized.modelId.trimmed();
    normalized.promptId = normalized.promptId.trimmed();
    normalized.sampling =
        normalizeModelSamplingSettings(normalized.sampling);

    normalized.input.order =
        normalizeFunctionInputOrder(normalized.input.order);
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
    normalized.output.order =
        normalizeFunctionOutputOrder(normalized.output.order);
    normalized.output.resultTemplate =
        normalizeResultTemplateValue(
            normalized.output.resultTemplate
        );
    normalized.output.resultActions =
        normalizeResultActionIds(normalized.output.resultActions);
    normalized.output.floatingBarSeconds =
        qBound(0, normalized.output.floatingBarSeconds, 60);
    normalized.output.floatingBarStyleOverride =
        normalizeFloatingBarStyleOverrideValue(
            normalized.output.floatingBarStyleOverride
        );
    normalized.output.resultPopupSeconds =
        qBound(0, normalized.output.resultPopupSeconds, 600);

    normalized.network.speech =
        normalizeNetworkPolicy(normalized.network.speech);
    normalized.network.ocr =
        normalizeNetworkPolicy(normalized.network.ocr);
    normalized.network.model =
        normalizeNetworkPolicy(normalized.network.model);
    normalized.flow.enabled =
        normalized.executionMode == FunctionExecutionMode::Canvas;
    return normalized;
}
