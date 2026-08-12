#ifndef VOCEKIT_HUB_SETTINGS_STATE_H
#define VOCEKIT_HUB_SETTINGS_STATE_H

#include "../config/app_settings_data.h"
#include "../domain/app_legacy_types.h"
#include "function_flow_settings_access.h"

#include <QRect>
#include <QStringList>
#include <QVector>

#include <functional>

// 主窗口只通过快照和保存回调访问配置，不再感知配置文件或旧设置对象。
struct HubWindowAccess
{
    std::function<AppSettingsData()> settingsSnapshotProvider;
    std::function<QVector<PromptLibraryItem>()> promptLibraryProvider;
    std::function<bool(const AppSettingsData &)> applyAndSave;
    std::function<bool(
        const AppSettingsData &,
        OperationError *
    )> applyNonFlowAndSave;
    std::function<bool(const QVector<PromptLibraryItem> &)> savePromptLibrary;
    FunctionFlowSettingsAccess functionFlows;
    std::function<void(const QStringList &)>
        refreshFunctionFlowRuntime;
    std::function<void(const QStringList &)>
        refreshFunctionFlowHotkeys;
};

FunctionSettings functionSettingsFromCustomFunction(
    const CustomFunctionDef &source
);

// 主窗口编辑中的配置副本。所有修改先落在类型化数据中，再统一提交给存储层。
class HubSettingsState
{
public:
    explicit HubSettingsState(const HubWindowAccess &access = HubWindowAccess());

    explicit operator bool() const;
    void load();
    bool save(OperationError *error = nullptr) const;
    bool replaceAndSave(
        const AppSettingsData &data,
        OperationError *error = nullptr
    );
    void replaceFunctionFlowState(
        const QString &functionId,
        const FunctionFlowState &state
    );
    bool reloadFunctionFlowState(const QString &functionId);
    // 调用方需要独立快照，不能持有可随界面编辑变化的内部引用。
    // cppcheck-suppress returnByReference
    AppSettingsData toData() const;

    QString hotkey(const QString &id) const;
    void setHotkey(const QString &id, const QString &value);
    bool conflictsWithOther(const QString &id, const QString &value, QString *otherTitle) const;

    const QVector<CustomFunctionDef> &customFunctions() const;
    QString nextCustomFunctionId() const;
    QString suggestedCustomShortcut() const;
    void addCustomFunction(const CustomFunctionDef &function);
    void updateCustomFunction(const CustomFunctionDef &function);
    void removeCustomFunction(const QString &id);

    const QVector<PromptLibraryItem> &promptLibrary() const;
    PromptLibraryItem promptLibraryItem(const QString &id) const;
    QString nextPromptLibraryId() const;
    QString promptIdFor(const QString &functionId) const;
    void setPromptIdFor(const QString &functionId, const QString &promptId);
    void addPromptLibraryItem(PromptLibraryItem item);
    bool updatePromptLibraryItem(const PromptLibraryItem &item);
    bool removePromptLibraryItem(const QString &id);
    bool savePromptLibrary() const;

    QString modelFor(const QString &id) const;
    void setModelFor(const QString &id, const QString &model);
    QString outputModeFor(const QString &id) const;
    void setOutputModeFor(const QString &id, const QString &mode);
    QString floatingBarStyleOverrideFor(const QString &id) const;
    void setFloatingBarStyleOverrideFor(
        const QString &id,
        const QString &style
    );
    QStringList outputOrderFor(const QString &id) const;
    void setOutputOrderFor(const QString &id, const QStringList &order);
    QString resultTemplateFor(const QString &id) const;
    void setResultTemplateFor(const QString &id, const QString &value);
    QStringList resultActionsFor(const QString &id) const;
    void setResultActionsFor(const QString &id, const QStringList &actions);
    FunctionNetworkPolicies networkPoliciesFor(const QString &id) const;
    void setNetworkPoliciesFor(const QString &id, const FunctionNetworkPolicies &policies);

