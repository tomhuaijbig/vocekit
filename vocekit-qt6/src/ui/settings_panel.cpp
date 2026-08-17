#include "settings_panel.h"

#include "api_settings_section.h"
#include "app_dialogs.h"
#include "attention_message.h"
#include "basic_settings_section.h"
#include "history_settings_section.h"
#include "shortcut_settings_section.h"
#include "ui_style.h"

#include "../capture/screenshot_types.h"
#include "../config/app_paths.h"
#include "../config/app_settings_defaults.h"
#include "../input/hotkey_definitions.h"
#include "../providers/model_catalog.h"

#include <QtWidgets>

namespace {

QString settingsPanelTr8(const char *text)
{
    return QString::fromUtf8(text);
}

FunctionSettings *mutableFunctionSettings(
    AppSettingsData *settings,
    const QString &id)
{
    if (!settings) {
        return nullptr;
    }
    const int index = settings->functionIndex(id);
    return index >= 0 ? &settings->functions[index] : nullptr;
}

QString functionShortcut(
    const AppSettingsData &settings,
    const HotkeyDef &definition)
{
    const FunctionSettings function = settings.function(definition.id);
    const QString value = settings.applicationHotkeys
        .value(definition.id, function.shortcut)
        .trimmed();
    return value.isEmpty() ? definition.defaultValue : value;
}

QString screenshotShortcut(
    const FunctionSettings &function,
    const QString &fallback)
{
    const QString value = function.input.screenshotShortcut.trimmed();
    return value.isEmpty() ? fallback : value;
}

bool shortcutConflictsWithOther(
    const AppSettingsData &settings,
    const QString &id,
    const QString &value,
    QString *otherTitle)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized.isEmpty()) {
        return false;
    }

    for (const HotkeyDef &definition : hotkeyDefs()) {
        if (definition.id == id) {
            continue;
        }
        if (functionShortcut(settings, definition).toLower() == normalized) {
            if (otherTitle) {
                *otherTitle = definition.title;
            }
            return true;
        }
    }

    for (const HotkeyDef &definition : coreFunctionDefs()) {
        const FunctionSettings function = settings.function(definition.id);
        if (screenshotHotkeyLogicalId(definition.id) == id
            || !function.input.useScreenshot
            || !screenshotTriggerUsesSeparate(
                function.input.screenshotTriggerMode)) {
            continue;
        }
        if (screenshotShortcut(
                function,
                defaultScreenshotShortcutForFunction(definition.id)
            ).toLower() == normalized) {
            if (otherTitle) {
                *otherTitle = definition.title
                    + settingsPanelTr8("截图");
            }
            return true;
        }
    }

    for (const FunctionSettings &function : settings.functions) {
        if (function.builtIn || function.id == id) {
            continue;
        }
        if (function.shortcut.trimmed().toLower() == normalized) {
            if (otherTitle) {
                *otherTitle = function.name;
            }
            return true;
        }
    }

    for (const FunctionSettings &function : settings.functions) {
        if (function.builtIn
            || screenshotHotkeyLogicalId(function.id) == id
            || !function.input.useScreenshot
            || !screenshotTriggerUsesSeparate(
                function.input.screenshotTriggerMode)) {
            continue;
        }
        if (screenshotShortcut(
                function,
                screenshotShortcutFromFunctionShortcut(function.shortcut)
            ).toLower() == normalized) {
            if (otherTitle) {
                *otherTitle = function.name + settingsPanelTr8("截图");
            }
            return true;
        }
    }
    return false;
}

QString normalizedRecordDirectorySetting(const QString &path)
{
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty() || trimmed == QStringLiteral("records")) {
        return QString();
    }
    const QDir source(trimmed);
    const QString cleanPath = source.isRelative()
        ? QDir::cleanPath(QDir(appBasePath()).absoluteFilePath(trimmed))
        : QDir::cleanPath(trimmed);
    return cleanPath == QDir::cleanPath(defaultRecordDirectory())
        ? QString()
        : cleanPath;
}

