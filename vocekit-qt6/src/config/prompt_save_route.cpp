#include "prompt_save_route.h"

PromptSaveDestination promptSaveDestination(
    const PromptTargetInfo &target)
{
    if (target.custom) {
        return PromptSaveDestination::FunctionSettings;
    }
    if (target.library) {
        return PromptSaveDestination::PromptLibrary;
    }
    return PromptSaveDestination::PromptFile;
}
