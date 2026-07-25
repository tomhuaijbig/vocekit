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

    void normalizesOcrEngineIds()
    {
        QCOMPARE(normalizeOcrEngine(QStringLiteral("rapid")), ocrEngineRapid());
        QCOMPARE(normalizeOcrEngine(QStringLiteral("windows")), ocrEngineWindows());
        QCOMPARE(normalizeOcrEngine(QStringLiteral("customCloud")), ocrEngineCustomCloud());
        QCOMPARE(normalizeOcrEngine(QStringLiteral("vision")), ocrEngineVision());
        QCOMPARE(normalizeOcrEngine(QStringLiteral("bad")), ocrEngineAutomatic());
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
};

QTEST_MAIN(AppSettingsDefaultsTests)
#include "app_settings_defaults_tests.moc"
