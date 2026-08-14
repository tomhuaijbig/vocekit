#include <QtTest>

#include "../../src/ui/basic_settings_section.h"
#include "../../src/ui/floating_bar_style_selector.h"
#include "../../src/ui/selection_context_settings_card.h"

#include <QCheckBox>
#include <QPushButton>

class BasicSettingsSectionTests : public QObject
{
    Q_OBJECT

private slots:
    void settingsPanelSavePassesTheWholeSelectionContextValue();
    void failedSaveRestoresPersistedValuesAndVisibleWidgets();
    void strongSelectionRemainsTheExistingGlobalCompatibilitySetting();
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

void BasicSettingsSectionTests::
settingsPanelSavePassesTheWholeSelectionContextValue()
{
    BasicSettingsSnapshot current;
    current.selectionContext.enabled = false;
    current.selectionContext.minimumTextLength = 2;
    BasicSettingsSnapshot applied;
    int saves = 0;
    BasicSettingsSection::Callbacks callbacks;
    callbacks.snapshotProvider = [&]() { return current; };
    callbacks.applySnapshot = [&](const BasicSettingsSnapshot &next) {
        applied = next;
        current = next;
    };
    callbacks.saveAndRefresh = [&]() { ++saves; };
    BasicSettingsSection section(BasicSettingsSection::General, callbacks);
    SelectionContextSettingsCard *card =
        section.findChild<SelectionContextSettingsCard *>(
            QStringLiteral("selectionContextSettingsCard")
        );
    QVERIFY(card);
    QCheckBox *enabled = card->findChild<QCheckBox *>(
        QStringLiteral("selectionContextEnabledToggle")
    );
    QVERIFY(enabled);
    enabled->click();
    QVERIFY(applied.selectionContext.enabled);
    QCOMPARE(applied.selectionContext.minimumTextLength, 2);
    QCOMPARE(saves, 1);
}

void BasicSettingsSectionTests::
failedSaveRestoresPersistedValuesAndVisibleWidgets()
{
    BasicSettingsSnapshot persisted;
    persisted.selectionContext.enabled = false;
    persisted.selectionContext.keyboardSelectionEnabled = true;
    persisted.selectionContext.minimumTextLength = 2;
    BasicSettingsSnapshot pending;
    bool hasPending = false;
    BasicSettingsSnapshot submitted;
    BasicSettingsSection *sectionPointer = nullptr;

    BasicSettingsSection::Callbacks callbacks;
    callbacks.snapshotProvider = [&]() {
        return hasPending ? pending : persisted;
    };
    callbacks.applySnapshot = [&](const BasicSettingsSnapshot &next) {
        pending = next;
        hasPending = true;
    };
    callbacks.saveAndRefresh = [&]() {
        submitted = pending;
        hasPending = false;
        if (sectionPointer) {
            sectionPointer->refreshFromSettings();
        }
    };
    BasicSettingsSection section(BasicSettingsSection::General, callbacks);
    sectionPointer = &section;
    SelectionContextSettingsCard *card =
        section.findChild<SelectionContextSettingsCard *>(
            QStringLiteral("selectionContextSettingsCard")
        );
    QVERIFY(card);
    QCheckBox *enabled = card->findChild<QCheckBox *>(
        QStringLiteral("selectionContextEnabledToggle")
    );
    QVERIFY(enabled);
    enabled->click();

    QVERIFY(submitted.selectionContext.enabled);
    QVERIFY(submitted.selectionContext.keyboardSelectionEnabled);
    QCOMPARE(submitted.selectionContext.minimumTextLength, 2);
    QVERIFY(!persisted.selectionContext.enabled);
    QVERIFY(!enabled->isChecked());
    QVERIFY(!card->settings().enabled);
}

void BasicSettingsSectionTests::
strongSelectionRemainsTheExistingGlobalCompatibilitySetting()
{
    BasicSettingsSnapshot current;
    current.strongSelectionEnabled = true;
    BasicSettingsSection::Callbacks callbacks;
    callbacks.snapshotProvider = [&]() { return current; };
    BasicSettingsSection section(BasicSettingsSection::General, callbacks);
    QCheckBox *globalToggle = section.findChild<QCheckBox *>(
        QStringLiteral("strongSelectionToggle")
    );
    SelectionContextSettingsCard *card =
        section.findChild<SelectionContextSettingsCard *>(
            QStringLiteral("selectionContextSettingsCard")
        );
    QVERIFY(globalToggle);
    QVERIFY(globalToggle->isChecked());
    QVERIFY(card);
    QCOMPARE(card->findChildren<QCheckBox *>(
        QStringLiteral("selectionContextStrongSelectionToggle")
    ).size(), 0);
}

QTEST_MAIN(BasicSettingsSectionTests)

#include "basic_settings_section_tests.moc"
