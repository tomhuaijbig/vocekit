#ifndef VOCEKIT_PROMPTS_PANEL_H
#define VOCEKIT_PROMPTS_PANEL_H

#include "prompt_settings_adapter.h"

#include <QWidget>

#include <functional>

class QLabel;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;
class QTextEdit;
class QVBoxLayout;

// 提示词页面只通过快照和操作回调读取或修改配置。
struct PromptsPanelAccess
{
    PromptSettingsAccess prompts;
    std::function<bool(bool, QString *)> setPromptLocked;
    std::function<bool(const PromptLibraryItem &, QString *)> saveLibraryPromptItem;
    std::function<bool(PromptLibraryItem *, QString *)> createLibraryPromptItem;
    std::function<bool(const QString &, QString *)> deleteLibraryPromptItem;
};

class PromptsPanel : public QWidget
{
public:
    explicit PromptsPanel(
        const PromptsPanelAccess &access,
        const std::function<void()> &settingsChanged = std::function<void()>(),
        QWidget *parent = nullptr
    );

    void refresh();

private:
    PromptRuntimeSnapshot promptSnapshot() const;
    bool promptLocked() const;
    QStringList promptScopeOptions() const;
    void setPromptScopeCurrent(const QString &scope);
    bool promptMatchesSearch(const struct PromptTargetInfo &target, const QString &keyword) const;
    QWidget *promptListCard(const struct PromptTargetInfo &target);
    void selectPromptTarget(const QString &id);
    void loadPromptEditor();
    void updatePromptEditorLock();
    void savePromptFromEditor();
    void addPromptLibraryItemFromUi();
    void duplicateCurrentPrompt();
    void deleteCurrentPrompt();
    void notifySettingsChanged();

    PromptsPanelAccess m_access;
    std::function<void()> m_settingsChanged;
    QVBoxLayout *m_listLayout = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QLineEdit *m_nameEdit = nullptr;
    QComboBox *m_scopeBox = nullptr;
    QLabel *m_typeLabel = nullptr;
    QTextEdit *m_editor = nullptr;
    QPushButton *m_saveButton = nullptr;
    QPushButton *m_deleteButton = nullptr;
    QPushButton *m_duplicateButton = nullptr;
    QCheckBox *m_lock = nullptr;
    QString m_currentPromptId;
};

#endif // VOCEKIT_PROMPTS_PANEL_H
