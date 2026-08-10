#include <QtTest>

#include "../../src/ui/basic_settings_section.h"

#include <QCheckBox>

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
};

QTEST_MAIN(BasicSettingsSectionTests)

#include "basic_settings_section_tests.moc"
