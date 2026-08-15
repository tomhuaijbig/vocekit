#include <QtTest>

#include "../../src/domain/selection_context_actions.h"
#include "../../src/ui/basic_settings_section.h"
#include "../../src/ui/floating_bar_style_selector.h"
#include "../../src/ui/selection_context_action_editor.h"
#include "../../src/ui/selection_context_settings_card.h"

#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>

class BasicSettingsSectionTests : public QObject
{
    Q_OBJECT

private slots:
    void settingsPanelSavePassesTheWholeSelectionContextValue();
    void failedSaveRestoresPersistedValuesAndVisibleWidgets();
    void failedSaveRestoresAllExpandedAndCollapsedActionEditors();
    void forwardsCatalogConfirmationAndWarningCallbacks();
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
failedSaveRestoresAllExpandedAndCollapsedActionEditors()
{
    BasicSettingsSnapshot persisted;
    SelectionContextActionCustomization persistedSearch =
        persisted.selectionContext.actionCustomizations.value(
            selectionContextActionAiSearch());
    persistedSearch.displayName = QString::fromUtf8("持久化搜索");
    persisted.selectionContext.actionCustomizations.insert(
        selectionContextActionAiSearch(), persistedSearch);
    SelectionContextActionCustomization persistedCopy =
        persisted.selectionContext.actionCustomizations.value(
            selectionContextActionCopy());
    persistedCopy.displayName = QString::fromUtf8("持久化复制");
    persisted.selectionContext.actionCustomizations.insert(
        selectionContextActionCopy(), persistedCopy);
    BasicSettingsSnapshot pending;
    bool hasPending = false;
    BasicSettingsSection *sectionPointer = nullptr;
    int saves = 0;

    BasicSettingsSection::Callbacks callbacks;
    callbacks.snapshotProvider = [&]() {
        return hasPending ? pending : persisted;
    };
    callbacks.applySnapshot = [&](const BasicSettingsSnapshot &next) {
        pending = next;
        hasPending = true;
    };
    callbacks.saveAndRefresh = [&]() {
        ++saves;
        hasPending = false;
        sectionPointer->refreshFromSettings();
    };
    BasicSettingsSection section(BasicSettingsSection::General, callbacks);
    sectionPointer = &section;
    SelectionContextSettingsCard *card =
        section.findChild<SelectionContextSettingsCard *>(
            QStringLiteral("selectionContextSettingsCard"));
    QVERIFY(card);
    SelectionContextActionEditor *search =
        card->findChild<SelectionContextActionEditor *>(
            QStringLiteral("selectionActionEditor_ai-search"));
    SelectionContextActionEditor *copy =
        card->findChild<SelectionContextActionEditor *>(
            QStringLiteral("selectionActionEditor_copy"));
    QVERIFY(search);
    QVERIFY(copy);
    search->findChild<QToolButton *>(QStringLiteral("selectionActionExpand"))
        ->click();
    QVERIFY(search->isExpanded());
    QVERIFY(!copy->isExpanded());
    search->findChild<QLineEdit *>(QStringLiteral("selectionActionDisplayName"))
        ->setText(QString::fromUtf8("未保存搜索"));
    QCoreApplication::processEvents();
    QCOMPARE(saves, 1);
    QCOMPARE(card->settings().actionCustomizations
             .value(selectionContextActionAiSearch()).displayName,
             persistedSearch.displayName);
    QCOMPARE(card->settings().actionCustomizations
             .value(selectionContextActionCopy()).displayName,
             persistedCopy.displayName);
    SelectionContextActionEditor *refreshedSearch =
        card->findChild<SelectionContextActionEditor *>(
            QStringLiteral("selectionActionEditor_ai-search"));
    SelectionContextActionEditor *refreshedCopy =
        card->findChild<SelectionContextActionEditor *>(
            QStringLiteral("selectionActionEditor_copy"));
    QVERIFY(refreshedSearch);
    QVERIFY(refreshedCopy);
    QCOMPARE(refreshedSearch->customization().displayName,
             persistedSearch.displayName);
    QCOMPARE(refreshedCopy->customization().displayName,
             persistedCopy.displayName);
}

void BasicSettingsSectionTests::
forwardsCatalogConfirmationAndWarningCallbacks()
{
    BasicSettingsSnapshot current;
    int confirms = 0;
    QStringList warnings;
    BasicSettingsSection::Callbacks callbacks;
    callbacks.snapshotProvider = [&]() { return current; };
    callbacks.modelCatalogProvider = []() {
        return QVector<QPair<QString, QString>>()
            << qMakePair(QStringLiteral("Model A"), QStringLiteral("model-a"));
    };
    callbacks.vocabularyScopeCatalogProvider = []() {
        return QVector<QPair<QString, QString>>()
            << qMakePair(QString::fromUtf8("全局词库"), QStringLiteral("__global"));
    };
    callbacks.confirmRestoreAllSelectionActions = [&]() {
        ++confirms;
        return false;
    };
    callbacks.selectionActionValidationWarning = [&](const QString &text) {
        warnings.append(text);
    };
    BasicSettingsSection section(BasicSettingsSection::General, callbacks);
    SelectionContextSettingsCard *card =
        section.findChild<SelectionContextSettingsCard *>(
            QStringLiteral("selectionContextSettingsCard"));
    QVERIFY(card);
    SelectionContextActionEditor *search =
        card->findChild<SelectionContextActionEditor *>(
            QStringLiteral("selectionActionEditor_ai-search"));
    QVERIFY(search);
    QComboBox *model = search->findChild<QComboBox *>(
        QStringLiteral("selectionActionModel"));
    QVERIFY(model);
    QVERIFY(model->findData(QStringLiteral("model-a")) >= 0);
    card->findChild<QPushButton *>(
        QStringLiteral("selectionContextRestoreAllButton"))->click();
    QCOMPARE(confirms, 1);

    for (const QString &id : defaultSelectionContextActionOrder()) {
        SelectionContextActionEditor *editor =
            card->findChild<SelectionContextActionEditor *>(
                QStringLiteral("selectionActionEditor_") + id);
        QVERIFY(editor);
        editor->findChild<QCheckBox *>(QStringLiteral("selectionActionVisible"))
            ->setChecked(id == selectionContextActionCopy());
        QCoreApplication::processEvents();
    }
    SelectionContextActionEditor *copy =
        card->findChild<SelectionContextActionEditor *>(
            QStringLiteral("selectionActionEditor_copy"));
    copy->findChild<QCheckBox *>(QStringLiteral("selectionActionVisible"))
        ->setChecked(false);
    QVERIFY(!warnings.isEmpty());
    QVERIFY(warnings.last().contains(QString::fromUtf8("至少保留一个")));
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
