#include <QtTest>

#include "../../src/config/app_settings_data.h"
#include "../../src/config/app_settings_defaults.h"
#include "../../src/domain/selection_context_actions.h"

#include <type_traits>

typedef SelectionContextActionCustomizationMap (*SelectionActionNormalizer)(
    const SelectionContextActionCustomizationMap &,
    const SelectionContextActionNormalizationContext &
);
static_assert(
    std::is_same<
        decltype(&normalizeSelectionContextActionCustomizations),
        SelectionActionNormalizer
    >::value,
    "Selection action normalization must expose one strong-context entry"
);

class AppSettingsDefaultsTests : public QObject
{
    Q_OBJECT

private slots:
    void defaultsExposeFiveIndependentActionCustomizations()
    {
        const AppSettingsData data;
        const SelectionContextActionCustomizationMap defaults =
            data.selectionContext.actionCustomizations;

        QCOMPARE(defaults.size(), 5);
        for (const QString &id : defaultSelectionContextActionOrder()) {
            QVERIFY(defaults.contains(id));
            const SelectionContextActionCustomization value =
                defaults.value(id);
            QCOMPARE(value.displayName, selectionContextActionTitle(id));
            QVERIFY(value.visible);
            QVERIFY(value.modelId.isEmpty());
            QVERIFY(value.promptOverride.isEmpty());
            QVERIFY(value.targetLanguage.isEmpty());
            QCOMPARE(value.vocabularyScopeId, QStringLiteral("__global"));
            QCOMPARE(value.copyMode, QStringLiteral("original"));
        }
    }

    void customizationNormalizationBoundsEveryUserField()
    {
        SelectionContextActionCustomizationMap values =
            defaultSelectionContextActionCustomizations();
        SelectionContextActionCustomization translate =
            values.value(selectionContextActionTranslate());
        translate.displayName = QString(40, QLatin1Char('n'));
        translate.promptOverride = QString(9000, QLatin1Char('p'));
        translate.targetLanguage = QString(80, QLatin1Char('l'));
        values.insert(selectionContextActionTranslate(), translate);
        SelectionContextActionCustomization copy =
            values.value(selectionContextActionCopy());
        copy.copyMode = QStringLiteral("invalid");
        values.insert(selectionContextActionCopy(), copy);

        SelectionContextActionNormalizationContext context;
        context.actionOrder = defaultSelectionContextActionOrder();
        context.writableVocabularyScopeIds = QStringList()
            << QStringLiteral("__global")
            << QStringLiteral("translate");
        const SelectionContextActionCustomizationMap normalized =
            normalizeSelectionContextActionCustomizations(values, context);

        QCOMPARE(
            normalized.value(selectionContextActionTranslate())
                .displayName.size(),
            24
        );
        QCOMPARE(
            normalized.value(selectionContextActionTranslate())
                .promptOverride.size(),
            8000
        );
        QCOMPARE(
            normalized.value(selectionContextActionTranslate())
                .targetLanguage.size(),
            64
        );
        QCOMPARE(
            normalized.value(selectionContextActionCopy()).copyMode,
            QStringLiteral("original")
        );
    }

    void displayAndVisibilityHelpersUseCustomizationAndOrderedFallback()
    {
        SelectionContextActionCustomizationMap values =
            defaultSelectionContextActionCustomizations();
        SelectionContextActionCustomization explain =
            values.value(selectionContextActionExplain());
        explain.displayName = QString::fromUtf8("  讲清楚  ");
        values.insert(selectionContextActionExplain(), explain);
        for (const QString &id : defaultSelectionContextActionOrder()) {
            SelectionContextActionCustomization item = values.value(id);
            item.visible = false;
            values.insert(id, item);
        }

        QCOMPARE(
            selectionContextActionDisplayName(
                selectionContextActionExplain(),
                values
            ),
            QString::fromUtf8("讲清楚")
        );
        QCOMPARE(
            visibleSelectionContextActionOrder(
                QStringList()
                    << selectionContextActionCopy()
                    << selectionContextActionExplain(),
                values
            ),
            QStringList() << selectionContextActionCopy()
        );
    }

