#ifndef VOCEKIT_PROMPT_RUNTIME_LIBRARY_H
#define VOCEKIT_PROMPT_RUNTIME_LIBRARY_H

#include "../config/app_settings_data.h"
#include "app_legacy_types.h"

#include <QString>
#include <QVector>

struct PromptTargetInfo
{
    PromptTargetInfo();
    PromptTargetInfo(
        const QString &targetId,
        const QString &targetTitle,
        const QString &targetFileName,
        const QString &targetFallback,
        bool isCustom,
        bool isLibrary = false,
        const QString &targetScope = QString(),
        bool isBuiltIn = false
    );

    QString id;
    QString title;
    QString fileName;
    QString fallback;
    bool custom;
    bool library;
    bool builtIn;
    QString scope;
};

struct PromptRuntimeSnapshot
{
    AppSettingsData settings;
    QVector<PromptLibraryItem> libraryItems;
};

QVector<PromptTargetInfo> promptRuntimeTargets(
    const PromptRuntimeSnapshot &snapshot
);
PromptTargetInfo promptRuntimeTargetForId(
    const PromptRuntimeSnapshot &snapshot,
    const QString &id
);
QString promptRuntimeText(
    const PromptRuntimeSnapshot &snapshot,
    const PromptTargetInfo &target
);
QString promptRuntimeForFunction(
    const PromptRuntimeSnapshot &snapshot,
    const QString &functionId,
    const QString &fallback
);
QString promptRuntimeForVocabulary(
    const PromptRuntimeSnapshot &snapshot
);

#endif // VOCEKIT_PROMPT_RUNTIME_LIBRARY_H
