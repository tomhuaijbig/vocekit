#include "prompt_settings_adapter.h"

#include "../config/app_paths.h"
#include "../config/prompt_save_route.h"
#include "../file_utils.h"

#include <QDir>

namespace {

QString promptText(const char *text)
{
    return QString::fromUtf8(text);
}

PromptRuntimeSnapshot runtimeSnapshot(const PromptSettingsAccess &access)
{
    return access.snapshotProvider
        ? access.snapshotProvider()
        : PromptRuntimeSnapshot();
}

bool saveFunctionPrompt(
    const PromptSettingsAccess &access,
    const PromptTargetInfo &target,
    const QString &text,
    QString *error)
{
    if (!access.saveFunctionPrompt) {
        if (error) {
            *error = promptText("功能提示词保存接口不可用。");
        }
        return false;
    }
    return access.saveFunctionPrompt(target.id, text, error);
}

bool saveLibraryPrompt(
    const PromptSettingsAccess &access,
    const PromptTargetInfo &target,
    const QString &text,
    QString *error)
{
    if (!access.saveLibraryPrompt) {
        if (error) {
            *error = promptText("提示词库保存接口不可用。");
        }
        return false;
    }
    return access.saveLibraryPrompt(target.id, text, error);
}

bool saveBuiltInPrompt(
    const PromptTargetInfo &target,
    const QString &text,
    QString *error)
{
    const QString path = QDir(appBasePath()).filePath(
        QStringLiteral("prompts/") + target.fileName
    );
    if (!writeTextFile(path, text)) {
        if (error) {
            *error = promptText("无法写入提示词文件。");
        }
        return false;
    }
    return true;
}

} // namespace

QVector<PromptTargetInfo> sharedPromptTargets(
    const PromptSettingsAccess &access)
{
    return promptRuntimeTargets(runtimeSnapshot(access));
}

PromptTargetInfo sharedPromptTargetForId(
    const PromptSettingsAccess &access,
    const QString &id)
{
    return promptRuntimeTargetForId(runtimeSnapshot(access), id);
}

QString sharedPromptText(
    const PromptSettingsAccess &access,
    const PromptTargetInfo &target)
{
    return promptRuntimeText(runtimeSnapshot(access), target);
}

bool saveSharedPromptText(
    const PromptSettingsAccess &access,
    const PromptTargetInfo &target,
    const QString &text,
    QString *error)
{
    switch (promptSaveDestination(target)) {
    case PromptSaveDestination::FunctionSettings:
        return saveFunctionPrompt(access, target, text, error);
    case PromptSaveDestination::PromptLibrary:
        return saveLibraryPrompt(access, target, text, error);
    case PromptSaveDestination::PromptFile:
        return saveBuiltInPrompt(target, text, error);
    }
    return false;
}
