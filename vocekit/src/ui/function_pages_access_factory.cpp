#include "function_pages_access_factory.h"

FunctionPagesAccessAssembly createFunctionPagesAccess(
    const FunctionPagesAccessDependencies &dependencies
)
{
    FunctionCommandPageAccessDependencies commandDependencies;
    commandDependencies.settings = dependencies.settings;
    commandDependencies.prompts = dependencies.prompts;
    commandDependencies.flows = dependencies.flows;
    commandDependencies.saveSettings = dependencies.saveSettings;
    commandDependencies.operationFailed =
        dependencies.operationFailed;

    FunctionManagementPageAccessDependencies managementDependencies;
    managementDependencies.settings = dependencies.settings;
    managementDependencies.summaryProvider = dependencies.summaryProvider;
    managementDependencies.addFunction = dependencies.addFunction;
    managementDependencies.editFunction = dependencies.editFunction;
    managementDependencies.flows = dependencies.flows;
    managementDependencies.operationFailed =
        dependencies.operationFailed;

    FunctionPagesAccessAssembly assembly;
    assembly.command = createFunctionCommandPageAccess(commandDependencies);
    assembly.management = createFunctionManagementPageAccess(
        managementDependencies
    );
    return assembly;
}
