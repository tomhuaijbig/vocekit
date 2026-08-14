#ifndef VOCEKIT_SELECTION_CONTEXT_MODEL_REQUEST_H
#define VOCEKIT_SELECTION_CONTEXT_MODEL_REQUEST_H

#include "model_request_task.h"
#include "../config/app_settings_data.h"
#include "../domain/prompt_runtime_library.h"

struct SelectionContextModelRequestInput
{
    QString actionId;
    QString selectedText;
    QString previousAnswer;
    QString followUpQuestion;
    AppSettingsData settings;
    PromptRuntimeSnapshot prompts;
};

struct SelectionContextModelRequest
{
    bool valid = false;
    bool degraded = false;
    QString degradedMessage;
    QString errorCode;
    QString errorMessage;
    QString diagnosticSummary;
    ModelRequestTaskRequest modelRequest;
};

SelectionContextModelRequest buildSelectionContextModelRequest(
    const SelectionContextModelRequestInput &input
);

#endif // VOCEKIT_SELECTION_CONTEXT_MODEL_REQUEST_H
