#ifndef VOCEKIT_PROMPT_SAVE_ROUTE_H
#define VOCEKIT_PROMPT_SAVE_ROUTE_H

#include "../domain/prompt_runtime_library.h"

enum class PromptSaveDestination
{
    FunctionSettings,
    PromptLibrary,
    PromptFile
};

PromptSaveDestination promptSaveDestination(
    const PromptTargetInfo &target
);

#endif // VOCEKIT_PROMPT_SAVE_ROUTE_H