    bool useSelectionFor(const QString &id) const;
    void setUseSelectionFor(const QString &id, bool enabled);
    bool useVoiceFor(const QString &id) const;
    void setUseVoiceFor(const QString &id, bool enabled);
    bool useScreenshotFor(const QString &id) const;
    void setUseScreenshotFor(const QString &id, bool enabled);
    QStringList inputOrderFor(const QString &id) const;
    void setInputOrderFor(const QString &id, const QStringList &order);
    QString screenshotTriggerModeFor(const QString &id) const;
    void setScreenshotTriggerModeFor(const QString &id, const QString &mode);
    QString screenshotShortcutFor(const QString &id) const;
    void setScreenshotShortcutFor(const QString &id, const QString &shortcut);

    int floatingBarSecondsFor(const QString &id) const;
    void setFloatingBarSecondsFor(const QString &id, int seconds);
    int resultPopupSecondsFor(const QString &id) const;
    void setResultPopupSecondsFor(const QString &id, int seconds);
    int countdownSecondsFor(const QString &id) const;
    void setCountdownSecondsFor(const QString &id, int seconds);
    bool recordingBeepEnabledFor(const QString &id) const;
    void setRecordingBeepEnabledFor(const QString &id, bool enabled);
    QString recordingBeepPathFor(const QString &id) const;
    void setRecordingBeepPathFor(const QString &id, const QString &path);
    QString recordingTriggerModeFor(const QString &id) const;
    void setRecordingTriggerModeFor(const QString &id, const QString &mode);
    bool longRecordingEnabledFor(const QString &id) const;
    void setLongRecordingEnabledFor(const QString &id, bool enabled);
    int segmentSecondsFor(const QString &id) const;
    void setSegmentSecondsFor(const QString &id, int seconds);
    int maxRecordingMinutesFor(const QString &id) const;
    void setMaxRecordingMinutesFor(const QString &id, int minutes);

    QStringList functionOrderIds() const;
    bool setFunctionOrderIds(const QStringList &ids);

    const QStringList &favoriteFolders() const;
    bool addFavoriteFolder(const QString &name);
    int historyInitialLoadCount() const;
    int historyLoadMoreCount() const;

    bool trayResident() const;
    bool autoStartEnabled() const;
    bool strongSelectionEnabled() const;
    void setStrongSelectionEnabled(bool enabled);
    bool floatingBarEnabled() const;
    void setFloatingBarEnabled(bool enabled);
    bool promptLocked() const;
    void setPromptLocked(bool locked);
    bool useSystemProxy() const;
    void setUseSystemProxy(bool enabled);
    int resultPopupOpacity() const;
    QString recordDirectoryPath() const;
    void setRecordDirectoryPath(const QString &path);
    void resetRecordDirectory();
    QString speechProvider() const;
    void setSpeechProvider(const QString &provider);
    QString windowsSpeechLanguage() const;
    void setWindowsSpeechLanguage(const QString &language);
    QString ocrEngine() const;
    void setOcrEngine(const QString &engine);

    bool hasFloatingBarPosition() const;
    QPoint floatingBarPosition() const;
    void setFloatingBarPosition(const QPoint &position);
    bool hasResultPopupGeometry() const;
    QRect resultPopupGeometry() const;
    void setResultPopupGeometry(const QRect &geometry);
    bool hasScreenshotLauncherPosition() const;
    QPoint screenshotLauncherPosition() const;
    void setScreenshotLauncherPosition(const QPoint &position);

private:
    FunctionSettings *mutableFunction(const QString &id);
    const FunctionSettings *findFunction(const QString &id) const;
    FunctionSettings defaultFunction(const QString &id) const;
    QStringList defaultFunctionOrderIds() const;
    bool hasPromptId(const QString &id) const;
    void refreshCustomFunctions();

    HubWindowAccess m_access;
    AppSettingsData m_data;
    QVector<PromptLibraryItem> m_promptLibrary;
    QVector<CustomFunctionDef> m_customFunctions;
};

#endif // VOCEKIT_HUB_SETTINGS_STATE_H
