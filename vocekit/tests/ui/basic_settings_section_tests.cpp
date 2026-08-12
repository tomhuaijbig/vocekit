#include <QtTest>

#include "../../src/ui/basic_settings_section.h"
#include "../../src/ui/floating_bar_style_selector.h"

#include <QCheckBox>
#include <QPushButton>

class BasicSettingsSectionTests : public QObject
{
    Q_OBJECT

private slots:
    void voiceSectionPersistsStreamingRecognitionToggle()
    {
        BasicSettingsSnapshot current;
        current.streamingSpeechRecognitionEnabled = true;
        BasicSettingsSnapshot applied;
        int applyCount = 0;
        int saveCount = 0;

        BasicSettingsSection::Callbacks callbacks;
        callbacks.snapshotProvider = [&current]() {
            return current;
        };
        callbacks.applySnapshot = [
            &applied,
            &applyCount
        ](const BasicSettingsSnapshot &snapshot) {
            applied = snapshot;
            ++applyCount;
        };
        callbacks.saveAndRefresh = [&saveCount]() {
            ++saveCount;
        };

        BasicSettingsSection section(
            BasicSettingsSection::Voice,
            callbacks
        );
        QCheckBox *toggle = section.findChild<QCheckBox *>(
            QStringLiteral("streamingSpeechRecognitionToggle")
        );

        QVERIFY(toggle);
        QVERIFY(toggle->isChecked());
        toggle->click();
        QCOMPARE(applyCount, 1);
        QCOMPARE(saveCount, 1);
        QVERIFY(!applied.streamingSpeechRecognitionEnabled);
    }

    void voiceSectionPersistsAndPreviewsSelectedFloatingStyle()
    {
        BasicSettingsSnapshot current;
        current.floatingBarStyle = QStringLiteral("statusPill");
        BasicSettingsSnapshot applied;
        int applyCount = 0;
        int saveCount = 0;
        QStringList previews;
        BasicSettingsSection::Callbacks callbacks;
        callbacks.snapshotProvider = [&current]() { return current; };
        callbacks.applySnapshot = [&](const BasicSettingsSnapshot &next) {
            applied = next;
            current = next;
            ++applyCount;
        };
        callbacks.saveAndRefresh = [&]() { ++saveCount; };
        callbacks.previewFloatingBarStyle = [&](const QString &style) {
            previews.append(style);
        };

        BasicSettingsSection section(BasicSettingsSection::Voice, callbacks);
        FloatingBarStyleSelector *selector =
            section.findChild<FloatingBarStyleSelector *>(
                QStringLiteral("globalFloatingBarStyleSelector")
            );
        QPushButton *preview = section.findChild<QPushButton *>(
            QStringLiteral("previewFloatingBarStyleButton")
        );
        QVERIFY(selector);
        QVERIFY(preview);
        selector->findChild<QAbstractButton *>(
            QStringLiteral("floatingBarStyleCard_liveTranscriptCard")
        )->click();
        QCOMPARE(applied.floatingBarStyle,
                 QStringLiteral("liveTranscriptCard"));
        QCOMPARE(applyCount, 1);
        QCOMPARE(saveCount, 1);
        preview->click();
        QCOMPARE(
            previews,
            QStringList() << QStringLiteral("liveTranscriptCard")
        );
    }

    void writeSectionDefaultsOnAndPersistsOff()
    {
        BasicSettingsSnapshot current;
        current.writeFailurePopupFallbackEnabled = true;
        BasicSettingsSnapshot applied;
        int saveCount = 0;
        BasicSettingsSection::Callbacks callbacks;
        callbacks.snapshotProvider = [&]() { return current; };
        callbacks.applySnapshot = [&](const BasicSettingsSnapshot &next) {
            applied = next;
            current = next;
        };
        callbacks.saveAndRefresh = [&]() { ++saveCount; };

        BasicSettingsSection section(BasicSettingsSection::Write, callbacks);
        QCheckBox *toggle = section.findChild<QCheckBox *>(
            QStringLiteral("writeFailurePopupFallbackToggle")
        );
        QVERIFY(toggle);
        QVERIFY(toggle->isChecked());
        toggle->click();
        QVERIFY(!applied.writeFailurePopupFallbackEnabled);
        QCOMPARE(saveCount, 1);
    }
};

QTEST_MAIN(BasicSettingsSectionTests)

#include "basic_settings_section_tests.moc"
