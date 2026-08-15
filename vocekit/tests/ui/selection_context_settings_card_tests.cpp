#include <QtTest>

#include "../../src/domain/selection_context_actions.h"
#include "../../src/ui/selection_context_action_editor.h"
#include "../../src/ui/selection_context_settings_card.h"

#include <QAbstractItemModel>
#include <QCheckBox>
#include <QComboBox>
#include <QFileInfo>
#include <QFontInfo>
#include <QLabel>
#include <QImage>
#include <QLineEdit>
#include <QListWidget>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

template <typename T>
T *required(QWidget *root, const char *name)
{
    T *widget = root->findChild<T *>(QString::fromLatin1(name));
    if (!widget) {
        qFatal("missing widget: %s", name);
    }
    return widget;
}

SelectionContextActionEditor *actionEditor(
    QWidget *root,
    const QString &actionId)
{
    return root->findChild<SelectionContextActionEditor *>(
        QStringLiteral("selectionActionEditor_") + actionId
    );
}

bool sameCustomization(
    const SelectionContextActionCustomization &left,
    const SelectionContextActionCustomization &right)
{
    return left.displayName == right.displayName
        && left.visible == right.visible
        && left.modelId == right.modelId
        && left.promptOverride == right.promptOverride
        && left.targetLanguage == right.targetLanguage
        && left.vocabularyScopeId == right.vocabularyScopeId
        && left.copyMode == right.copyMode;
}

SelectionContextActionEditor::Catalogs testCatalogs()
{
    SelectionContextActionEditor::Catalogs catalogs;
    catalogs.models
        << qMakePair(QStringLiteral("Model Alpha"), QStringLiteral("alpha"))
        << qMakePair(QStringLiteral("Model Beta"), QStringLiteral("beta"));
    catalogs.vocabularyScopes
        << qMakePair(QString::fromUtf8("全局词库"), QStringLiteral("__global"))
        << qMakePair(QString::fromUtf8("写作功能"), QStringLiteral("writing"));
    catalogs.targetLanguages
        << qMakePair(QString::fromUtf8("跟随全局目标语言"), QString())
        << qMakePair(QStringLiteral("English"), QStringLiteral("English"));
    return catalogs;
}

QString visualOutputPath(const QString &fileName, QTemporaryDir *fallback)
{
    const QString configured = QString::fromLocal8Bit(
        qgetenv("VOCEKIT_SELECTION_CONTEXT_VISUAL_OUTPUT_DIR")
    ).trimmed();
    const QString root = configured.isEmpty()
        ? fallback->path()
        : configured;
    QDir().mkpath(root);
    return QDir(root).filePath(fileName);
}

bool hasDarkForegroundPixels(const QPixmap &pixmap)
{
    const QImage image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
    int dark = 0;
    for (int y = 0; y < image.height(); ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const QRgb pixel = line[x];
            if (qAlpha(pixel) > 100
                && qRed(pixel) < 120
                && qGreen(pixel) < 130
                && qBlue(pixel) < 150) {
                if (++dark >= 20) {
                    return true;
                }
            }
        }
    }
    return false;
}

} // namespace

class SelectionContextSettingsCardTests : public QObject
{
    Q_OBJECT

private slots:
    void cardLoadsAndReturnsEveryTypedSetting();
    void actionRowsFollowCatalogAndDragReorderPersistsStableIds();
    void blockedApplicationsNormalizeOneExecutablePerLine();
    void pauseDurationAndKeyboardObservationAreIndependent();
    void acceptedNetworkConsentCanBeResetForTheNextModelAction();
    void strongSelectionRemainsAnExplanationInsteadOfADuplicateToggle();
    void buildsFiveEditorsFromStableOrderAndNamesNeverChangeIdentity();
    void onlyOneActionEditorIsExpandedAtATime();
    void refusesToHideLastVisibleActionImmediatelyAndAfterQueuedDelivery();
    void restoreOneDoesNotChangeOtherActionsOrToolbarFields();
    void restoreAllRequiresConfirmationAndKeepsToolbarFieldsAndOrder();
    void catalogsReachApplicableEditorsAndUnknownCurrentModelStaysVisible();
    void dragOrderStillContainsEveryBuiltInIdExactlyOnceWithDuplicateNames();
    void refreshInvalidatesQueuedEditorChanges();
    void successfulSaveEchoKeepsEditorAndNewerQueuedChange();
    void simultaneousEditorSaveEchoPreservesBothPendingChanges();
    void buttonsAndChineseLabelsDoNotClipAt100_125_150_200Percent();
    void smallWindowAndExpandedEditorsRemainScrollableAndReachable();
};

