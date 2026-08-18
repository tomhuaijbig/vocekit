#include "function_command_page_access_factory.h"

FunctionCommandPageAccess createFunctionCommandPageAccess(
    const FunctionCommandPageAccessDependencies &dependencies
)
{
    FunctionCommandPageAccess access;
    access.settings = dependencies.settings;
    access.prompts = dependencies.prompts;
    access.flows = dependencies.flows;
    access.saveSettings = dependencies.saveSettings;
    access.functionRenamed = dependencies.functionRenamed;
    access.functionRemoved = dependencies.functionRemoved;
    access.operationFailed = dependencies.operationFailed;
    return access;
}