    void normalizationUsesCurrentOrderFirstValidAction()
    {
        SelectionContextActionCustomizationMap allHidden =
            defaultSelectionContextActionCustomizations();
        for (const QString &id : defaultSelectionContextActionOrder()) {
            SelectionContextActionCustomization item = allHidden.value(id);
            item.visible = false;
            allHidden.insert(id, item);
        }
        SelectionContextActionCustomization save =
            allHidden.value(selectionContextActionSave());
        save.vocabularyScopeId = QStringLiteral("custom-scope");
        allHidden.insert(selectionContextActionSave(), save);
        const QStringList requestedOrder = QStringList()
            << QStringLiteral("unknown")
            << selectionContextActionCopy()
            << selectionContextActionCopy()
            << selectionContextActionTranslate();
        SelectionContextActionNormalizationContext context;
        context.actionOrder = requestedOrder;
        context.writableVocabularyScopeIds = QStringList()
            << QStringLiteral("__global")
            << QStringLiteral("custom-scope");

        const SelectionContextActionCustomizationMap ordered =
            normalizeSelectionContextActionCustomizations(
                allHidden,
                context
            );

        for (const QString &id : defaultSelectionContextActionOrder()) {
            QCOMPARE(
                ordered.value(id).visible,
                id == selectionContextActionCopy()
            );
        }
        QCOMPARE(
            ordered.value(selectionContextActionSave()).vocabularyScopeId,
            QStringLiteral("custom-scope")
        );
    }

    void vocabularyScopeNormalizationDistinguishesSyntaxAndCatalogPasses()
    {
        SelectionContextActionCustomizationMap scopes =
            defaultSelectionContextActionCustomizations();
        SelectionContextActionCustomization ai =
            scopes.value(selectionContextActionAiSearch());
        ai.vocabularyScopeId = QStringLiteral("__global");
        scopes.insert(selectionContextActionAiSearch(), ai);
        SelectionContextActionCustomization translate =
            scopes.value(selectionContextActionTranslate());
        translate.vocabularyScopeId.clear();
        scopes.insert(selectionContextActionTranslate(), translate);
        SelectionContextActionCustomization explain =
            scopes.value(selectionContextActionExplain());
        explain.vocabularyScopeId = QStringLiteral("__all");
        scopes.insert(selectionContextActionExplain(), explain);
        SelectionContextActionCustomization save =
            scopes.value(selectionContextActionSave());
        save.vocabularyScopeId = QStringLiteral("custom-scope");
        scopes.insert(selectionContextActionSave(), save);
        SelectionContextActionCustomization copy =
            scopes.value(selectionContextActionCopy());
        copy.vocabularyScopeId = QStringLiteral("deleted-scope");
        scopes.insert(selectionContextActionCopy(), copy);

        SelectionContextActionNormalizationContext syntaxContext;
        syntaxContext.actionOrder = defaultSelectionContextActionOrder();
        const SelectionContextActionCustomizationMap syntaxOnly =
            normalizeSelectionContextActionCustomizations(
                scopes,
                syntaxContext
            );
        QCOMPARE(
            syntaxOnly.value(selectionContextActionAiSearch())
                .vocabularyScopeId,
            QStringLiteral("__global")
        );
        QCOMPARE(
            syntaxOnly.value(selectionContextActionTranslate())
                .vocabularyScopeId,
            QStringLiteral("__global")
        );
        QCOMPARE(
            syntaxOnly.value(selectionContextActionExplain())
                .vocabularyScopeId,
            QStringLiteral("__global")
        );
        QCOMPARE(
            syntaxOnly.value(selectionContextActionSave()).vocabularyScopeId,
            QStringLiteral("custom-scope")
        );
        QCOMPARE(
            syntaxOnly.value(selectionContextActionCopy()).vocabularyScopeId,
            QStringLiteral("deleted-scope")
        );

        SelectionContextActionNormalizationContext completeContext;
        completeContext.actionOrder = defaultSelectionContextActionOrder();
        completeContext.writableVocabularyScopeIds = QStringList()
            << QStringLiteral("__global")
            << QStringLiteral("custom-scope");
        const SelectionContextActionCustomizationMap complete =
            normalizeSelectionContextActionCustomizations(
                scopes,
                completeContext
            );
        QCOMPARE(
            complete.value(selectionContextActionAiSearch()).vocabularyScopeId,
            QStringLiteral("__global")
        );
        QCOMPARE(
            complete.value(selectionContextActionTranslate()).vocabularyScopeId,
            QStringLiteral("__global")
        );
        QCOMPARE(
            complete.value(selectionContextActionExplain()).vocabularyScopeId,
            QStringLiteral("__global")
        );
        QCOMPARE(
            complete.value(selectionContextActionSave()).vocabularyScopeId,
            QStringLiteral("custom-scope")
        );
        QCOMPARE(
            complete.value(selectionContextActionCopy()).vocabularyScopeId,
            QStringLiteral("__global")
        );
    }

