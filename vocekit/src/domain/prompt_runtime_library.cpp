#include "prompt_runtime_library.h"

#include "../config/app_paths.h"
#include "../file_utils.h"

#include <QDir>

namespace {

QString promptText(const char *text)
{
    return QString::fromUtf8(text);
}

} // namespace

PromptTargetInfo::PromptTargetInfo()
    : custom(false), library(false), builtIn(false)
{
}

PromptTargetInfo::PromptTargetInfo(
    const QString &targetId,
    const QString &targetTitle,
    const QString &targetFileName,
    const QString &targetFallback,
    bool isCustom,
    bool isLibrary,
    const QString &targetScope,
    bool isBuiltIn)
    : id(targetId),
      title(targetTitle),
      fileName(targetFileName),
      fallback(targetFallback),
      custom(isCustom),
      library(isLibrary),
      builtIn(isBuiltIn),
      scope(targetScope)
{
}

QVector<PromptTargetInfo> promptRuntimeTargets(
    const PromptRuntimeSnapshot &snapshot)
{
    QVector<PromptTargetInfo> targets;
    targets.append(PromptTargetInfo(
        QStringLiteral("dictate"),
        promptText("听写提示词"),
        QStringLiteral("asr.txt"),
        promptText("整理语音识别文本，只输出可直接粘贴的结果。"),
        false,
        false,
        promptText("听写"),
        true
    ));
    targets.append(PromptTargetInfo(
        QStringLiteral("translate"),
        promptText("翻译提示词"),
        QStringLiteral("translate.txt"),
        promptText("翻译为简体中文，只输出翻译结果。"),
        false,
        false,
        promptText("翻译"),
        true
    ));
    targets.append(PromptTargetInfo(
        QStringLiteral("ask"),
        promptText("问答提示词"),
        QStringLiteral("qa.txt"),
        promptText("基于选中文本回答用户问题。"),
        false,
        false,
        promptText("问答"),
        true
    ));
    targets.append(PromptTargetInfo(
        QStringLiteral("lexicon"),
        promptText("词库提示词"),
        QStringLiteral("lexicon.txt"),
        promptText(
            "你是词库整理助手。根据用户提供的选中文字、修改前后文本或当前表单草稿生成一个词库词条。"
            "只输出 JSON 对象，不要解释，不要 Markdown。字段必须包含 source、target、aliases、"
            "scopeId、matchMode、note、enabled。只生成一个最有价值的词条；已经填写的 source、target、"
            "aliases、scopeId、matchMode 要优先保留，只补全缺失或明显错误的字段。不要输出 source 和 "
            "target 完全相同且 aliases 为空的无效词条；如果需要保留标准写法，请把常见错写放到 aliases。"
        ),
        false,
        false,
        promptText("词库"),
        true
    ));
    for (const PromptLibraryItem &item : snapshot.libraryItems) {
        targets.append(PromptTargetInfo(
            item.id,
            item.name,
            QString(),
            item.content,
            false,
            true,
            item.scope.trimmed().isEmpty()
                ? promptText("通用")
                : item.scope.trimmed()
        ));
    }
    for (const FunctionSettings &function : snapshot.settings.functions) {
        if (function.builtIn) {
            continue;
        }
        targets.append(PromptTargetInfo(
            function.id,
            function.name + promptText("提示词"),
            QString(),
            function.prompt,
            true,
            false,
            promptText("功能")
        ));
    }
    return targets;
}

PromptTargetInfo promptRuntimeTargetForId(
    const PromptRuntimeSnapshot &snapshot,
    const QString &id)
{
    const QVector<PromptTargetInfo> targets = promptRuntimeTargets(snapshot);
    for (const PromptTargetInfo &target : targets) {
        if (target.id == id) {
            return target;
        }
    }
    return targets.isEmpty() ? PromptTargetInfo() : targets.first();
}

QString promptRuntimeText(
    const PromptRuntimeSnapshot &snapshot,
    const PromptTargetInfo &target)
{
    QString text;
    if (target.custom) {
        text = snapshot.settings.function(target.id).prompt;
    } else if (target.library) {
        for (const PromptLibraryItem &item : snapshot.libraryItems) {
            if (item.id == target.id) {
                text = item.content;
                break;
            }
        }
    } else if (!target.fileName.trimmed().isEmpty()) {
        text = readTextFile(
            QDir(appBasePath()).filePath(
                QStringLiteral("prompts/") + target.fileName
            )
        ).trimmed();
    }
    return text.trimmed().isEmpty() ? target.fallback : text;
}

QString promptRuntimeForFunction(
    const PromptRuntimeSnapshot &snapshot,
    const QString &functionId,
    const QString &fallback)
{
    const FunctionSettings function = snapshot.settings.function(functionId);
    const QString promptId = function.promptId.trimmed().isEmpty()
        ? functionId
        : function.promptId.trimmed();
    const PromptTargetInfo target =
        promptRuntimeTargetForId(snapshot, promptId);
    const QString text = promptRuntimeText(snapshot, target).trimmed();
    return text.isEmpty() ? fallback : text;
}

QString promptRuntimeForVocabulary(
    const PromptRuntimeSnapshot &snapshot)
{
    const PromptTargetInfo target = promptRuntimeTargetForId(
        snapshot,
        QStringLiteral("lexicon")
    );
    return promptRuntimeText(snapshot, target);
}