QString selectionVocabularyScopeTitle(
    const AppSettingsData &settings,
    const QString &scopeId)
{
    if (scopeId == QStringLiteral("__global")) {
        return settingsPanelTr8("全局词库");
    }
    if (scopeId == QStringLiteral("dictate")) {
        return settingsPanelTr8("听写");
    }
    if (scopeId == QStringLiteral("translate")) {
        return settingsPanelTr8("翻译");
    }
    if (scopeId == QStringLiteral("ask")) {
        return settingsPanelTr8("问答");
    }
    const FunctionSettings function = settings.function(scopeId);
    return function.name.trimmed().isEmpty()
        ? scopeId
        : function.name.trimmed();
}

} // namespace
    SettingsPanel::SettingsPanel(const SettingsPanelAccess &access, const std::function<void()> &onChanged, QWidget *parent, int initialTab)
        : QWidget(parent), m_access(access), m_onChanged(onChanged)
    {
        setObjectName(QStringLiteral("settingsPanel"));
        setFont(appFont());
        setStyleSheet(QStringLiteral("QWidget#settingsPanel { background: #f6f7f9; } QLabel { color: #111827; }"));

        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(28, 24, 28, 24);
        root->setSpacing(16);

        auto *top = new QHBoxLayout;
        auto *heading = new QLabel(settingsPanelTr8("设置"));
        heading->setFont(appFont(24, QFont::DemiBold));
        top->addWidget(heading, 1);
        root->addLayout(top);

        m_tabs = new QTabWidget;
        m_tabs->setStyleSheet(QStringLiteral(
            "QTabWidget::pane { border: 1px solid #dde2ea; background: #ffffff; border-radius: 8px; }"
            "QTabBar::tab { padding: 9px 16px; color: #4b5563; }"
            "QTabBar::tab:selected { color: #111827; font-weight: 600; }"
        ));
        m_tabs->addTab(generalTab(), settingsPanelTr8("常用设置"));
        m_tabs->addTab(vocabularySettingsTab(), settingsPanelTr8("词库"));
        m_tabs->addTab(voiceSettingsTab(), settingsPanelTr8("语音录音"));
        m_tabs->addTab(writeSettingsTab(), settingsPanelTr8("写入"));
        m_tabs->addTab(networkSettingsTab(), settingsPanelTr8("网络"));
        m_tabs->addTab(historySettingsTab(), settingsPanelTr8("历史记录"));
        m_tabs->addTab(shortcutsTab(), settingsPanelTr8("快捷键"));
        m_tabs->addTab(apiTab(), settingsPanelTr8("接口"));
        setCurrentTab(initialTab);
        root->addWidget(m_tabs, 1);
    }

    void SettingsPanel::setCurrentTab(int index)
    {
        if (m_tabs && index >= 0 && index < m_tabs->count()) {
            m_tabs->setCurrentIndex(index);
        }
    }

    bool SettingsPanel::savePendingSecrets(bool showConfirmation)
    {
        return m_apiSettingsSection ? m_apiSettingsSection->saveSecretsFromUi(showConfirmation) : true;
    }

    void SettingsPanel::refreshFromSettings()
    {
        refreshShortcutRows();
        refreshBasicSettings();
        refreshRecordDirectoryLabel();
        refreshApiSettings();
    }

    void SettingsPanel::showSettingDetail(const QString &title, const QString &detail)
    {
        AppDialog dialog(this);
        dialog.setWindowTitle(settingsPanelTr8("设置说明"));
        dialog.setMinimumWidth(560);
        dialog.setFont(appFont());
        dialog.setStyleSheet(QStringLiteral(
            "QDialog { background: #f6f7f9; }"
            "QLabel { color: #111827; }"
        ));

        auto *root = new QVBoxLayout(&dialog);
        root->setContentsMargins(22, 20, 22, 18);
        root->setSpacing(14);

        auto *heading = new QLabel(title);
        heading->setFont(appFont(18, QFont::DemiBold));
        root->addWidget(heading);

        auto *card = new QFrame;
        card->setObjectName(QStringLiteral("card"));
        card->setStyleSheet(cardStyle());
        auto *bodyLayout = new QVBoxLayout(card);
        bodyLayout->setContentsMargins(16, 14, 16, 14);

        auto *body = new QLabel(detail);
        body->setWordWrap(true);
        body->setTextInteractionFlags(Qt::TextSelectableByMouse);
        body->setStyleSheet(QStringLiteral("color: #475467; line-height: 150%;"));
        bodyLayout->addWidget(body);
        root->addWidget(card);

        auto *buttons = new QHBoxLayout;
        buttons->addStretch();
        auto *close = new QPushButton(settingsPanelTr8("关闭"));
        close->setFixedHeight(36);
        close->setStyleSheet(buttonStyle(QStringLiteral("#111827")));
        connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
        buttons->addWidget(close);
        root->addLayout(buttons);

        dialog.exec();
    }

    QWidget *SettingsPanel::shortcutsTab()
    {
        ShortcutSettingsSection::Callbacks callbacks;
        callbacks.snapshotProvider = [this]() {
            const AppSettingsData settings = settingsSnapshot();
            ShortcutSettingsSnapshot snapshot;
            QSet<QString> coreIds;
            for (const HotkeyDef &def : coreFunctionDefs()) {
                coreIds.insert(def.id);
            }
            for (const HotkeyDef &def : hotkeyDefs()) {
                ShortcutSettingsItem item;
                item.id = def.id;
                item.title = def.title;
                item.hint = def.hint;
                item.shortcut = functionShortcut(settings, def);
                item.defaultShortcut = def.defaultValue;
                if (coreIds.contains(def.id)) {
                    const FunctionSettings function = settings.function(def.id);
                    item.screenshotShortcutEnabled =
                        function.input.useScreenshot
                        && screenshotTriggerUsesSeparate(
                            function.input.screenshotTriggerMode);
                    item.screenshotShortcut = screenshotShortcut(
                        function,
                        defaultScreenshotShortcutForFunction(def.id)
                    );
                    item.defaultScreenshotShortcut =
                        defaultScreenshotShortcutForFunction(def.id);
                }
                snapshot.builtInItems.append(item);
            }
            for (const FunctionSettings &function : settings.functions) {
                if (function.builtIn) {
                    continue;
                }
                ShortcutSettingsItem item;
                item.id = function.id;
                item.title = function.name.trimmed().isEmpty()
                    ? settingsPanelTr8("自定义功能")
                    : function.name.trimmed();
                item.shortcut = function.shortcut;
                item.custom = true;
                item.screenshotShortcutEnabled = function.input.useScreenshot
                    && screenshotTriggerUsesSeparate(
                        function.input.screenshotTriggerMode);
                item.screenshotShortcut = screenshotShortcut(
                    function,
                    screenshotShortcutFromFunctionShortcut(function.shortcut)
                );
                item.defaultScreenshotShortcut =
                    screenshotShortcutFromFunctionShortcut(function.shortcut);
                snapshot.customItems.append(item);
            }
            return snapshot;
        };
        callbacks.conflictsWithOther = [this](
            const QString &id,
            const QString &shortcut,
            QString *otherTitle) {
            return shortcutConflictsWithOther(
                settingsSnapshot(),
                id,
                shortcut,
                otherTitle
            );
        };
        callbacks.setFunctionShortcut = [this](
            const QString &id,
            const QString &shortcut) {
            updatePendingSettings([id, shortcut](AppSettingsData &settings) {
                for (const HotkeyDef &def : hotkeyDefs()) {
                    if (def.id == id) {
                        settings.applicationHotkeys.insert(id, shortcut);
                        FunctionSettings *function = mutableFunctionSettings(
                            &settings,
                            id
                        );
                        if (function) {
                            function->shortcut = shortcut;
                        }
                        return;
                    }
                }
                FunctionSettings *function = mutableFunctionSettings(
                    &settings,
                    id
                );
                if (function) {
                    function->shortcut = shortcut;
                }
            });
        };
        callbacks.setScreenshotShortcut = [this](
            const QString &id,
            const QString &shortcut) {
            updatePendingSettings([id, shortcut](AppSettingsData &settings) {
                FunctionSettings *function = mutableFunctionSettings(
                    &settings,
                    id
                );
                if (function) {
                    function->input.screenshotShortcut = shortcut;
                }
            });
        };
        callbacks.saveAndRefresh = [this]() {
            saveAndRefresh();
        };
        callbacks.showDetail = [this](const QString &title, const QString &detail) {
            showSettingDetail(title, detail);
        };
        m_shortcutSettingsSection = new ShortcutSettingsSection(callbacks, this);
        return m_shortcutSettingsSection;
    }

    QWidget *SettingsPanel::generalTab()
    {
        m_generalSettingsSection = newBasicSettingsSection(BasicSettingsSection::General);
        return m_generalSettingsSection;
    }

    QWidget *SettingsPanel::vocabularySettingsTab()
    {
        m_vocabularySettingsSection = newBasicSettingsSection(BasicSettingsSection::Vocabulary);
        return m_vocabularySettingsSection;
    }

    QWidget *SettingsPanel::voiceSettingsTab()
    {
        m_voiceSettingsSection = newBasicSettingsSection(BasicSettingsSection::Voice);
        return m_voiceSettingsSection;
    }

    QWidget *SettingsPanel::networkSettingsTab()
    {
        m_networkSettingsSection = newBasicSettingsSection(BasicSettingsSection::Network);
        return m_networkSettingsSection;
    }

    QWidget *SettingsPanel::writeSettingsTab()
    {
        m_writeSettingsSection = newBasicSettingsSection(BasicSettingsSection::Write);
        return m_writeSettingsSection;
    }

    BasicSettingsSection *SettingsPanel::newBasicSettingsSection(BasicSettingsSection::Kind kind)
    {
        BasicSettingsSection::Callbacks callbacks;
        callbacks.snapshotProvider = [this]() {
            const AppSettingsData settings = settingsSnapshot();
            BasicSettingsSnapshot snapshot;
            snapshot.trayResident = settings.trayResident;
            snapshot.autoStartEnabled = settings.autoStartEnabled;
            snapshot.strongSelectionEnabled = settings.strongSelectionEnabled;
            snapshot.vocabularyEnabled = settings.vocabularyEnabled;
            snapshot.vocabularyAddMode = settings.vocabularyAddMode;
            snapshot.vocabularyOnlyForVoiceInput =
                settings.vocabularyOnlyForVoiceInput;
            snapshot.vocabularyPromptEntryLimit =
                settings.vocabularyPromptEntryLimit;
            snapshot.floatingBarEnabled = settings.floatingBarEnabled;
            snapshot.floatingBarStyle = settings.floatingBarStyle;
            snapshot.writeFailurePopupFallbackEnabled =
                settings.writeFailurePopupFallbackEnabled;
            snapshot.streamingSpeechRecognitionEnabled =
                settings.streamingSpeechRecognitionEnabled;
            snapshot.preRecordCountdownEnabled =
                settings.preRecordCountdownEnabled;
            snapshot.recordingBeepEnabled = settings.recordingBeepEnabled;
            snapshot.dictatePolishEnabled = settings.dictatePolishEnabled;
            snapshot.useSystemProxy = settings.useSystemProxy;
            snapshot.selectionContext = settings.selectionContext;
            return snapshot;
        };
        callbacks.applySnapshot = [this](const BasicSettingsSnapshot &snapshot) {
            updatePendingSettings([snapshot](AppSettingsData &settings) {
                settings.trayResident = snapshot.trayResident;
                settings.autoStartEnabled = snapshot.autoStartEnabled;
                settings.strongSelectionEnabled =
                    snapshot.strongSelectionEnabled;
                settings.vocabularyEnabled = snapshot.vocabularyEnabled;
                settings.vocabularyAddMode = snapshot.vocabularyAddMode;
                settings.vocabularyOnlyForVoiceInput =
                    snapshot.vocabularyOnlyForVoiceInput;
                settings.vocabularyPromptEntryLimit =
                    snapshot.vocabularyPromptEntryLimit;
                settings.floatingBarEnabled = snapshot.floatingBarEnabled;
                settings.floatingBarStyle = snapshot.floatingBarStyle;
                settings.writeFailurePopupFallbackEnabled =
                    snapshot.writeFailurePopupFallbackEnabled;
                settings.streamingSpeechRecognitionEnabled =
                    snapshot.streamingSpeechRecognitionEnabled;
                settings.preRecordCountdownEnabled =
                    snapshot.preRecordCountdownEnabled;
                settings.recordingBeepEnabled =
                    snapshot.recordingBeepEnabled;
                settings.dictatePolishEnabled = snapshot.dictatePolishEnabled;
                settings.useSystemProxy = snapshot.useSystemProxy;
                settings.selectionContext = snapshot.selectionContext;
            });
        };
        callbacks.saveAndRefresh = [this]() {
            saveAndRefresh();
        };
        callbacks.showDetail = [this](const QString &title, const QString &detail) {
            showSettingDetail(title, detail);
        };
        callbacks.previewFloatingBarStyle =
            m_access.previewFloatingBarStyle;
        callbacks.modelCatalogProvider = []() {
            QVector<QPair<QString, QString>> catalog;
            for (const ModelOption &option : modelOptions()) {
                const QString id = option.id.trimmed();
                if (id.isEmpty()) {
                    continue;
                }
                const QString title = option.title.trimmed();
                catalog.append(qMakePair(
                    title.isEmpty() ? id : title,
                    id
                ));
            }
            return catalog;
        };
        callbacks.vocabularyScopeCatalogProvider = [this]() {
            const AppSettingsData settings = settingsSnapshot();
            QVector<QPair<QString, QString>> catalog;
            for (const QString &scopeId :
                 writableSelectionContextVocabularyScopeIds(settings)) {
                if (scopeId == QStringLiteral("__all")) {
                    continue;
                }
                catalog.append(qMakePair(
                    selectionVocabularyScopeTitle(settings, scopeId),
                    scopeId
                ));
            }
            return catalog;
        };
        callbacks.confirmRestoreAllSelectionActions = [this]() {
            return QMessageBox::question(
                this,
                settingsPanelTr8("恢复全部默认设置"),
                settingsPanelTr8("确定恢复五个工具条功能的默认设置吗？工具条总开关和顺序会保留。"),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No
            ) == QMessageBox::Yes;
        };
        callbacks.selectionActionValidationWarning =
            [this](const QString &warning) {
            showAttentionWarning(
                this,
                settingsPanelTr8("设置未更改"),
                warning
            );
        };
        return new BasicSettingsSection(kind, callbacks, this);
    }

    QWidget *SettingsPanel::historySettingsTab()
    {
        HistorySettingsSection::Callbacks callbacks;
        callbacks.snapshotProvider = [this]() {
            const AppSettingsData settings = settingsSnapshot();
            HistorySettingsSnapshot snapshot;
            snapshot.recordDirectoryPath = settings.recordDirectory.trimmed().isEmpty()
                ? defaultRecordDirectory()
                : settings.recordDirectory;
            snapshot.historyInitialLoadCount = settings.historyInitialLoadCount;
            snapshot.historyLoadMoreCount = settings.historyLoadMoreCount;
            snapshot.logInitialLoadCount = settings.logInitialLoadCount;
            snapshot.logLoadMoreCount = settings.logLoadMoreCount;
            return snapshot;
        };
        callbacks.setRecordDirectory = [this](const QString &path) {
            updatePendingSettings([path](AppSettingsData &settings) {
                settings.recordDirectory =
                    normalizedRecordDirectorySetting(path);
            });
        };
        callbacks.resetRecordDirectory = [this]() {
            updatePendingSettings([](AppSettingsData &settings) {
                settings.recordDirectory.clear();
            });
        };
        callbacks.setHistoryInitialLoadCount = [this](int count) {
            updatePendingSettings([count](AppSettingsData &settings) {
                settings.historyInitialLoadCount = count;
            });
        };
        callbacks.setHistoryLoadMoreCount = [this](int count) {
            updatePendingSettings([count](AppSettingsData &settings) {
                settings.historyLoadMoreCount = count;
            });
        };
        callbacks.setLogInitialLoadCount = [this](int count) {
            updatePendingSettings([count](AppSettingsData &settings) {
                settings.logInitialLoadCount = count;
            });
        };
        callbacks.setLogLoadMoreCount = [this](int count) {
            updatePendingSettings([count](AppSettingsData &settings) {
                settings.logLoadMoreCount = count;
            });
        };
        callbacks.saveAndRefresh = [this]() {
            saveAndRefresh();
        };
        callbacks.showDetail = [this](const QString &title, const QString &detail) {
            showSettingDetail(title, detail);
        };
        m_historySettingsSection = new HistorySettingsSection(callbacks, this);
        return m_historySettingsSection;
    }

    QWidget *SettingsPanel::apiTab()
    {
        ApiSettingsSection::Callbacks callbacks;
        callbacks.snapshotProvider = [this]() {
            const AppSettingsData settings = settingsSnapshot();
            ApiSettingsSnapshot snapshot;
            snapshot.speechProvider = settings.speechProvider;
            snapshot.ocrEngine = settings.ocrEngine;
            snapshot.windowsSpeechLanguage = settings.windowsSpeechLanguage;
            snapshot.useSystemProxy = settings.useSystemProxy;
            return snapshot;
        };
        callbacks.saveRuntimeSettings = [this](
            const QString &speechProvider,
            const QString &ocrEngine,
            const QString &windowsSpeechLanguage) {
            AppSettingsData settings = settingsSnapshot();
            settings.speechProvider = speechProvider;
            settings.ocrEngine = ocrEngine;
            settings.windowsSpeechLanguage = normalizeWindowsSpeechLanguage(
                windowsSpeechLanguage
            );
            return persistSettings(settings);
        };
        callbacks.onChanged = [this]() {
            if (m_onChanged) {
                m_onChanged();
            }
        };
        callbacks.showDetail = [this](const QString &title, const QString &detail) {
            showSettingDetail(title, detail);
        };
        m_apiSettingsSection = new ApiSettingsSection(callbacks, this);
        return m_apiSettingsSection;
    }

    void SettingsPanel::refreshRecordDirectoryLabel()
    {
        if (m_historySettingsSection) {
            m_historySettingsSection->refreshFromSettings();
        }
    }

    void SettingsPanel::refreshBasicSettings()
    {
        const QVector<BasicSettingsSection *> sections = QVector<BasicSettingsSection *>()
            << m_generalSettingsSection
            << m_vocabularySettingsSection
            << m_voiceSettingsSection
            << m_writeSettingsSection
            << m_networkSettingsSection;
        for (BasicSettingsSection *section : sections) {
            if (section) {
                section->refreshFromSettings();
            }
        }
    }

    void SettingsPanel::refreshApiSettings()
    {
        if (m_apiSettingsSection) {
            m_apiSettingsSection->refreshFromSettings();
        }
    }

    void SettingsPanel::refreshShortcutRows()
    {
        if (m_shortcutSettingsSection) {
            m_shortcutSettingsSection->refreshFromSettings();
        }
    }

    AppSettingsData SettingsPanel::settingsSnapshot() const
    {
        if (m_hasPendingSettings) {
            return m_pendingSettings;
        }
        return m_access.snapshotProvider
            ? m_access.snapshotProvider()
            : AppSettingsData();
    }

    void SettingsPanel::updatePendingSettings(
        const std::function<void(AppSettingsData &)> &update)
    {
        AppSettingsData settings = settingsSnapshot();
        if (update) {
            update(settings);
        }
        m_pendingSettings = settings;
        m_hasPendingSettings = true;
    }

    bool SettingsPanel::persistSettings(const AppSettingsData &settings)
    {
        const bool saved = m_access.applyAndSave
            && m_access.applyAndSave(settings);
        m_hasPendingSettings = false;
        return saved;
    }

    void SettingsPanel::saveAndRefresh()
    {
        const bool saved = persistSettings(settingsSnapshot());
        if (!saved) {
            showAttentionWarning(this, settingsPanelTr8("保存失败"), settingsPanelTr8("无法写入 config/settings.json。"));
        }

        refreshBasicSettings();
        refreshShortcutRows();
        refreshRecordDirectoryLabel();
        refreshApiSettings();
        if (saved && m_onChanged) {
            m_onChanged();
        }
    }