    void defaultsKeepAutomaticSelectionToolbarOptIn()
    {
        const AppSettingsData data;

        QVERIFY(!data.selectionContext.enabled);
        QVERIFY(data.selectionContext.keyboardSelectionEnabled);
        QCOMPARE(data.selectionContext.minimumTextLength, 2);
        QVERIFY(data.selectionContext.closeOnOutsideClick);
        QVERIFY(data.selectionContext.pinEnabled);
        QVERIFY(!data.selectionContext.networkConsentAcknowledged);
        QCOMPARE(data.selectionContext.pauseMinutes, 30);
        QCOMPARE(
            data.selectionContext.actionOrder,
            QStringList()
                << selectionContextActionAiSearch()
                << selectionContextActionTranslate()
                << selectionContextActionExplain()
                << selectionContextActionSave()
                << selectionContextActionCopy()
        );
    }

    void selectionContextActionCatalogIsStableAndRejectsMalformedFunctions()
    {
        QCOMPARE(
            defaultSelectionContextActionOrder(),
            QStringList()
                << QStringLiteral("ai-search")
                << QStringLiteral("translate")
                << QStringLiteral("explain")
                << QStringLiteral("save")
                << QStringLiteral("copy")
        );
        QCOMPARE(
            normalizeSelectionContextActionOrder(
                QStringList()
                    << QStringLiteral("unknown")
                    << selectionContextActionCopy()
                    << selectionContextActionCopy()
            ),
            QStringList()
                << selectionContextActionCopy()
                << selectionContextActionAiSearch()
                << selectionContextActionTranslate()
                << selectionContextActionExplain()
                << selectionContextActionSave()
        );

        const QString action = selectionContextActionForFunction(
            QStringLiteral(" custom-action ")
        );
        QCOMPARE(action, QStringLiteral("function:custom-action"));
        QVERIFY(isSelectionContextFunctionAction(action));
        QCOMPARE(
            selectionContextFunctionId(action),
            QStringLiteral("custom-action")
        );
        QVERIFY(selectionContextActionForFunction(QString()).isEmpty());
        QVERIFY(selectionContextFunctionId(QStringLiteral("function:")).isEmpty());
        QVERIFY(!isSelectionContextFunctionAction(QStringLiteral("function:")));
        QVERIFY(!isSelectionContextFunctionAction(QStringLiteral("function:a:b")));
        QVERIFY(selectionContextActionForFunction(QStringLiteral("a:b")).isEmpty());
        QVERIFY(selectionContextFunctionId(selectionContextActionCopy()).isEmpty());
        QCOMPARE(
            selectionContextActionTitle(selectionContextActionAiSearch()),
            QString::fromUtf8("AI 搜索")
        );
        QCOMPARE(
            selectionContextActionTitle(selectionContextActionTranslate()),
            QString::fromUtf8("翻译")
        );
        QCOMPARE(
            selectionContextActionTitle(selectionContextActionExplain()),
            QString::fromUtf8("解释")
        );
        QCOMPARE(
            selectionContextActionTitle(selectionContextActionSave()),
            QString::fromUtf8("保存")
        );
        QCOMPARE(
            selectionContextActionTitle(selectionContextActionCopy()),
            QString::fromUtf8("复制")
        );
        QCOMPARE(
            selectionContextMenuBlockApplication(),
            QStringLiteral("block-application")
        );
        QCOMPARE(
            selectionContextActionTitle(
                selectionContextMenuBlockApplication()
            ),
            QString::fromUtf8("在此应用中禁用")
        );
        QCOMPARE(
            selectionContextMenuOpenSettings(),
            QStringLiteral("open-settings")
        );
        QCOMPARE(
            selectionContextActionTitle(selectionContextMenuOpenSettings()),
            QString::fromUtf8("打开设置")
        );
        QVERIFY(selectionContextActionTitle(QStringLiteral("unknown")).isEmpty());
    }

