#include "vocabulary_diagnostic_task.h"

#include "../config/app_settings_defaults.h"
#include "../file_utils.h"
#include "../storage/vocabulary_store.h"
#include "diagnostic_helpers.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonObject>

namespace {

QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

} // namespace

VocabularyDiagnosticRequest buildVocabularyDiagnosticRequest(
    const AppSettingsData &settings,
    const QString &storePath
)
{
    VocabularyDiagnosticRequest request;
    request.storePath = storePath;
    request.vocabularyEnabled = settings.vocabularyEnabled;
    request.vocabularyAddMode = normalizeVocabularyAddMode(
        settings.vocabularyAddMode
    );
    request.vocabularyOnlyForVoiceInput =
        settings.vocabularyOnlyForVoiceInput;
    request.vocabularyPromptEntryLimit = qBound(
        0,
        settings.vocabularyPromptEntryLimit,
        100
    );
    return request;
}

QStringList runVocabularyDiagnosticTask(const VocabularyDiagnosticRequest &request)
{
    QStringList lines;
    const VocabularyStore store(
        request.storePath.trimmed().isEmpty() ? vocabularyStorePath() : request.storePath
    );
    const QString path = store.path();
    const QFileInfo info(path);
    QJsonObject root;
    const bool jsonReadable = readJsonObjectFile(path, &root);
    const QVector<VocabularyEntry> entries = store.loadEntries();

    int enabledCount = 0;
    int correctionCount = 0;
    VocabularyEntry sampleEntry;
    QString sampleTerm;
    for (const VocabularyEntry &entry : entries) {
        if (entry.enabled) {
            ++enabledCount;
        }
        if (VocabularyStore::entryHasCorrection(entry)) {
            ++correctionCount;
            if (sampleEntry.id.isEmpty() && entry.enabled) {
                const QStringList terms = VocabularyStore::terms(entry);
                for (const QString &term : terms) {
                    if (term.trimmed() != entry.target.trimmed()) {
                        sampleEntry = entry;
                        sampleTerm = term.trimmed();
                        break;
                    }
                }
            }
        }
    }

    lines << diagnosticStatusLine(
        tr8("词库文件"),
        info.exists() ? (jsonReadable ? tr8("可读") : tr8("读取失败")) : tr8("不存在"),
        QDir::toNativeSeparators(path)
    );
    lines << diagnosticStatusLine(
        tr8("词库设置"),
        request.vocabularyEnabled ? tr8("已启用") : tr8("已关闭"),
        tr8("加入方式：") + vocabularyAddModeTitle(request.vocabularyAddMode)
            + tr8("\n  仅语音输入时启用：")
            + (request.vocabularyOnlyForVoiceInput ? tr8("是") : tr8("否"))
            + tr8("\n  大模型注入数量：")
            + QString::number(request.vocabularyPromptEntryLimit)
    );
    lines << diagnosticStatusLine(
        tr8("词条数量"),
        entries.isEmpty() ? tr8("暂无词条") : tr8("已加载"),
        tr8("总数：") + QString::number(entries.size())
            + tr8("\n  已启用：") + QString::number(enabledCount)
            + tr8("\n  有修正效果：") + QString::number(correctionCount)
    );

    if (sampleEntry.id.isEmpty() || sampleTerm.isEmpty()) {
        lines << diagnosticStatusLine(
            tr8("示例替换"),
            tr8("跳过"),
            entries.isEmpty()
                ? tr8("词库里还没有词条。")
                : tr8("没有找到已启用且原词/别名和标准写法不同的词条。")
        );
        return lines;
    }

    const QString modeId = VocabularyStore::normalizeScope(sampleEntry.scopeId)
            == QStringLiteral("__global")
        ? QStringLiteral("dictate")
        : VocabularyStore::normalizeScope(sampleEntry.scopeId);
    const QString sampleText = tr8("词库测试：") + sampleTerm;
    const QString replaced = store.applyEntries(sampleText, modeId, request.vocabularyEnabled);
    const QString promptBlock = store.promptBlock(
        modeId,
        request.vocabularyEnabled,
        sampleText,
        request.vocabularyPromptEntryLimit
    );

    lines << diagnosticStatusLine(
        tr8("示例替换"),
        replaced != sampleText ? tr8("通过") : tr8("未替换"),
        tr8("输入：") + sampleText
            + tr8("\n  输出：") + replaced
            + tr8("\n  作用范围：") + modeId
    );
    lines << diagnosticStatusLine(
        tr8("大模型词库注入"),
        promptBlock.trimmed().isEmpty() ? tr8("未生成") : tr8("可生成"),
        request.vocabularyPromptEntryLimit <= 0
            ? tr8("当前注入数量为 0，不会把词库规则发给大模型。")
            : compactDiagnosticError(promptBlock)
    );
    return lines;
}
