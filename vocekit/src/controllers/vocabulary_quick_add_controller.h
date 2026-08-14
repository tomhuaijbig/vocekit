#ifndef VOCEKIT_VOCABULARY_QUICK_ADD_CONTROLLER_H
#define VOCEKIT_VOCABULARY_QUICK_ADD_CONTROLLER_H

#include "../config/app_settings_data.h"
#include "../domain/app_legacy_types.h"
#include "../input/selected_text_reader.h"
#include "../tasks/vocabulary_suggestion_task.h"

#include <QObject>

#include <functional>

enum class VocabularyQuickAddChoice
{
    UseAi,
    Manual,
    Cancel
};

enum class VocabularyQuickAddOutcome
{
    Busy,
    MissingSelection,
    Cancelled,
    EditorOpened,
    Saved,
    Failed
};

// 系统选区、模型、存储和界面操作通过适配器接入。
struct VocabularyQuickAddAccess
{
    std::function<QString(
        bool,
        SelectedTextNativeWindowHandle
    )> readSelectedText;
    std::function<VocabularyQuickAddChoice()> askChoice;
    std::function<QString()> vocabularyPrompt;
    std::function<VocabularySuggestion(
        const VocabularySuggestionTaskRequest &,
        QString *
    )> requestSuggestion;
    std::function<bool(VocabularyEntry *, QString *)> appendEntry;
    std::function<void(const VocabularyEntry &)> openEditor;
    std::function<void()> notifyVocabularyChanged;
    std::function<void(bool, int)> prepareStatus;
    std::function<void(const QString &, const QString &)> setStatus;
    std::function<void()> hideStatusLater;
    std::function<void(const QString &, const QString &)> showInformation;
    std::function<void(const QString &, const QString &)> showWarning;
    std::function<void()> flushUi;
};

// 负责“选中文字后按快捷键加入词库”的完整决策和保存流程。
class VocabularyQuickAddController : public QObject
{
public:
    explicit VocabularyQuickAddController(
        const VocabularyQuickAddAccess &access,
        QObject *parent = nullptr
    );

    void updateConfiguration(const AppSettingsData &settings);
    VocabularyQuickAddOutcome handleHotkey(
        SelectedTextNativeWindowHandle targetWindow,
        bool recordingBusy
    );
    VocabularyQuickAddOutcome addText(
        const QString &sourceText,
        const QString &scopeId,
        const QString &editedText = QString()
    );
    VocabularyQuickAddOutcome addTextLocally(
        const QString &sourceText,
        const QString &scopeId,
        const QString &editedText = QString()
    );
    VocabularySuggestion suggest(
        const QString &sourceText,
        const QString &scopeId,
        QString *error,
        const QString &editedText = QString(),
        const QString &extraContext = QString()
    ) const;

private:
    VocabularyQuickAddChoice choiceFromSettings() const;
    VocabularyEntry manualEntry(
        const QString &sourceText,
        const QString &scopeId,
        const QString &editedText
    ) const;

    VocabularyQuickAddAccess m_access;
    AppSettingsData m_settings;
};

// 真实选区读取、模型请求和词库文件写入由默认适配器提供。
VocabularyQuickAddAccess defaultVocabularyQuickAddAccess();

#endif // VOCEKIT_VOCABULARY_QUICK_ADD_CONTROLLER_H