    void normalizesSpeechProviderIds()
    {
        QCOMPARE(normalizeSpeechProvider(QStringLiteral("xfyun")), speechProviderXfyun());
        QCOMPARE(normalizeSpeechProvider(QStringLiteral("custom")), speechProviderCustom());
        QCOMPARE(normalizeSpeechProvider(QStringLiteral("unknown")), speechProviderBaidu());
        QCOMPARE(normalizeSpeechProvider(QString()), speechProviderBaidu());
    }

    void windowsSpeechProviderIsCatalogued()
    {
        QCOMPARE(
            speechProviderWindowsLocal(),
            QStringLiteral("windows-local")
        );
        QCOMPARE(
            normalizeSpeechProvider(QStringLiteral("WINDOWS-LOCAL")),
            speechProviderWindowsLocal()
        );
        QCOMPARE(
            speechProviderTitle(speechProviderWindowsLocal()),
            QString::fromUtf8("Windows 本地语音识别")
        );
        QCOMPARE(
            supportedSpeechProviderIds(),
            QStringList()
                << speechProviderBaidu()
                << speechProviderXfyun()
                << speechProviderCustom()
                << speechProviderWindowsLocal()
        );
    }

    void windowsSpeechLanguageCatalogIsStable()
    {
        QCOMPARE(
            windowsSpeechLanguageFollowWindows(),
            QStringLiteral("follow-windows")
        );
        QCOMPARE(
            windowsSpeechLanguageChinese(),
            QStringLiteral("zh-CN")
        );
        QCOMPARE(
            windowsSpeechLanguageEnglish(),
            QStringLiteral("en-US")
        );
        QCOMPARE(
            supportedWindowsSpeechLanguages(),
            QStringList()
                << windowsSpeechLanguageFollowWindows()
                << windowsSpeechLanguageChinese()
                << windowsSpeechLanguageEnglish()
        );
        QCOMPARE(
            windowsSpeechLanguageTitle(windowsSpeechLanguageFollowWindows()),
            QString::fromUtf8("跟随 Windows")
        );
        QCOMPARE(
            windowsSpeechLanguageTitle(windowsSpeechLanguageChinese()),
            QString::fromUtf8("简体中文")
        );
        QCOMPARE(
            windowsSpeechLanguageTitle(windowsSpeechLanguageEnglish()),
            QStringLiteral("English")
        );
    }

    void windowsSpeechLanguageNormalizesSafely()
    {
        QCOMPARE(
            normalizeWindowsSpeechLanguage(QString()),
            windowsSpeechLanguageFollowWindows()
        );
        QCOMPARE(
            normalizeWindowsSpeechLanguage(QStringLiteral("  ZH-cn  ")),
            windowsSpeechLanguageChinese()
        );
        QCOMPARE(
            normalizeWindowsSpeechLanguage(QStringLiteral(" En-uS ")),
            windowsSpeechLanguageEnglish()
        );
        QCOMPARE(
            normalizeWindowsSpeechLanguage(
                QStringLiteral(" FOLLOW-WINDOWS ")
            ),
            windowsSpeechLanguageFollowWindows()
        );
        QCOMPARE(
            normalizeWindowsSpeechLanguage(QStringLiteral("fr-FR")),
            windowsSpeechLanguageFollowWindows()
        );
    }

    void normalizesOcrEngineIds()
    {
        QCOMPARE(normalizeOcrEngine(QStringLiteral("rapid")), ocrEngineRapid());
        QCOMPARE(normalizeOcrEngine(QStringLiteral("windows")), ocrEngineWindows());
        QCOMPARE(normalizeOcrEngine(QStringLiteral("customCloud")), ocrEngineCustomCloud());
        QCOMPARE(normalizeOcrEngine(QStringLiteral("vision")), ocrEngineVision());
        QCOMPARE(normalizeOcrEngine(QStringLiteral("bad")), ocrEngineAutomatic());
    }