void SelectionContextSettingsCardTests::cardLoadsAndReturnsEveryTypedSetting()
{
    SelectionContextSettings initial;
    initial.enabled = true;
    initial.keyboardSelectionEnabled = false;
    initial.minimumTextLength = 17;
    initial.closeOnOutsideClick = false;
    initial.pinEnabled = false;
    initial.networkConsentAcknowledged = true;
    initial.pauseMinutes = 75;
    initial.actionOrder = QStringList()
        << selectionContextActionCopy()
        << selectionContextActionSave()
        << selectionContextActionExplain()
        << selectionContextActionTranslate()
        << selectionContextActionAiSearch();
    initial.blockedApplications = QStringList()
        << QStringLiteral("chrome.exe")
        << QStringLiteral("word.exe");

    QVector<SelectionContextSettings> changes;
    SelectionContextSettingsCard::Callbacks callbacks;
    callbacks.settingsChanged = [&](const SelectionContextSettings &value) {
        changes.append(value);
    };
    SelectionContextSettingsCard card(initial, callbacks);

    QCOMPARE(required<QCheckBox>(&card, "selectionContextEnabledToggle")->isChecked(), true);
    QCOMPARE(required<QCheckBox>(&card, "selectionContextKeyboardToggle")->isChecked(), false);
    QCOMPARE(required<QSpinBox>(&card, "selectionContextMinimumLengthSpin")->value(), 17);
    QCOMPARE(required<QCheckBox>(&card, "selectionContextCloseOutsideToggle")->isChecked(), false);
    QCOMPARE(required<QCheckBox>(&card, "selectionContextPinToggle")->isChecked(), false);
    QCOMPARE(required<QSpinBox>(&card, "selectionContextPauseMinutesSpin")->value(), 75);
    QCOMPARE(card.settings().actionOrder, initial.actionOrder);
    QCOMPARE(card.settings().blockedApplications, initial.blockedApplications);

    required<QCheckBox>(&card, "selectionContextKeyboardToggle")->click();
    QVERIFY(!changes.isEmpty());
    QCOMPARE(changes.last().keyboardSelectionEnabled, true);
    QCOMPARE(changes.last().minimumTextLength, 17);
}

void SelectionContextSettingsCardTests::
actionRowsFollowCatalogAndDragReorderPersistsStableIds()
{
    SelectionContextSettings settings;
    settings.actionOrder = QStringList()
        << selectionContextActionCopy()
        << selectionContextActionAiSearch()
        << selectionContextActionTranslate()
        << selectionContextActionExplain()
        << selectionContextActionSave();
    SelectionContextSettingsCard card(settings);
    QListWidget *list = required<QListWidget>(
        &card,
        "selectionContextActionList"
    );
    QCOMPARE(list->dragDropMode(), QAbstractItemView::InternalMove);
    QCOMPARE(list->count(), defaultSelectionContextActionOrder().size());
    for (int i = 0; i < list->count(); ++i) {
        QCOMPARE(
            list->item(i)->data(Qt::UserRole).toString(),
            settings.actionOrder.at(i)
        );
        QVERIFY(!list->item(i)->text().trimmed().isEmpty());
    }

    QListWidgetItem *last = list->takeItem(list->count() - 1);
    list->insertItem(0, last);
    QCoreApplication::processEvents();
    QCOMPARE(card.settings().actionOrder.first(), selectionContextActionSave());
}

void SelectionContextSettingsCardTests::
blockedApplicationsNormalizeOneExecutablePerLine()
{
    SelectionContextSettingsCard card((SelectionContextSettings()));
    QTextEdit *blocked = required<QTextEdit>(
        &card,
        "selectionContextBlockedApplicationsEdit"
    );
    blocked->setPlainText(QString::fromUtf8(
        " C:/Program Files/Browser/Chrome.EXE \n"
        "chrome.exe\n\nWORD.EXE\nD:/Office/WORD.EXE\n"
    ));
    QCoreApplication::processEvents();
    QCOMPARE(
        card.settings().blockedApplications,
        QStringList() << QStringLiteral("chrome.exe")
                      << QStringLiteral("word.exe")
    );
}

