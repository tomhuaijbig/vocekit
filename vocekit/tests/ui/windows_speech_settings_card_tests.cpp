#include <QtTest>

#include "../../src/config/app_settings_defaults.h"
#include "../../src/ui/windows_speech_settings_card.h"

#include <QComboBox>
#include <QDir>
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
    const QList<QPushButton *> buttons = card.findChildren<QPushButton *>();
    for (QPushButton *button : buttons) {
        QVERIFY(button->height() >= button->sizeHint().height());
        QVERIFY(button->minimumHeight() != button->maximumHeight());
    }
    const QString screenshot = QDir::current().filePath(
        QStringLiteral("windows-speech-card-%1.png").arg(percent)
    );
    QVERIFY2(card.grab().save(screenshot), qPrintable(screenshot));
}

QTEST_MAIN(WindowsSpeechSettingsCardTests)
#include "windows_speech_settings_card_tests.moc"
