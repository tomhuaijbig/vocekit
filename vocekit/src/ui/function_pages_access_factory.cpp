#include "function_pages_access_factory.h"

FunctionPagesAccessAssembly createFunctionPagesAccess(
    const FunctionPagesAccessDependencies &dependencies
)
{
    FunctionCommandPageAccessDependencies commandDependencies;
    commandDependencies.settings = dependencies.settings;
    commandDependencies.prompts = dependencies.prompts;
    commandDependencies.saveSettings = dependencies.saveSettings;

    const auto editFunction = dependencies.editFunction;
    commandDependencies.editCustomFunction = [editFunction](
        const QString &id,
        const QString &title,
        const CustomFunctionDef &function
    ) {
        if (editFunction) {
            editFunction(id, title, true, function);
        }
    };

    FunctionManagementPageAccessDependencies managementDependencies;
    managementDependencies.settings = dependencies.settings;
    managementDependencies.summaryProvider = dependencies.summaryProvider;
    managementDependencies.addFunction = dependencies.addFunction;
    managementDependencies.editFunction = dependencies.editFunction;
    managementDependencies.saveSettings = dependencies.saveSettings;

    FunctionPagesAccessAssembly assembly;
    assembly.command = createFunctionCommandPageAccess(commandDependencies);
    assembly.management = createFunctionManagementPageAccess(
        managementDependencies
    );
    return assembly;
}
