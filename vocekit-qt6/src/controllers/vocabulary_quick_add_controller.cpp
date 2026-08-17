#include "vocabulary_quick_add_controller.h"

#include "../config/app_settings_defaults.h"

namespace {

QString quickAddText(const char *text)
{
    return QString::fromUtf8(text);
}

QString normalizedScope(const QString &scopeId)
{
    const QString scope = scopeId.trimmed();
    return scope.isEmpty() ? QStringLiteral("__global") : scope;
}

} // namespace

VocabularyQuickAddController::VocabularyQuickAddController(
    const VocabularyQuickAddAccess &access,
    QObject *parent
)
    : QObject(parent),
      m_access(access)
{
}

void VocabularyQuickAddController::updateConfiguration(
    const AppSettingsData &settings
)
{
    m_settings = settings;
}

VocabularyQuickAddOutcome VocabularyQuickAddController::handleHotkey(
    SelectedTextNativeWindowHandle targetWindow,
    bool recordingBusy
)
{
    if (recordingBusy) {
        if (m_access.setStatus) {
            m_access.setStatus(
                quickAddText("正在录音"),
                quickAddText("请先结束当前录音，再把选中文字加入词库。")
            );
        }
        if (m_access.hideStatusLater) {
            m_access.hideStatusLater();
        }
        return VocabularyQuickAddOutcome::Busy;
    }

    if (m_access.prepareStatus) {
        m_access.prepareStatus(m_settings.floatingBarEnabled, 5000);
    }
    if (m_access.setStatus) {
        m_access.setStatus(
            quickAddText("正在读取选中文字"),
            quickAddText("读取后会按词库页设置加入词库")
        );
    }

    const QString selectedText = m_access.readSelectedText
        ? m_access.readSelectedText(
            m_settings.strongSelectionEnabled,
            targetWindow
        ).trimmed()
        : QString();
    if (!selectedText.isEmpty()) {
        return addText(selectedText, QStringLiteral("__global"));
    }

    const QString title = quickAddText("未识别到有选中文字");
    const QString message = quickAddText(
        "未识别到有选中文字。请先用鼠标左键拖动选中文字，"
        "再按加入词库快捷键。"
    );
    if (m_access.setStatus) {
        m_access.setStatus(title, message);
    }
    if (m_access.hideStatusLater) {
        m_access.hideStatusLater();
    }
    if (m_access.showInformation) {
        m_access.showInformation(title, message);
    }
    return VocabularyQuickAddOutcome::MissingSelection;
}

VocabularyQuickAddOutcome VocabularyQuickAddController::addText(
    const QString &sourceText,
    const QString &scopeId,
    const QString &editedText
)
{
    const QString source = sourceText.trimmed();
    if (source.isEmpty()) {
        if (m_access.showWarning) {
            m_access.showWarning(
                quickAddText("无法加入词库"),
                quickAddText("没有可用的原词或错词。")
            );
        }
        return VocabularyQuickAddOutcome::Failed;
    }

    const VocabularyQuickAddChoice choice = choiceFromSettings();
    if (choice == VocabularyQuickAddChoice::Cancel) {
        if (m_access.setStatus) {
            m_access.setStatus(
                quickAddText("已取消"),
                quickAddText("没有加入词库。")
            );
        }
        if (m_access.hideStatusLater) {
            m_access.hideStatusLater();
        }
        return VocabularyQuickAddOutcome::Cancelled;
    }

    if (choice == VocabularyQuickAddChoice::Manual) {
        if (m_access.openEditor) {
            m_access.openEditor(manualEntry(source, scopeId, editedText));
        }
        if (m_access.setStatus) {
            m_access.setStatus(
                quickAddText("手动加入词库"),
                quickAddText("请在弹出的词条窗口中确认并保存。")
            );
        }
        if (m_access.hideStatusLater) {
            m_access.hideStatusLater();
        }
        return VocabularyQuickAddOutcome::EditorOpened;
    }

    if (m_access.setStatus) {
        m_access.setStatus(
            quickAddText("正在生成词条"),
            quickAddText("正在调用 AI 分析选中文字")
        );
    }
    if (m_access.flushUi) {
        m_access.flushUi();
    }

    QString error;
    const VocabularySuggestion suggestion = suggest(
        source,
        scopeId,
        &error,
        editedText
    );
    if (!suggestion.valid) {
        if (m_access.showWarning) {
            m_access.showWarning(
                quickAddText("词库 AI 生成失败"),
                error.trimmed().isEmpty()
                    ? quickAddText(
                        "AI 没有返回可用词条，已切换为手动填写。"
                    )
                    : error
            );
        }
        if (m_access.openEditor) {
            m_access.openEditor(
                manualEntry(source, scopeId, editedText)
            );
        }
        if (m_access.hideStatusLater) {
            m_access.hideStatusLater();
        }
        return VocabularyQuickAddOutcome::EditorOpened;
    }

    VocabularyEntry entry = suggestion.entry;
    const bool saved = m_access.appendEntry
        && m_access.appendEntry(&entry, &error);
    if (!saved) {
        if (m_access.showWarning) {
            m_access.showWarning(
                quickAddText("保存词条失败"),
                error.trimmed().isEmpty()
                    ? quickAddText("无法写入词库文件。")
                    : error
            );
        }
        if (m_access.openEditor) {
            m_access.openEditor(entry);
        }
        if (m_access.hideStatusLater) {
            m_access.hideStatusLater();
        }
        return m_access.openEditor
            ? VocabularyQuickAddOutcome::EditorOpened
            : VocabularyQuickAddOutcome::Failed;
    }

    if (m_access.notifyVocabularyChanged) {
        m_access.notifyVocabularyChanged();
    }
    const QString result = entry.source
        + quickAddText(" -> ")
        + entry.target;
    if (m_access.setStatus) {
        m_access.setStatus(quickAddText("已加入词库"), result);
    }
    if (m_access.hideStatusLater) {
        m_access.hideStatusLater();
    }
    if (m_access.showInformation) {
        m_access.showInformation(quickAddText("已加入词库"), result);
    }
    return VocabularyQuickAddOutcome::Saved;
}