void SelectionContextSettingsCardTests::
pauseDurationAndKeyboardObservationAreIndependent()
{
    SelectionContextSettings settings;
    settings.keyboardSelectionEnabled = true;
    settings.pauseMinutes = 30;
    SelectionContextSettingsCard card(settings);
    QCheckBox *keyboard = required<QCheckBox>(
        &card,
        "selectionContextKeyboardToggle"
    );
    QSpinBox *pause = required<QSpinBox>(
        &card,
        "selectionContextPauseMinutesSpin"
    );
    keyboard->click();
    pause->setValue(240);
    QCOMPARE(card.settings().keyboardSelectionEnabled, false);
    QCOMPARE(card.settings().pauseMinutes, 240);
}

void SelectionContextSettingsCardTests::
acceptedNetworkConsentCanBeResetForTheNextModelAction()
{
    SelectionContextSettings settings;
    settings.networkConsentAcknowledged = true;
    QVector<SelectionContextSettings> changes;
    SelectionContextSettingsCard::Callbacks callbacks;
    callbacks.settingsChanged = [&](const SelectionContextSettings &value) {
        changes.append(value);
    };
    SelectionContextSettingsCard card(settings, callbacks);
    QPushButton *reset = required<QPushButton>(
        &card,
        "selectionContextResetConsentButton"
    );
    QVERIFY(reset->isVisibleTo(&card));
    reset->click();
    QVERIFY(!card.settings().networkConsentAcknowledged);
    QVERIFY(!changes.isEmpty());
    QVERIFY(!changes.last().networkConsentAcknowledged);
    QVERIFY(reset->isHidden());
}

void SelectionContextSettingsCardTests::
strongSelectionRemainsAnExplanationInsteadOfADuplicateToggle()
{
    int details = 0;
    SelectionContextSettingsCard::Callbacks callbacks;
    callbacks.showStrongSelectionDetails = [&]() { ++details; };
    SelectionContextSettingsCard card(SelectionContextSettings(), callbacks);
    QPushButton *link = required<QPushButton>(
        &card,
        "selectionContextStrongSelectionLink"
    );
    QCOMPARE(
        card.findChildren<QCheckBox *>(
            QStringLiteral("selectionContextStrongSelectionToggle")
        ).size(),
        0
    );
    link->click();
    QCOMPARE(details, 1);
}

void SelectionContextSettingsCardTests::
buildsFiveEditorsFromStableOrderAndNamesNeverChangeIdentity()
{
    SelectionContextSettings settings;
    settings.actionOrder = QStringList()
        << selectionContextActionCopy()
        << selectionContextActionSave()
        << selectionContextActionTranslate()
        << selectionContextActionAiSearch()
        << selectionContextActionExplain();
    SelectionContextSettingsCard card(settings);
    QListWidget *list = required<QListWidget>(
        &card, "selectionContextActionList");
    QCOMPARE(list->count(), 5);
    for (int row = 0; row < list->count(); ++row) {
        const QString id = list->item(row)->data(Qt::UserRole).toString();
        QCOMPARE(id, settings.actionOrder.at(row));
        SelectionContextActionEditor *editor = actionEditor(&card, id);
        QVERIFY2(editor, qPrintable(id));
        QCOMPARE(list->itemWidget(list->item(row)), editor);
    }

    SelectionContextActionEditor *copy = actionEditor(
        &card, selectionContextActionCopy());
    required<QLineEdit>(copy, "selectionActionDisplayName")
        ->setText(QString::fromUtf8("同名动作"));
    QCOMPARE(list->item(0)->data(Qt::UserRole).toString(),
             selectionContextActionCopy());
    QCOMPARE(card.settings().actionOrder, settings.actionOrder);
}

void SelectionContextSettingsCardTests::
onlyOneActionEditorIsExpandedAtATime()
{
    SelectionContextSettingsCard card((SelectionContextSettings()));
    SelectionContextActionEditor *search = actionEditor(
        &card, selectionContextActionAiSearch());
    SelectionContextActionEditor *copy = actionEditor(
        &card, selectionContextActionCopy());
    QVERIFY(search);
    QVERIFY(copy);
    required<QToolButton>(search, "selectionActionExpand")->click();
    QVERIFY(search->isExpanded());
    QVERIFY(!copy->isExpanded());
    QVERIFY(required<QLineEdit>(search, "selectionActionDisplayName")
            ->isVisibleTo(search));
    required<QToolButton>(copy, "selectionActionExpand")->click();
    QVERIFY(!search->isExpanded());
    QVERIFY(copy->isExpanded());
    QVERIFY(required<QLineEdit>(search, "selectionActionDisplayName")
            ->isVisibleTo(search));
}

