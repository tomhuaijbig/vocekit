#ifndef VOCEKIT_SETTINGS_PANEL_H
#define VOCEKIT_SETTINGS_PANEL_H

#include "basic_settings_section.h"

#include "../config/app_settings_data.h"

#include <QString>
#include <QWidget>
#include <functional>

class ApiSettingsSection;
class HistorySettingsSection;
class QTabWidget;
class ShortcutSettingsSection;
class UpdateSettingsSection;

// 设置页只通过类型化快照读写配置，避免界面层依赖旧配置对象。
struct SettingsPanelAccess
{
    std::function<AppSettingsData()> snapshotProvider;
    std::function<bool(const AppSettingsData &)> applyAndSave;
    std::function<void(const QString &)> previewFloatingBarStyle;
};

// 设置页总面板：只负责组装各个设置分区，具体分区逻辑放到独立类里。
class SettingsPanel : public QWidget
{
public:
    explicit SettingsPanel(
        const SettingsPanelAccess &access,
        const std::function<void()> &onChanged,
        QWidget *parent = nullptr,
        int initialTab = 0
    );

    void setCurrentTab(int index);
    bool savePendingSecrets(bool showConfirmation = false);
    void refreshFromSettings();

private:
    void showSettingDetail(const QString &title, const QString &detail);

    QWidget *shortcutsTab();
    QWidget *generalTab();
    QWidget *vocabularySettingsTab();
    QWidget *voiceSettingsTab();
    QWidget *writeSettingsTab();
    QWidget *networkSettingsTab();
    BasicSettingsSection *newBasicSettingsSection(BasicSettingsSection::Kind kind);
    QWidget *historySettingsTab();
    QWidget *apiTab();
    QWidget *updateTab();

    void refreshRecordDirectoryLabel();
    void refreshBasicSettings();
    void refreshApiSettings();
    void refreshShortcutRows();
    void saveAndRefresh();

    AppSettingsData settingsSnapshot() const;
    void updatePendingSettings(
        const std::function<void(AppSettingsData &)> &update
    );
    bool persistSettings(const AppSettingsData &settings);

    SettingsPanelAccess m_access;
    AppSettingsData m_pendingSettings;
    bool m_hasPendingSettings = false;
    std::function<void()> m_onChanged;
    QTabWidget *m_tabs = nullptr;
    ShortcutSettingsSection *m_shortcutSettingsSection = nullptr;
    BasicSettingsSection *m_generalSettingsSection = nullptr;
    BasicSettingsSection *m_vocabularySettingsSection = nullptr;
    BasicSettingsSection *m_voiceSettingsSection = nullptr;
    BasicSettingsSection *m_writeSettingsSection = nullptr;
    BasicSettingsSection *m_networkSettingsSection = nullptr;
    ApiSettingsSection *m_apiSettingsSection = nullptr;
    HistorySettingsSection *m_historySettingsSection = nullptr;
    UpdateSettingsSection *m_updateSettingsSection = nullptr;
};

#endif // VOCEKIT_SETTINGS_PANEL_H