VocabularyQuickAddOutcome VocabularyQuickAddController::addTextLocally(
    const QString &sourceText,
    const QString &scopeId,
    const QString &editedText)
{
    const QString source = sourceText.trimmed();
    if (source.isEmpty()) {
        if (m_access.showWarning) {
            m_access.showWarning(
                quickAddText("无法加入词库"),
                quickAddText("没有可用的原词或错词。")
            );
        }
        return VocabularyQuickAddOutcome::Failed;
    }
    if (!m_access.openEditor) {
        return VocabularyQuickAddOutcome::Failed;
    }

    m_access.openEditor(manualEntry(source, scopeId, editedText));
    if (m_access.setStatus) {
        m_access.setStatus(
            quickAddText("手动加入词库"),
            quickAddText("请在弹出的词条窗口中确认并保存。")
        );
    }
    if (m_access.hideStatusLater) {
        m_access.hideStatusLater();
    }
    return VocabularyQuickAddOutcome::EditorOpened;
}

VocabularySuggestion VocabularyQuickAddController::suggest(
    const QString &sourceText,
    const QString &scopeId,
    QString *error,
    const QString &editedText,
    const QString &extraContext
) const
{
    if (!m_access.requestSuggestion) {
        if (error) {
            *error = quickAddText("词库 AI 服务尚未连接。");
        }
        return VocabularySuggestion();
    }

    VocabularySuggestionTaskRequest request;
    request.input.sourceText = sourceText;
    request.input.scopeId = scopeId;
    request.input.editedText = editedText;
    request.input.extraContext = extraContext;
    request.systemPrompt = m_access.vocabularyPrompt
        ? m_access.vocabularyPrompt()
        : QString();
    request.useSystemProxy = m_settings.useSystemProxy;
    return m_access.requestSuggestion(request, error);
}

VocabularyQuickAddChoice
VocabularyQuickAddController::choiceFromSettings() const
{
    const QString mode = normalizeVocabularyAddMode(
        m_settings.vocabularyAddMode
    );
    if (mode == vocabularyAddModeAi()) {
        return VocabularyQuickAddChoice::UseAi;
    }
    if (mode == vocabularyAddModeManual()) {
        return VocabularyQuickAddChoice::Manual;
    }
    return m_access.askChoice
        ? m_access.askChoice()
        : VocabularyQuickAddChoice::Cancel;
}

VocabularyEntry VocabularyQuickAddController::manualEntry(
    const QString &sourceText,
    const QString &scopeId,
    const QString &editedText
) const
{
    VocabularyEntry entry;
    entry.source = sourceText.trimmed();
    entry.target = editedText.trimmed().isEmpty()
        ? sourceText.trimmed()
        : editedText.trimmed();
    entry.scopeId = normalizedScope(scopeId);
    entry.matchMode = QStringLiteral("caseInsensitive");
    entry.enabled = true;
    entry.note = editedText.trimmed().isEmpty()
        ? quickAddText("通过快捷键从选中文字加入。")
        : quickAddText("根据结果小框里的用户修改生成。");
    return entry;
}
