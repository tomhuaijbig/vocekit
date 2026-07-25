#include "function_command_page_access_factory.h"

FunctionCommandPageAccess createFunctionCommandPageAccess(
    const FunctionCommandPageAccessDependencies &dependencies
)
{
    FunctionCommandPageAccess access;
    access.settings = dependencies.settings;
    access.prompts = dependencies.prompts;
    access.saveSettings = dependencies.saveSettings;

    const std::function<void(
        const QString &,
        const QString &,
        const CustomFunctionDef &
    )> editCustomFunction = dependencies.editCustomFunction;

    access.manageCustomFunction = [editCustomFunction](
        const QString &id,
        const QString &title,
        const CustomFunctionDef &function
    ) {
        if (editCustomFunction) {
            editCustomFunction(id, title, function);
        }
    };
    return access;
}
