#include "prompt_access_factory.h"

#include "hub_settings_state.h"

namespace {

QString text8(const char *text)
{
    return QString::fromUtf8(text);
}

void setError(QString *error, const char *message)
{
    if (error) {
        *error = text8(message);
    }
}

PromptSettingsAccess createSettingsAccess(HubSettingsState *settings)
{
    PromptSettingsAccess access;
    access.snapshotProvider = [settings]() {
        PromptRuntimeSnapshot snapshot;
        if (settings) {
            snapshot.settings = settings->toData();
            snapshot.libraryItems = settings->promptLibrary();
        }
        return snapshot;
    };
    access.saveFunctionPrompt = [settings](
        const QString &id,
        const QString &text,
        QString *error
    ) {
        if (!settings) {
            setError(error, "设置对象不可用。");
            return false;
        }
        for (const CustomFunctionDef &existing : settings->customFunctions()) {
            if (existing.id != id) {
                continue;
            }
            CustomFunctionDef function = existing;
            function.prompt = text;
            settings->updateCustomFunction(function);
            const bool saved = settings->save();
            if (!saved) {
                setError(error, "无法写入 config/settings.json。");
            }
            return saved;
        }
        setError(error, "没有找到对应的自定义功能。");
        return false;
    };
    access.saveLibraryPrompt = [settings](
        const QString &id,
        const QString &text,
        QString *error
    ) {
        if (!settings) {
            setError(error, "设置对象不可用。");
            return false;
        }
        PromptLibraryItem item = settings->promptLibraryItem(id);
        if (item.id.trimmed().isEmpty()) {
            setError(error, "没有找到对应的提示词。");
            return false;
        }
        item.content = text;
        settings->updatePromptLibraryItem(item);
        if (!settings->savePromptLibrary()) {
            setError(error, "无法写入 config/prompts.json。");
            return false;
        }
        return true;
    };
    return access;
}

PromptsPanelAccess createPanelAccess(
    HubSettingsState *settings,
    const PromptSettingsAccess &promptSettings
)
{
    PromptsPanelAccess access;
    access.prompts = promptSettings;
    access.setPromptLocked = [settings](bool locked, QString *error) {
        if (!settings) {
            setError(error, "设置对象不可用。");
            return false;
        }
        const bool previous = settings->promptLocked();
        settings->setPromptLocked(locked);
        if (settings->save()) {
            return true;
        }
        settings->setPromptLocked(previous);
        setError(error, "无法写入 config/settings.json。");
        return false;
    };
    access.saveLibraryPromptItem = [settings](
        const PromptLibraryItem &item,
        QString *error
    ) {
        if (!settings || !settings->updatePromptLibraryItem(item)) {
            setError(error, "没有找到对应的提示词。");
            return false;
        }
        if (settings->savePromptLibrary()) {
            return true;
        }
        setError(error, "无法写入 config/prompts.json。");
        return false;
    };
    access.createLibraryPromptItem = [settings](
        PromptLibraryItem *item,
        QString *error
    ) {
        if (!settings || !item) {
            setError(error, "设置对象不可用。");
            return false;
        }
        item->id = settings->nextPromptLibraryId();
        if (item->name == text8("新提示词")) {
            item->name += QStringLiteral(" ") + item->id.mid(7);
        }
        settings->addPromptLibraryItem(*item);
        if (settings->savePromptLibrary()) {
            return true;
        }
        setError(error, "无法写入 config/prompts.json。");
        return false;
    };
    access.deleteLibraryPromptItem = [settings](
        const QString &id,
        QString *error
    ) {
        if (!settings || !settings->removePromptLibraryItem(id)) {
            setError(error, "没有找到要删除的提示词。");
            return false;
        }
        if (settings->savePromptLibrary() && settings->save()) {
            return true;
        }
        setError(error, "无法保存提示词删除结果。");
        return false;
    };
    return access;
}

} // namespace

PromptAccessAssembly createPromptAccessAssembly(
    const PromptAccessFactoryDependencies &dependencies)
{
    PromptAccessAssembly assembly;
    assembly.settings = createSettingsAccess(dependencies.settings);
    assembly.panel = createPanelAccess(dependencies.settings, assembly.settings);
    return assembly;
}