void SelectionContextSettingsCardTests::
refusesToHideLastVisibleActionImmediatelyAndAfterQueuedDelivery()
{
    SelectionContextSettings settings;
    for (const QString &id : defaultSelectionContextActionOrder()) {
        SelectionContextActionCustomization item =
            settings.actionCustomizations.value(id);
        item.visible = id == selectionContextActionCopy();
        settings.actionCustomizations.insert(id, item);
    }
    QStringList warnings;
    QVector<SelectionContextSettings> changes;
    SelectionContextSettingsCard::Callbacks callbacks;
    callbacks.validationWarning = [&](const QString &text) {
        warnings.append(text);
    };
    callbacks.settingsChanged = [&](const SelectionContextSettings &value) {
        changes.append(value);
    };
    SelectionContextSettingsCard card(settings, callbacks);
    SelectionContextActionEditor *copy = actionEditor(
        &card, selectionContextActionCopy());
    QCheckBox *visible = required<QCheckBox>(copy, "selectionActionVisible");
    visible->setChecked(false);

    QVERIFY(card.settings().actionCustomizations
            .value(selectionContextActionCopy()).visible);
    QVERIFY(visible->isChecked());
    QVERIFY(!warnings.isEmpty());
    QVERIFY(warnings.last().contains(QString::fromUtf8("至少保留一个")));
    QVERIFY(changes.isEmpty());
    QCoreApplication::processEvents();
    QVERIFY(card.settings().actionCustomizations
            .value(selectionContextActionCopy()).visible);
    QVERIFY(changes.isEmpty());
}

void SelectionContextSettingsCardTests::
restoreOneDoesNotChangeOtherActionsOrToolbarFields()
{
    SelectionContextSettings settings;
    settings.enabled = true;
    settings.pauseMinutes = 91;
    settings.actionOrder = QStringList()
        << selectionContextActionCopy()
        << selectionContextActionAiSearch()
        << selectionContextActionTranslate()
        << selectionContextActionExplain()
        << selectionContextActionSave();
    SelectionContextActionCustomization search =
        settings.actionCustomizations.value(selectionContextActionAiSearch());
    search.displayName = QString::fromUtf8("我的搜索");
    search.modelId = QStringLiteral("alpha");
    search.promptOverride = QStringLiteral("custom prompt");
    settings.actionCustomizations.insert(selectionContextActionAiSearch(), search);
    SelectionContextActionCustomization copy =
        settings.actionCustomizations.value(selectionContextActionCopy());
    copy.displayName = QString::fromUtf8("另一个复制");
    settings.actionCustomizations.insert(selectionContextActionCopy(), copy);
    QVector<SelectionContextSettings> changes;
    SelectionContextSettingsCard::Callbacks callbacks;
    callbacks.settingsChanged = [&](const SelectionContextSettings &value) {
        changes.append(value);
    };
    SelectionContextSettingsCard card(settings, callbacks);
    required<QPushButton>(
        actionEditor(&card, selectionContextActionAiSearch()),
        "selectionActionRestore")->click();

    QCOMPARE(changes.size(), 1);
    const SelectionContextSettings restored = changes.last();
    const SelectionContextActionCustomization defaults =
        defaultSelectionContextActionCustomizations()
            .value(selectionContextActionAiSearch());
    QVERIFY(sameCustomization(
        restored.actionCustomizations.value(selectionContextActionAiSearch()),
        defaults));
    QVERIFY(sameCustomization(
        restored.actionCustomizations.value(selectionContextActionCopy()), copy));
    QCOMPARE(restored.enabled, true);
    QCOMPARE(restored.pauseMinutes, 91);
    QCOMPARE(restored.actionOrder, settings.actionOrder);
}

