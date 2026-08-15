#include <QtTest>

#include "../../src/domain/selection_context_actions.h"
#include "../../src/ui/basic_settings_section.h"
#include "../../src/ui/floating_bar_style_selector.h"
#include "../../src/ui/selection_context_action_editor.h"
#include "../../src/ui/selection_context_settings_card.h"

#include <QCheckBox>
#include <QLineEdit>
#include <QPointer>
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
    void refreshReloadsDynamicSelectionActionCatalogs();
    void unchangedCatalogRefreshPreservesActionEditorIdentityAndFocus();
    void refreshCatalogProviderMayDeleteSectionSynchronously();
    void selectionSettingsCallbackMayDeleteSectionSynchronously();
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
refreshReloadsDynamicSelectionActionCatalogs()
{
    BasicSettingsSnapshot current;
    SelectionContextActionCustomization search =
        current.selectionContext.actionCustomizations.value(
            selectionContextActionAiSearch());
    search.modelId = QStringLiteral("unknown-model");
    current.selectionContext.actionCustomizations.insert(
        selectionContextActionAiSearch(), search);
    SelectionContextActionCustomization save =
        current.selectionContext.actionCustomizations.value(
            selectionContextActionSave());
    save.vocabularyScopeId = QStringLiteral("unknown-scope");
    current.selectionContext.actionCustomizations.insert(
        selectionContextActionSave(), save);

    QVector<QPair<QString, QString>> models;
    models << qMakePair(QStringLiteral("Model A"), QStringLiteral("model-a"))
           << qMakePair(QStringLiteral("Removed Model"),
                        QStringLiteral("model-removed"));
    QVector<QPair<QString, QString>> scopes;
    scopes << qMakePair(QStringLiteral("Scope A"), QStringLiteral("scope-a"))
           << qMakePair(QStringLiteral("Removed Scope"),
                        QStringLiteral("scope-removed"));
    int modelReads = 0;
    int scopeReads = 0;
    BasicSettingsSection::Callbacks callbacks;
    callbacks.snapshotProvider = [&]() { return current; };
    callbacks.modelCatalogProvider = [&]() {
        ++modelReads;
        return models;
    };
    callbacks.vocabularyScopeCatalogProvider = [&]() {
        ++scopeReads;
        return scopes;
    };
    BasicSettingsSection section(BasicSettingsSection::General, callbacks);
    QCOMPARE(modelReads, 1);
    QCOMPARE(scopeReads, 1);

    models.clear();
    models << qMakePair(QStringLiteral("Model A Renamed"),
                        QStringLiteral("model-a"))
           << qMakePair(QStringLiteral("Model B"), QStringLiteral("model-b"))
           << qMakePair(QStringLiteral("Model B Duplicate"),
                        QStringLiteral("model-b"));
    scopes.clear();
    scopes << qMakePair(QStringLiteral("Scope A Renamed"),
                        QStringLiteral("scope-a"))
           << qMakePair(QStringLiteral("Scope B"), QStringLiteral("scope-b"))
           << qMakePair(QStringLiteral("Scope B Duplicate"),
                        QStringLiteral("scope-b"));

    section.refreshFromSettings();
    QCOMPARE(modelReads, 2);
    QCOMPARE(scopeReads, 2);
    SelectionContextSettingsCard *card =
        section.findChild<SelectionContextSettingsCard *>(
            QStringLiteral("selectionContextSettingsCard"));
    QVERIFY(card);
    SelectionContextActionEditor *refreshedSearch =
        card->findChild<SelectionContextActionEditor *>(
            QStringLiteral("selectionActionEditor_ai-search"));
    SelectionContextActionEditor *refreshedSave =
        card->findChild<SelectionContextActionEditor *>(
            QStringLiteral("selectionActionEditor_save"));
    SelectionContextActionEditor *refreshedTranslate =
        card->findChild<SelectionContextActionEditor *>(
            QStringLiteral("selectionActionEditor_translate"));
    QVERIFY(refreshedSearch);
    QVERIFY(refreshedSave);
    QVERIFY(refreshedTranslate);
    QComboBox *model = refreshedSearch->findChild<QComboBox *>(
        QStringLiteral("selectionActionModel"));
    QComboBox *scope = refreshedSave->findChild<QComboBox *>(
        QStringLiteral("selectionActionVocabularyScope"));
    QComboBox *targetLanguage = refreshedTranslate->findChild<QComboBox *>(
        QStringLiteral("selectionActionTargetLanguage"));
    QVERIFY(model);
    QVERIFY(scope);
    QVERIFY(targetLanguage);
    QCOMPARE(model->itemText(model->findData(QStringLiteral("model-a"))),
             QStringLiteral("Model A Renamed"));
    QVERIFY(model->findData(QStringLiteral("model-b")) >= 0);
    QCOMPARE(model->findData(QStringLiteral("model-removed")), -1);
    QCOMPARE(model->currentData().toString(), QStringLiteral("unknown-model"));
    QCOMPARE(model->findData(QStringLiteral("unknown-model")),
             model->currentIndex());
    int modelBCount = 0;
    for (int index = 0; index < model->count(); ++index) {
        if (model->itemData(index).toString() == QStringLiteral("model-b")) {
            ++modelBCount;
        }
    }
    QCOMPARE(modelBCount, 1);
    QCOMPARE(scope->itemText(scope->findData(QStringLiteral("scope-a"))),
             QStringLiteral("Scope A Renamed"));
    QVERIFY(scope->findData(QStringLiteral("scope-b")) >= 0);
    QCOMPARE(scope->findData(QStringLiteral("scope-removed")), -1);
    QCOMPARE(scope->currentData().toString(), QStringLiteral("unknown-scope"));
    QCOMPARE(scope->findData(QStringLiteral("unknown-scope")),
             scope->currentIndex());
    int scopeBCount = 0;
    for (int index = 0; index < scope->count(); ++index) {
        if (scope->itemData(index).toString() == QStringLiteral("scope-b")) {
            ++scopeBCount;
        }
    }
    QCOMPARE(scopeBCount, 1);
    QVERIFY(targetLanguage->findData(QString()) >= 0);
    QVERIFY(targetLanguage->findData(QStringLiteral("English")) >= 0);
    QCOMPARE(card->settings().actionCustomizations
             .value(selectionContextActionAiSearch()).modelId,
             QStringLiteral("unknown-model"));
    QCOMPARE(card->settings().actionCustomizations
             .value(selectionContextActionSave()).vocabularyScopeId,
             QStringLiteral("unknown-scope"));
}

