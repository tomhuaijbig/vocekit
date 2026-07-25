#ifndef VOCEKIT_DIAGNOSTICS_PAGE_FACTORY_H
#define VOCEKIT_DIAGNOSTICS_PAGE_FACTORY_H

#include "diagnostics_panel.h"

class HubSettingsState;

// 诊断页装配输入。工厂负责创建测试页及其默认测试卡片。
struct DiagnosticsPageFactoryDependencies
{
    HubSettingsState *settings = nullptr;
    FloatingBar *floatingBar = nullptr;
    QWidget *popupFallbackParent = nullptr;
    DiagnosticsPanel::FaqMatchCounter faqMatchCounter;
    DiagnosticsPanel::FaqOpener faqOpener;
    DiagnosticsPanelDefaultCards::PathProvider appBasePathProvider;
    DiagnosticsPanelDefaultCards::PathProvider vocabularyStorePathProvider;
    DiagnosticsPanelDefaultCards::SecretConfigProvider secretsProvider;
};

DiagnosticsPanel *createDiagnosticsPanel(
    const DiagnosticsPageFactoryDependencies &dependencies,
    QWidget *parent = nullptr
);

#endif // VOCEKIT_DIAGNOSTICS_PAGE_FACTORY_H
