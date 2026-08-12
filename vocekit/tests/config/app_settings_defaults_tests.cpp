#include <QtTest>

#include "../../src/config/app_settings_defaults.h"

class AppSettingsDefaultsTests : public QObject
{
    Q_OBJECT

private slots:
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
            normalizeWindowsSpeechLanguage(QStringLiteral("ZH-cn")),
            windowsSpeechLanguageChinese()
        );
        QCOMPARE(
            normalizeWindowsSpeechLanguage(QStringLiteral("en-US")),
            windowsSpeechLanguageEnglish()
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