void SelectionContextSettingsCardTests::
restoreAllRequiresConfirmationAndKeepsToolbarFieldsAndOrder()
{
    SelectionContextSettings settings;
    settings.enabled = true;
    settings.keyboardSelectionEnabled = false;
    settings.closeOnOutsideClick = false;
    settings.pinEnabled = false;
    settings.networkConsentAcknowledged = true;
    settings.minimumTextLength = 31;
    settings.pauseMinutes = 72;
    settings.blockedApplications << QStringLiteral("secret.exe");
    settings.actionOrder = QStringList()
        << selectionContextActionCopy()
        << selectionContextActionSave()
        << selectionContextActionExplain()
        << selectionContextActionTranslate()
        << selectionContextActionAiSearch();
    for (const QString &id : defaultSelectionContextActionOrder()) {
        SelectionContextActionCustomization item =
            settings.actionCustomizations.value(id);
        item.displayName = QStringLiteral("custom-") + id;
        item.visible = id != selectionContextActionExplain();
        item.modelId = QStringLiteral("alpha");
        item.promptOverride = QStringLiteral("prompt");
        item.targetLanguage = QStringLiteral("English");
        item.vocabularyScopeId = QStringLiteral("writing");
        item.copyMode = QStringLiteral("trim");
        settings.actionCustomizations.insert(id, item);
    }
    bool confirm = false;
    int confirmations = 0;
    QVector<SelectionContextSettings> changes;
    SelectionContextSettingsCard::Callbacks callbacks;
    callbacks.confirmRestoreAllSelectionActions = [&]() {
        ++confirmations;
        return confirm;
    };
    callbacks.settingsChanged = [&](const SelectionContextSettings &value) {
        changes.append(value);
    };
    SelectionContextSettingsCard card(settings, callbacks);
    QPushButton *restoreAll = required<QPushButton>(
        &card, "selectionContextRestoreAllButton");
    restoreAll->click();
    QCOMPARE(confirmations, 1);
    QVERIFY(changes.isEmpty());
    QVERIFY(sameCustomization(
        card.settings().actionCustomizations.value(selectionContextActionCopy()),
        settings.actionCustomizations.value(selectionContextActionCopy())));

    confirm = true;
    restoreAll->click();
    QCOMPARE(confirmations, 2);
    QCOMPARE(changes.size(), 1);
    const SelectionContextSettings restored = changes.last();
    const SelectionContextActionCustomizationMap defaults =
        defaultSelectionContextActionCustomizations();
    QListWidget *actions = required<QListWidget>(
        &card, "selectionContextActionList");
    for (const QString &id : defaultSelectionContextActionOrder()) {
        QVERIFY2(sameCustomization(
            restored.actionCustomizations.value(id), defaults.value(id)),
            qPrintable(id));
        QListWidgetItem *listItem = nullptr;
        for (int row = 0; row < actions->count(); ++row) {
            if (actions->item(row)->data(Qt::UserRole).toString() == id) {
                listItem = actions->item(row);
                break;
            }
        }
        QVERIFY2(listItem, qPrintable(id));
        SelectionContextActionEditor *editor = actionEditor(&card, id);
        QVERIFY2(editor, qPrintable(id));
        QCOMPARE(listItem->text(), defaults.value(id).displayName);
        QCOMPARE(listItem->data(Qt::AccessibleTextRole).toString(),
                 defaults.value(id).displayName);
        QCOMPARE(listItem->sizeHint(), editor->sizeHint());
    }
    QCOMPARE(restored.enabled, settings.enabled);
    QCOMPARE(restored.keyboardSelectionEnabled,
             settings.keyboardSelectionEnabled);
    QCOMPARE(restored.closeOnOutsideClick, settings.closeOnOutsideClick);
    QCOMPARE(restored.pinEnabled, settings.pinEnabled);
    QCOMPARE(restored.networkConsentAcknowledged,
             settings.networkConsentAcknowledged);
    QCOMPARE(restored.minimumTextLength, settings.minimumTextLength);
    QCOMPARE(restored.pauseMinutes, settings.pauseMinutes);
    QCOMPARE(restored.blockedApplications, settings.blockedApplications);
    QCOMPARE(restored.actionOrder, settings.actionOrder);
}