void BasicSettingsSectionTests::
unchangedCatalogRefreshPreservesActionEditorIdentityAndFocus()
{
    BasicSettingsSnapshot current;
    const QVector<QPair<QString, QString>> models =
        QVector<QPair<QString, QString>>()
            << qMakePair(QStringLiteral("Model A"),
                         QStringLiteral("model-a"));
    const QVector<QPair<QString, QString>> scopes =
        QVector<QPair<QString, QString>>()
            << qMakePair(QStringLiteral("Scope A"),
                         QStringLiteral("scope-a"));
    BasicSettingsSection::Callbacks callbacks;
    callbacks.snapshotProvider = [&]() { return current; };
    callbacks.modelCatalogProvider = [&]() { return models; };
    callbacks.vocabularyScopeCatalogProvider = [&]() { return scopes; };
    BasicSettingsSection section(BasicSettingsSection::General, callbacks);
    SelectionContextSettingsCard *card =
        section.findChild<SelectionContextSettingsCard *>(
            QStringLiteral("selectionContextSettingsCard"));
    QVERIFY(card);
    SelectionContextActionEditor *search =
        card->findChild<SelectionContextActionEditor *>(
            QStringLiteral("selectionActionEditor_ai-search"));
    QVERIFY(search);
    QPointer<SelectionContextActionEditor> original(search);
    QLineEdit *name = search->findChild<QLineEdit *>(
        QStringLiteral("selectionActionDisplayName"));
    QVERIFY(name);
    section.show();
    QTRY_VERIFY(section.isVisible());
    name->setFocus();
    QTRY_VERIFY(name->hasFocus());

    section.refreshFromSettings();

    QVERIFY(original);
    QCOMPARE(card->findChild<SelectionContextActionEditor *>(
                 QStringLiteral("selectionActionEditor_ai-search")),
             original.data());
    QTRY_VERIFY(name->hasFocus());
}

void BasicSettingsSectionTests::
selectionSettingsCallbackMayDeleteSectionSynchronously()
{
    BasicSettingsSnapshot current;
    BasicSettingsSection *sectionPointer = nullptr;
    int saveCalls = 0;
    BasicSettingsSection::Callbacks callbacks;
    callbacks.snapshotProvider = [&]() { return current; };
    callbacks.applySnapshot = [&](const BasicSettingsSnapshot &) {
        delete sectionPointer;
        sectionPointer = nullptr;
    };
    callbacks.saveAndRefresh = [&]() { ++saveCalls; };
    sectionPointer = new BasicSettingsSection(
        BasicSettingsSection::General, callbacks);
    QPointer<BasicSettingsSection> alive(sectionPointer);
    SelectionContextSettingsCard *card =
        sectionPointer->findChild<SelectionContextSettingsCard *>(
            QStringLiteral("selectionContextSettingsCard"));
    QVERIFY(card);
    QCheckBox *enabled = card->findChild<QCheckBox *>(
        QStringLiteral("selectionContextEnabledToggle"));
    QVERIFY(enabled);

    enabled->click();

    QVERIFY(!alive);
    QCOMPARE(saveCalls, 0);
}

void BasicSettingsSectionTests::
refreshCatalogProviderMayDeleteSectionSynchronously()
{
    BasicSettingsSnapshot current;
    BasicSettingsSection *sectionPointer = nullptr;
    int modelReads = 0;
    int scopeReads = 0;
    BasicSettingsSection::Callbacks callbacks;
    callbacks.snapshotProvider = [&]() { return current; };
    callbacks.modelCatalogProvider = [&]() {
        ++modelReads;
        if (modelReads == 2) {
            delete sectionPointer;
            sectionPointer = nullptr;
        }
        return QVector<QPair<QString, QString>>();
    };
    callbacks.vocabularyScopeCatalogProvider = [&]() {
        ++scopeReads;
        return QVector<QPair<QString, QString>>();
    };
    sectionPointer = new BasicSettingsSection(
        BasicSettingsSection::General, callbacks);
    QPointer<BasicSettingsSection> alive(sectionPointer);
    QCOMPARE(modelReads, 1);
    QCOMPARE(scopeReads, 1);

    sectionPointer->refreshFromSettings();

    QVERIFY(!alive);
    QCOMPARE(modelReads, 2);
    QCOMPARE(scopeReads, 1);
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
