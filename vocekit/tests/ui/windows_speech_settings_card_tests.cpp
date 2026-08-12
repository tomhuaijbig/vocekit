#include <QtTest>

#include "../../src/config/app_settings_defaults.h"
#include "../../src/ui/windows_speech_settings_card.h"

#include <QComboBox>
#include <QDir>
#include <QFontDatabase>
#include <QFontInfo>
#include <QImage>
#include <QLabel>
#include <QLayout>
#include <QPushButton>

class WindowsSpeechSettingsCardTests : public QObject
{
    Q_OBJECT

private slots:
    void exposesNormalizedLanguageCatalogAndScalableControls();
    void runsProbeOffTheUiThreadAndPublishesResult();
    void exposesLanguageSettingsForMissingRecognizer();
    void ignoresMissingRecognizerTextOutsideTheErrorCodeLine();
    void remainsUnclippedAcrossSupportedFontScales_data();
    void remainsUnclippedAcrossSupportedFontScales();
};

void WindowsSpeechSettingsCardTests::
exposesNormalizedLanguageCatalogAndScalableControls()
{
    WindowsSpeechSettingsCardCallbacks callbacks;
    WindowsSpeechSettingsCard card(callbacks);
    QComboBox *language = card.findChild<QComboBox *>(
        QStringLiteral("windowsSpeechLanguageBox")
    );
    QPushButton *test = card.findChild<QPushButton *>(
        QStringLiteral("windowsSpeechTestButton")
    );
    QPushButton *open = card.findChild<QPushButton *>(
        QStringLiteral("windowsSpeechOpenSettingsButton")
    );
    QVERIFY(language && test && open);
    QCOMPARE(language->count(), 3);
    QCOMPARE(language->itemData(0).toString(), windowsSpeechLanguageFollowWindows());
    QCOMPARE(language->itemData(1).toString(), windowsSpeechLanguageChinese());
    QCOMPARE(language->itemData(2).toString(), windowsSpeechLanguageEnglish());
    QCOMPARE(language->currentData().toString(), windowsSpeechLanguageFollowWindows());

    card.setLanguage(QStringLiteral(" EN-us "));
    QCOMPARE(card.language(), windowsSpeechLanguageEnglish());
    card.setLanguage(QStringLiteral("legacy-or-unknown"));
    QCOMPARE(card.language(), windowsSpeechLanguageFollowWindows());

    QVERIFY(test->minimumHeight() >= 40);
    QVERIFY(open->minimumHeight() >= 40);
    QVERIFY(test->minimumHeight() != test->maximumHeight());
    QVERIFY(open->minimumHeight() != open->maximumHeight());
}

void WindowsSpeechSettingsCardTests::runsProbeOffTheUiThreadAndPublishesResult()
{
    QString observedLanguage;
    WindowsSpeechSettingsCardCallbacks callbacks;
    callbacks.probe = [&observedLanguage](
        const QString &language,
        const CancellationToken &
    ) {
        observedLanguage = language;
        QThread::msleep(60);
        return QStringList()
            << QStringLiteral("OK")
            << QStringLiteral("resolvedLanguage=zh-CN")
            << QStringLiteral("installedLanguages=zh-CN,en-US");
    };
    WindowsSpeechSettingsCard card(callbacks);
    card.setLanguage(windowsSpeechLanguageChinese());
    QPushButton *test = card.findChild<QPushButton *>(
        QStringLiteral("windowsSpeechTestButton")
    );
    QLabel *result = card.findChild<QLabel *>(
        QStringLiteral("windowsSpeechProbeResult")
    );
    QVERIFY(test && result);

    QTest::mouseClick(test, Qt::LeftButton);
    QVERIFY(!test->isEnabled());
    QVERIFY(result->text().contains(QString::fromUtf8("正在")));
    QTRY_VERIFY_WITH_TIMEOUT(test->isEnabled(), 2000);
    QCOMPARE(observedLanguage, windowsSpeechLanguageChinese());
    QVERIFY(result->text().contains(QStringLiteral("zh-CN")));
    QVERIFY(result->text().contains(QStringLiteral("en-US")));
}

void WindowsSpeechSettingsCardTests::exposesLanguageSettingsForMissingRecognizer()
{
    int openCount = 0;
    WindowsSpeechSettingsCardCallbacks callbacks;
    callbacks.probe = [](
        const QString &language,
        const CancellationToken &
    ) {
        return QStringList()
            << QStringLiteral("RECOGNIZER_MISSING")
            << QStringLiteral("requestedLanguage=") + language;
    };
    callbacks.openWindowsLanguageSettings = [&openCount]() {
        ++openCount;
    };
    WindowsSpeechSettingsCard card(callbacks);
    card.setLanguage(windowsSpeechLanguageEnglish());
    QPushButton *test = card.findChild<QPushButton *>(
        QStringLiteral("windowsSpeechTestButton")
    );
    QPushButton *open = card.findChild<QPushButton *>(
        QStringLiteral("windowsSpeechOpenSettingsButton")
    );
    QLabel *result = card.findChild<QLabel *>(
        QStringLiteral("windowsSpeechProbeResult")
    );
    QVERIFY(test && open && result);
    QVERIFY(!open->isEnabled());

    QTest::mouseClick(test, Qt::LeftButton);
    QTRY_VERIFY_WITH_TIMEOUT(test->isEnabled(), 2000);
    QVERIFY(result->text().contains(QStringLiteral("RECOGNIZER_MISSING")));
    QVERIFY(result->text().contains(windowsSpeechLanguageEnglish()));
    QVERIFY(open->isEnabled());
    QTest::mouseClick(open, Qt::LeftButton);
    QCOMPARE(openCount, 1);
}