void SelectionContextSettingsCardTests::
catalogsReachApplicableEditorsAndUnknownCurrentModelStaysVisible()
{
    SelectionContextSettings settings;
    SelectionContextActionCustomization search =
        settings.actionCustomizations.value(selectionContextActionAiSearch());
    search.modelId = QStringLiteral("retired-model-that-must-remain");
    settings.actionCustomizations.insert(selectionContextActionAiSearch(), search);
    SelectionContextSettingsCard card(settings);
    card.setCatalogs(testCatalogs());

    SelectionContextActionEditor *searchEditor = actionEditor(
        &card, selectionContextActionAiSearch());
    QComboBox *model = required<QComboBox>(searchEditor, "selectionActionModel");
    QCOMPARE(model->currentData().toString(), search.modelId);
    QVERIFY(model->currentText().contains(QString::fromUtf8("不可用")));
    QVERIFY(model->findData(QStringLiteral("alpha")) >= 0);
    QCOMPARE(model->itemText(model->findData(QStringLiteral("alpha"))),
             QStringLiteral("Model Alpha"));
    QComboBox *scope = required<QComboBox>(
        actionEditor(&card, selectionContextActionSave()),
        "selectionActionVocabularyScope");
    QVERIFY(scope->findData(QStringLiteral("writing")) >= 0);
    QVERIFY(scope->findData(QStringLiteral("__all")) < 0);
    QComboBox *language = required<QComboBox>(
        actionEditor(&card, selectionContextActionTranslate()),
        "selectionActionTargetLanguage");
    QVERIFY(language->findData(QStringLiteral("English")) >= 0);
}

void SelectionContextSettingsCardTests::
dragOrderStillContainsEveryBuiltInIdExactlyOnceWithDuplicateNames()
{
    SelectionContextSettings settings;
    for (const QString &id : defaultSelectionContextActionOrder()) {
        SelectionContextActionCustomization item =
            settings.actionCustomizations.value(id);
        item.displayName = QString::fromUtf8("完全相同");
        settings.actionCustomizations.insert(id, item);
    }
    QVector<SelectionContextSettings> changes;
    SelectionContextSettingsCard::Callbacks callbacks;
    callbacks.settingsChanged = [&](const SelectionContextSettings &value) {
        changes.append(value);
    };
    SelectionContextSettingsCard card(settings, callbacks);
    QListWidget *list = required<QListWidget>(
        &card, "selectionContextActionList");
    QListWidgetItem *last = list->takeItem(list->count() - 1);
    list->insertItem(0, last);
    QCoreApplication::processEvents();
    QVERIFY(!changes.isEmpty());
    const QStringList order = changes.last().actionOrder;
    QCOMPARE(order.size(), 5);
    for (const QString &id : defaultSelectionContextActionOrder()) {
        QCOMPARE(order.count(id), 1);
    }
    QCOMPARE(order.first(), selectionContextActionCopy());
}

void SelectionContextSettingsCardTests::
refreshInvalidatesQueuedEditorChanges()
{
    SelectionContextSettings first;
    QVector<SelectionContextSettings> changes;
    SelectionContextSettingsCard::Callbacks callbacks;
    callbacks.settingsChanged = [&](const SelectionContextSettings &value) {
        changes.append(value);
    };
    SelectionContextSettingsCard card(first, callbacks);
    SelectionContextActionEditor *search = actionEditor(
        &card, selectionContextActionAiSearch());
    required<QLineEdit>(search, "selectionActionDisplayName")
        ->setText(QString::fromUtf8("排队中的旧名称"));

    SelectionContextSettings persisted = first;
    SelectionContextActionCustomization persistedSearch =
        persisted.actionCustomizations.value(selectionContextActionAiSearch());
    persistedSearch.displayName = QString::fromUtf8("持久化名称");
    persisted.actionCustomizations.insert(
        selectionContextActionAiSearch(), persistedSearch);
    card.setSettings(persisted);
    QCoreApplication::processEvents();
    QVERIFY(changes.isEmpty());
    QCOMPARE(card.settings().actionCustomizations
             .value(selectionContextActionAiSearch()).displayName,
             persistedSearch.displayName);
}

void SelectionContextSettingsCardTests::
successfulSaveEchoKeepsEditorAndNewerQueuedChange()
{
    QStringList deliveredNames;
    SelectionContextSettingsCard *cardPointer = nullptr;
    SelectionContextSettingsCard::Callbacks callbacks;
    callbacks.settingsChanged = [&](const SelectionContextSettings &value) {
        deliveredNames.append(value.actionCustomizations
            .value(selectionContextActionAiSearch()).displayName);
        cardPointer->setSettings(value);
    };
    SelectionContextSettingsCard card(
        SelectionContextSettings(), callbacks);
    cardPointer = &card;
    SelectionContextActionEditor *search = actionEditor(
        &card, selectionContextActionAiSearch());
    QPointer<SelectionContextActionEditor> original(search);
    QLineEdit *name = required<QLineEdit>(
        search, "selectionActionDisplayName");
    card.show();
    QTRY_VERIFY(card.isVisible());
    name->setFocus();
    QTRY_VERIFY(name->hasFocus());
    name->setText(QStringLiteral("A"));
    name->setText(QStringLiteral("B"));

    const QStringList expectedNames = QStringList()
        << QStringLiteral("A") << QStringLiteral("B");
    QTRY_COMPARE(deliveredNames, expectedNames);
    const QString diagnostic = QStringLiteral("delivered=%1 originalAlive=%2")
        .arg(deliveredNames.join(QStringLiteral(",")))
        .arg(original ? QStringLiteral("true") : QStringLiteral("false"));
    QVERIFY2(original, qPrintable(diagnostic));
    QVERIFY2(actionEditor(&card, selectionContextActionAiSearch())
                 == original.data(),
             qPrintable(diagnostic));
    QCOMPARE(card.settings().actionCustomizations
             .value(selectionContextActionAiSearch()).displayName,
             QStringLiteral("B"));
    QTRY_VERIFY(name->hasFocus());
}

