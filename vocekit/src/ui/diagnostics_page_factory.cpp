#include "diagnostics_page_factory.h"

#include "diagnostics_settings_snapshot.h"
#include "hub_settings_state.h"
#include "result_choice_popup.h"
#include "result_popup_test_card.h"
#include "vocabulary_test_card.h"

#include "../tasks/vocabulary_diagnostic_task.h"

namespace {

QString diagnosticsFactoryTr8(const char *text)
{
    return QString::fromUtf8(text);
}

} // namespace

DiagnosticsPanel *createDiagnosticsPanel(
    const DiagnosticsPageFactoryDependencies &dependencies,
    QWidget *parent
)
{
    HubSettingsState *settings = dependencies.settings;
    QWidget *popupFallbackParent = dependencies.popupFallbackParent;
    const auto vocabularyStorePathProvider =
        dependencies.vocabularyStorePathProvider;

    auto *panel = new DiagnosticsPanel(
        dependencies.faqMatchCounter,
        dependencies.faqOpener,
        parent
    );

    DiagnosticsPanelDefaultCards cards;
    cards.floatingBar = dependencies.floatingBar;
    cards.settingsProvider = [settings]() {
        return buildDiagnosticsSettingsSnapshot(
            settings ? settings->toData() : AppSettingsData()
        );
    };
    cards.appBasePathProvider = dependencies.appBasePathProvider;
    cards.recordDirectoryProvider = [settings]() {
        return settings ? settings->recordDirectoryPath() : QString();
    };
    cards.secretsProvider = dependencies.secretsProvider;
    cards.vocabularyTestCard = new VocabularyTestCard(
        [settings, vocabularyStorePathProvider]() {
            const QString storePath = vocabularyStorePathProvider
                ? vocabularyStorePathProvider()
                : QString();
            return buildVocabularyDiagnosticRequest(
                settings ? settings->toData() : AppSettingsData(),
                storePath
            );
        }
    );
    cards.resultPopupTestCard = new ResultPopupTestCard(
        [settings, popupFallbackParent](QWidget *source) {
            QWidget *popupParent = source && source->window()
                ? source->window()
                : popupFallbackParent;
            ClipboardWindowHandle targetWindow = nullptr;
#ifdef Q_OS_WIN
            targetWindow = reinterpret_cast<ClipboardWindowHandle>(
                popupParent ? popupParent->winId() : WId(0)
            );
#endif
            ResultPopupWindowPreferences preferences;
            if (settings) {
                preferences.opacityPercent = settings->resultPopupOpacity();
                preferences.hasGeometry = settings->hasResultPopupGeometry();
                preferences.geometry = settings->resultPopupGeometry();
            }
            auto *popup = new ResultChoicePopup(
                preferences,
                diagnosticsFactoryTr8("结果小框测试"),
                diagnosticsFactoryTr8(
                    "这是测试结果。你可以查看复制、写入、替换选中和关闭按钮是否完整显示。测试小框不会调用大模型。"
                ),
                targetWindow,
                false,
                10000,
                popupParent
            );
            popup->setWindowPreferenceCallback(
                [settings](const QRect &geometry) {
                    if (!settings) {
                        return;
                    }
                    settings->setResultPopupGeometry(geometry);
                    settings->save();
                }
            );
            popup->showNearBottom();
        }
    );
    panel->addDefaultCards(cards);
    return panel;
}
