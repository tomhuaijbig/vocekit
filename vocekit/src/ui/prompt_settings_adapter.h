#ifndef VOCEKIT_PROMPT_SETTINGS_ADAPTER_H
#define VOCEKIT_PROMPT_SETTINGS_ADAPTER_H

#include "../domain/prompt_runtime_library.h"

#include <QString>
#include <QVector>

#include <functional>

struct PromptSettingsAccess
{
    std::function<PromptRuntimeSnapshot()> snapshotProvider;
    std::function<bool(const QString &, const QString &, QString *)>
        saveFunctionPrompt;
    std::function<bool(const QString &, const QString &, QString *)>
        saveLibraryPrompt;
};

// 提示词界面的存储适配层，只负责读取可编辑目标和分发保存请求。
QVector<PromptTargetInfo> sharedPromptTargets(
    const PromptSettingsAccess &access
);
PromptTargetInfo sharedPromptTargetForId(
    const PromptSettingsAccess &access,
    const QString &id
);
QString sharedPromptText(
    const PromptSettingsAccess &access,
    const PromptTargetInfo &target
);
bool saveSharedPromptText(
    const PromptSettingsAccess &access,
    const PromptTargetInfo &target,
    const QString &text,
    QString *error
);

#endif // VOCEKIT_PROMPT_SETTINGS_ADAPTER_H