void WindowsSpeechSettingsCardTests::
ignoresMissingRecognizerTextOutsideTheErrorCodeLine()
{
    WindowsSpeechSettingsCardCallbacks callbacks;
    callbacks.probe = [](
        const QString &,
        const CancellationToken &
    ) {
        return QStringList()
            << QStringLiteral("PROGRAM_MISSING")
            << QStringLiteral("details mention RECOGNIZER_MISSING");
    };
    WindowsSpeechSettingsCard card(callbacks);
    QPushButton *test = card.findChild<QPushButton *>(
        QStringLiteral("windowsSpeechTestButton")
    );
    QPushButton *open = card.findChild<QPushButton *>(
        QStringLiteral("windowsSpeechOpenSettingsButton")
    );
    QVERIFY(test && open);

    QTest::mouseClick(test, Qt::LeftButton);
    QTRY_VERIFY_WITH_TIMEOUT(test->isEnabled(), 2000);
    QVERIFY(!open->isEnabled());
}

void WindowsSpeechSettingsCardTests::remainsUnclippedAcrossSupportedFontScales_data()
{
    QTest::addColumn<int>("percent");
    QTest::newRow("100-percent") << 100;
    QTest::newRow("125-percent") << 125;
    QTest::newRow("150-percent") << 150;
}

void WindowsSpeechSettingsCardTests::remainsUnclippedAcrossSupportedFontScales()
{
    QFETCH(int, percent);
    WindowsSpeechSettingsCardCallbacks callbacks;
    WindowsSpeechSettingsCard card(callbacks);
    const QList<QWidget *> widgets = card.findChildren<QWidget *>();
    for (QWidget *widget : widgets) {
        QFont font = widget->font();
        const qreal basePointSize = font.pointSizeF() > 0.0
            ? font.pointSizeF()
            : 10.0;
        font.setPointSizeF(basePointSize * percent / 100.0);
        widget->setFont(font);
    }
    card.layout()->invalidate();
    card.resize(760, card.sizeHint().height());
    card.show();
    QCoreApplication::processEvents();
    card.adjustSize();
    QCoreApplication::processEvents();

    const QList<QLabel *> labels = card.findChildren<QLabel *>();
    for (QLabel *label : labels) {
        QVERIFY2(label->height() >= label->sizeHint().height(),
                 qPrintable(label->objectName()));
    }
    QLabel *title = card.findChild<QLabel *>(
        QStringLiteral("windowsSpeechCardTitle")
    );
    QVERIFY(title);
    const QString chineseSample = QString::fromUtf8("本地语音识别");
    const QFontInfo fontInfo(title->font());
    QVERIFY2(!fontInfo.family().trimmed().isEmpty(), "No usable font family");
    const QFontMetrics metrics(title->font());
#if defined(Q_OS_WIN)
    QVERIFY2(metrics.inFont(chineseSample.at(0)), "CJK glyph is unavailable");
#else
    if (!metrics.inFont(chineseSample.at(0))) {
        QSKIP("This host has no CJK font; native Windows visual test is required");
    }
#endif
    QVERIFY2(
        metrics.width(chineseSample) >= metrics.height() * 2,
        "CJK text advance is not plausible"
    );
    const QList<QPushButton *> buttons = card.findChildren<QPushButton *>();
    for (QPushButton *button : buttons) {
        QVERIFY(button->height() >= button->sizeHint().height());
        QVERIFY(button->minimumHeight() != button->maximumHeight());
    }
    const QString outputDirectory = qEnvironmentVariableIsSet(
        "VOCEKIT_VISUAL_OUTPUT_DIR"
    )
        ? qgetenv("VOCEKIT_VISUAL_OUTPUT_DIR")
        : QDir::tempPath();
    QVERIFY(QDir().mkpath(outputDirectory));
    const QString screenshot = QDir(outputDirectory).filePath(
        QStringLiteral("windows-speech-card-%1.png").arg(percent)
    );
    const QPixmap rendered = card.grab();
    QVERIFY2(rendered.save(screenshot), qPrintable(screenshot));
    const QImage image = rendered.toImage().convertToFormat(
        QImage::Format_RGB32
    );
    const QRect titleRect(
        title->mapTo(&card, QPoint(0, 0)),
        title->size()
    );
    int darkTitlePixels = 0;
    for (int y = titleRect.top(); y <= titleRect.bottom(); ++y) {
        for (int x = titleRect.left(); x <= titleRect.right(); ++x) {
            const QColor pixel(image.pixel(x, y));
            if (pixel.red() < 120 && pixel.green() < 120
                && pixel.blue() < 120) {
                ++darkTitlePixels;
            }
        }
    }
    QVERIFY2(darkTitlePixels >= 20, "Rendered title contains no text pixels");
    card.hide();
}

QTEST_MAIN(WindowsSpeechSettingsCardTests)
#include "windows_speech_settings_card_tests.moc"