void SelectionContextSettingsCardTests::
simultaneousEditorSaveEchoPreservesBothPendingChanges()
{
    QVector<SelectionContextSettings> delivered;
    SelectionContextSettingsCard *cardPointer = nullptr;
    SelectionContextSettingsCard::Callbacks callbacks;
    callbacks.settingsChanged = [&](const SelectionContextSettings &value) {
        delivered.append(value);
        cardPointer->setSettings(value);
    };
    SelectionContextSettingsCard card(
        SelectionContextSettings(), callbacks);
    cardPointer = &card;
    SelectionContextActionEditor *search = actionEditor(
        &card, selectionContextActionAiSearch());
    SelectionContextActionEditor *translate = actionEditor(
        &card, selectionContextActionTranslate());
    QPointer<SelectionContextActionEditor> originalSearch(search);
    QPointer<SelectionContextActionEditor> originalTranslate(translate);
    QLineEdit *searchName = required<QLineEdit>(
        search, "selectionActionDisplayName");
    QLineEdit *translateName = required<QLineEdit>(
        translate, "selectionActionDisplayName");
    card.show();
    QTRY_VERIFY(card.isVisible());

    searchName->setText(QStringLiteral("Search A"));
    translateName->setFocus();
    translateName->setText(QStringLiteral("Translate B"));
    QTRY_VERIFY(translateName->hasFocus());

    QTRY_COMPARE(delivered.size(), 2);
    QCoreApplication::processEvents();
    QCOMPARE(delivered.size(), 2);
    for (const SelectionContextSettings &snapshot : delivered) {
        QCOMPARE(snapshot.actionCustomizations
                 .value(selectionContextActionAiSearch()).displayName,
                 QStringLiteral("Search A"));
        QCOMPARE(snapshot.actionCustomizations
                 .value(selectionContextActionTranslate()).displayName,
                 QStringLiteral("Translate B"));
    }
    QVERIFY(originalSearch);
    QVERIFY(originalTranslate);
    QCOMPARE(actionEditor(&card, selectionContextActionAiSearch()),
             originalSearch.data());
    QCOMPARE(actionEditor(&card, selectionContextActionTranslate()),
             originalTranslate.data());
    QCOMPARE(card.settings().actionCustomizations
             .value(selectionContextActionAiSearch()).displayName,
             QStringLiteral("Search A"));
    QCOMPARE(card.settings().actionCustomizations
             .value(selectionContextActionTranslate()).displayName,
             QStringLiteral("Translate B"));
    QTRY_VERIFY(translateName->hasFocus());
}

