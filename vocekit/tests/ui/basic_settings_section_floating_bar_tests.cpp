#include <QtTest>

#include "../../src/ui/basic_settings_section.h"
#include "../../src/ui/floating_bar_style_selector.h"

#include <QAbstractButton>
#include <QCheckBox>
#include <QFont>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>

class BasicSettingsSectionFloatingBarTests : public QObject
{
    Q_OBJECT

private slots:
    void refreshesStyleAndFallbackWithoutSaving()
    {
        BasicSettingsSnapshot current;
        current.floatingBarStyle = QStringLiteral("statusPill");
        current.writeFailurePopupFallbackEnabled = true;
        int saveCount = 0;
        BasicSettingsSection::Callbacks callbacks;
        callbacks.snapshotProvider = [&current]() { return current; };
        callbacks.applySnapshot = [&current](
            const BasicSettingsSnapshot &next) { current = next; };
        callbacks.saveAndRefresh = [&saveCount]() { ++saveCount; };

        BasicSettingsSection voice(BasicSettingsSection::Voice, callbacks);
        BasicSettingsSection write(BasicSettingsSection::Write, callbacks);
        auto *selector = voice.findChild<FloatingBarStyleSelector *>(
            QStringLiteral("globalFloatingBarStyleSelector")
        );
        auto *fallback = write.findChild<QCheckBox *>(
            QStringLiteral("writeFailurePopupFallbackToggle")
        );
        QVERIFY(selector);
        QVERIFY(fallback);

        current.floatingBarStyle = QStringLiteral("liveTranscriptCard");
        current.writeFailurePopupFallbackEnabled = false;
        voice.refreshFromSettings();
        write.refreshFromSettings();

        QCOMPARE(selector->currentStyle(),
                 QStringLiteral("liveTranscriptCard"));
        QVERIFY(!fallback->isChecked());
        QCOMPARE(saveCount, 0);
    }

    void enlargedFontKeepsControlsUsableAndTextWrapped()
    {
        BasicSettingsSnapshot current;
        BasicSettingsSection::Callbacks callbacks;
        callbacks.snapshotProvider = [&current]() { return current; };

        BasicSettingsSection voice(BasicSettingsSection::Voice, callbacks);
        QFont large = voice.font();
        large.setPointSize(qMax(18, large.pointSize() + 6));
        voice.setFont(large);
        voice.resize(720, 640);
        voice.show();
        QTest::qWaitForWindowExposed(&voice);
        QCoreApplication::processEvents();

        auto *preview = voice.findChild<QPushButton *>(
            QStringLiteral("previewFloatingBarStyleButton")
        );
        auto *selector = voice.findChild<FloatingBarStyleSelector *>(
            QStringLiteral("globalFloatingBarStyleSelector")
        );
        QVERIFY(preview);
        QVERIFY(selector);
        QVERIFY(preview->height() >= preview->fontMetrics().height() + 16);
        for (QAbstractButton *card : selector->findChildren<QAbstractButton *>()) {
            QVERIFY(card->height() >= card->fontMetrics().height() + 16);
        }
        bool foundWrappedHint = false;
        for (QLabel *label : voice.findChildren<QLabel *>()) {
            if (label->text().contains(QString::fromUtf8("状态胶囊更简洁"))) {
                foundWrappedHint = label->wordWrap();
            }
        }
        QVERIFY(foundWrappedHint);
        QVERIFY(voice.findChild<QScrollArea *>());
    }
};

QTEST_MAIN(BasicSettingsSectionFloatingBarTests)

#include "basic_settings_section_floating_bar_tests.moc"
