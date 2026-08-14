#include <QtTest>

#include "../../src/config/app_settings_data.h"
#include "../../src/config/app_settings_defaults.h"
#include "../../src/domain/selection_context_actions.h"

class AppSettingsDefaultsTests : public QObject
{
    Q_OBJECT

private slots:
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
        QCOMPARE(speech.size(), speech.toSet().size());

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
        QCOMPARE(ocr.size(), ocr.toSet().size());
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