void SelectionContextSettingsCardTests::
buttonsAndChineseLabelsDoNotClipAt100_125_150_200Percent()
{
    QTemporaryDir fallback;
    QVERIFY(fallback.isValid());
    const QFont originalFont = QApplication::font();
    const QVector<int> scales = QVector<int>() << 100 << 125 << 150 << 200;
    for (int scale : scales) {
        QFont font(QStringLiteral("Microsoft YaHei UI"));
        font.setPixelSize(qMax(12, (14 * scale) / 100));
        QApplication::setFont(font);
        SelectionContextSettings settings;
        settings.networkConsentAcknowledged = true;
        SelectionContextSettingsCard *card =
            new SelectionContextSettingsCard(settings);
        QWidget host;
        host.setObjectName(QStringLiteral("selectionContextVisualHost"));
        QVBoxLayout *hostLayout = new QVBoxLayout(&host);
        QScrollArea *scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setWidget(card);
        hostLayout->addWidget(scroll);
        host.resize(qMax(620, (620 * scale) / 100), 520);
        host.show();
        QTRY_VERIFY(host.isVisible());
        QTRY_VERIFY(card->isVisibleTo(&host));
        scroll->verticalScrollBar()->setValue(0);
        QCoreApplication::processEvents();

#ifdef Q_OS_WIN
        QFontMetrics metrics(card->font());
        QVERIFY2(metrics.width(QString::fromUtf8("选中文字工具条")) > 20,
                 "Windows visual gate requires renderable CJK glyphs");
#endif
        const QList<QLabel *> labels = card->findChildren<QLabel *>();
        for (QLabel *label : labels) {
            if (!label->isVisible() || label->text().trimmed().isEmpty()) {
                continue;
            }
            const QString diagnostic = QStringLiteral(
                "scale=%1 text=%2 height=%3 hint=%4 object=%5"
            ).arg(scale)
             .arg(label->text())
             .arg(label->height())
             .arg(label->sizeHint().height())
             .arg(label->objectName());
            QVERIFY2(label->height() >= label->sizeHint().height(),
                     qPrintable(diagnostic));
        }
        const QList<QPushButton *> buttons = card->findChildren<QPushButton *>();
        for (QPushButton *button : buttons) {
            if (!button->isVisible()) {
                continue;
            }
            const QString diagnostic = QStringLiteral(
                "scale=%1 text=%2 minimum=%3 required=%4 object=%5"
            ).arg(scale)
             .arg(button->text())
             .arg(button->minimumHeight())
             .arg(qMax(40, QFontMetrics(button->font()).height() + 16))
             .arg(button->objectName());
            QVERIFY2(button->minimumHeight()
                     >= qMax(40, QFontMetrics(button->font()).height() + 16),
                     qPrintable(diagnostic));
            QVERIFY(button->height() >= button->sizeHint().height());
            QVERIFY(button->maximumHeight() == QWIDGETSIZE_MAX);
        }
        QLabel *title = required<QLabel>(
            card,
            "selectionContextSettingsTitle"
        );
        QVERIFY(QFontMetrics(title->font()).height()
                >= QFontMetrics(card->font()).height());
        QVERIFY(hasDarkForegroundPixels(title->grab()));
        const QString compactPath = visualOutputPath(
            QStringLiteral("selection-context-settings-%1-compact.png").arg(scale),
            &fallback
        );
        QVERIFY(host.grab().save(compactPath));
        QVERIFY(QFileInfo(compactPath).size() > 1000);

        host.resize(qMax(1000, (900 * scale) / 100), 900);
        scroll->verticalScrollBar()->setValue(0);
        QCoreApplication::processEvents();
        const QString maximizedPath = visualOutputPath(
            QStringLiteral("selection-context-settings-%1-maximized.png").arg(scale),
            &fallback
        );
        QVERIFY(host.grab().save(maximizedPath));
        QVERIFY(QFileInfo(maximizedPath).size() > 1000);

    }
    QApplication::setFont(originalFont);
}

void SelectionContextSettingsCardTests::
smallWindowAndExpandedEditorsRemainScrollableAndReachable()
{
    SelectionContextSettingsCard *card = new SelectionContextSettingsCard(
        SelectionContextSettings()
    );
    card->setExpandedAction(selectionContextActionTranslate());
    QWidget host;
    QVBoxLayout *layout = new QVBoxLayout(&host);
    QScrollArea *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setWidget(card);
    layout->addWidget(scroll);
    host.resize(520, 360);
    host.show();
    QTRY_VERIFY(host.isVisible());
    QTRY_VERIFY(scroll->verticalScrollBar()->maximum() > 0);

    QListWidget *actions = required<QListWidget>(
        card,
        "selectionContextActionList"
    );
    QCOMPARE(actions->count(), 5);
    scroll->verticalScrollBar()->setValue(
        scroll->verticalScrollBar()->maximum()
    );
    QCoreApplication::processEvents();
    QTextEdit *blocked = required<QTextEdit>(
        card,
        "selectionContextBlockedApplicationsEdit"
    );
    QVERIFY(scroll->viewport()->rect().intersects(
        blocked->geometry().translated(blocked->parentWidget()->pos())
    ) || scroll->verticalScrollBar()->value()
        == scroll->verticalScrollBar()->maximum());
}

QTEST_MAIN(SelectionContextSettingsCardTests)

#include "selection_context_settings_card_tests.moc"
