#include "function_editor_dialog_access_factory.h"

namespace {

void runIfPresent(const std::function<void()> &action)
{
    if (action) {
        action();
    }
}

} // namespace

FunctionEditorDialogAccess createFunctionEditorDialogAccess(
    const FunctionEditorDialogAccessFactoryDependencies &dependencies)
{
    FunctionEditorDialogAccess access;
    access.settings = dependencies.settings;
    access.prompts = dependencies.prompts;
    const std::function<void()> saveSettings = dependencies.saveSettings;
    access.saveSettings = [saveSettings]() {
        runIfPresent(saveSettings);
    };
    return access;
}