    void exposesStableProviderAndOcrCatalogs()
    {
        const QStringList speech = supportedSpeechProviderIds();
        QCOMPARE(
            speech,
            QStringList()
                << QStringLiteral("baidu")
                << QStringLiteral("xfyun")
                << QStringLiteral("custom")
                << QStringLiteral("windows-local")
        );
        QCOMPARE(
            speech.size(),
            QSet<QString>(speech.constBegin(), speech.constEnd()).size()
        );

        const QStringList ocr = supportedOcrEngineIds();
        QCOMPARE(
            ocr,
            QStringList()
                << QStringLiteral("automatic")
                << QStringLiteral("rapid")
                << QStringLiteral("windows")
                << QStringLiteral("customCloud")
                << QStringLiteral("vision")
        );
        QCOMPARE(
            ocr.size(),
            QSet<QString>(ocr.constBegin(), ocr.constEnd()).size()
        );
    }

    void keepsBuiltInFunctionDefaults()
    {
        QCOMPARE(defaultModelForFunction(QStringLiteral("ask")), QStringLiteral("deepseek-v4-pro"));
        QCOMPARE(defaultModelForFunction(QStringLiteral("dictate")), QStringLiteral("deepseek-v4-flash"));
        QCOMPARE(defaultOutputModeForFunction(QStringLiteral("dictate")), outputModeAutoWrite());
        QCOMPARE(defaultOutputModeForFunction(QStringLiteral("translate")), outputModePopup());
        QVERIFY(defaultUseSelectionForFunction(QStringLiteral("translate")));
        QVERIFY(defaultUseSelectionForFunction(QStringLiteral("ask")));
        QVERIFY(!defaultUseSelectionForFunction(QStringLiteral("dictate")));
        QVERIFY(defaultUseVoiceForFunction(QStringLiteral("dictate")));
        QVERIFY(!defaultUseVoiceForFunction(QStringLiteral("translate")));
    }

    void normalizesResultAndVocabularyOptions()
    {
        QCOMPARE(normalizeOutputMode(QStringLiteral("bad")), outputModePopup());
        QCOMPARE(normalizeResultTemplate(QStringLiteral("compare")), resultTemplateCompare());
        QCOMPARE(normalizeResultTemplate(QStringLiteral("bad")), resultTemplateSimple());
        QCOMPARE(normalizeVocabularyAddMode(QStringLiteral("ai")), vocabularyAddModeAi());
        QCOMPARE(normalizeVocabularyAddMode(QStringLiteral("manual")), vocabularyAddModeManual());
        QCOMPARE(normalizeVocabularyAddMode(QStringLiteral("bad")), vocabularyAddModeAsk());
    }

    void normalizesFloatingBarStylesAndResolvesInheritance()
    {
        QCOMPARE(
            floatingBarStyleStatusPill(),
            QStringLiteral("statusPill")
        );
        QCOMPARE(
            floatingBarStyleLiveTranscriptCard(),
            QStringLiteral("liveTranscriptCard")
        );
        QCOMPARE(floatingBarStyleInherit(), QStringLiteral("inherit"));
        QCOMPARE(
            normalizeGlobalFloatingBarStyle(QStringLiteral("bad")),
            floatingBarStyleStatusPill()
        );
        QCOMPARE(
            normalizeFunctionFloatingBarStyle(QString()),
            floatingBarStyleInherit()
        );
        QCOMPARE(
            resolveFloatingBarStyle(
                floatingBarStyleInherit(),
                floatingBarStyleLiveTranscriptCard()
            ),
            floatingBarStyleLiveTranscriptCard()
        );
        QCOMPARE(
            resolveFloatingBarStyle(
                floatingBarStyleStatusPill(),
                floatingBarStyleLiveTranscriptCard()
            ),
            floatingBarStyleStatusPill()
        );
        QCOMPARE(
            floatingBarStyleTitle(
                floatingBarStyleLiveTranscriptCard(),
                false
            ),
            QStringLiteral("实时文字卡片")
        );
        QCOMPARE(
            floatingBarStyleTitle(floatingBarStyleInherit(), true),
            QStringLiteral("跟随全局")
        );
    }
};

QTEST_MAIN(AppSettingsDefaultsTests)
#include "app_settings_defaults_tests.moc"
