#include <QtTest>

#include "../../src/domain/selection_context_actions.h"
#include "../../src/ui/selection_context_action_editor.h"
#include "../../src/ui/selection_context_settings_card.h"

#include <QAbstractButton>
#include <QAbstractItemModel>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QFontInfo>
#include <QLabel>
#include <QImage>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QSpinBox>
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
        && left.usageHint == right.usageHint
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

QString visualOutputPath(const QString &fileName)
{
    const QString configured = QString::fromLocal8Bit(
        qgetenv("VOCEKIT_VISUAL_OUTPUT_DIR")
    ).trimmed();
    if (configured.isEmpty()) {
        return QString();
    }
    QDir().mkpath(configured);
    return QDir(configured).filePath(fileName);
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

QString visibleControlHeightError(QWidget *root, int percent)
{
    QList<QWidget *> controls;
    for (QLabel *widget : root->findChildren<QLabel *>()) {
        controls.append(widget);
    }
    for (QAbstractButton *widget :
         root->findChildren<QAbstractButton *>()) {
        controls.append(widget);
    }
    for (QComboBox *widget : root->findChildren<QComboBox *>()) {
        controls.append(widget);
    }
    for (QLineEdit *widget : root->findChildren<QLineEdit *>()) {
        controls.append(widget);
    }
    for (QPlainTextEdit *widget :
         root->findChildren<QPlainTextEdit *>()) {
        controls.append(widget);
    }

    QSet<QWidget *> checked;
    for (QWidget *widget : controls) {
        if (!widget || checked.contains(widget)
            || !widget->isVisibleTo(root)) {
            continue;
        }
        // An editable combo owns a private line edit inside its style frame.
        // The combo is the independently laid-out control checked by this gate.
        if (qobject_cast<QLineEdit *>(widget)
            && qobject_cast<QComboBox *>(widget->parentWidget())) {
            continue;
        }
        checked.insert(widget);
        const QSize hint = widget->sizeHint();
        if (widget->height() < hint.height()) {
            return QStringLiteral(
                "scale=%1 object=%2 class=%3 height=%4 hint=%5"
            ).arg(percent)
             .arg(widget->objectName())
             .arg(QString::fromLatin1(widget->metaObject()->className()))
             .arg(widget->height())
             .arg(hint.height());
        }
        QAbstractButton *button = qobject_cast<QAbstractButton *>(widget);
        if (button && !button->text().trimmed().isEmpty()
            && button->width() < hint.width()) {
            return QStringLiteral(
                "scale=%1 button=%2 text=%3 width=%4 hint=%5"
            ).arg(percent)
             .arg(button->objectName())
             .arg(button->text())
             .arg(button->width())
             .arg(hint.width());
        }
    }
    return QString();
}

bool isFullyVisibleIn(QWidget *widget, QWidget *ancestor)
{
    if (!widget || !ancestor || !widget->isVisibleTo(ancestor)) {
        return false;
    }
    return ancestor->rect().contains(QRect(
        widget->mapTo(ancestor, QPoint(0, 0)), widget->size()));
}

class ApplicationFontGuard
{
public:
    ApplicationFontGuard() : original_(QApplication::font()) {}
    ~ApplicationFontGuard() { QApplication::setFont(original_); }

private:
    QFont original_;
};

} // namespace

class SelectionContextSettingsCardTests : public QObject
{
    Q_OBJECT

private slots:
    void cardLoadsAndReturnsEveryTypedSetting();
    void actionRowsFollowCatalogAndDragReorderPersistsStableIds();
    void itemWidgetsSuppressDelegateTextButKeepAccessibleNames();
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
    void customizedActionsRenderWithoutClippingAt100_125_150Percent();
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
        QVERIFY(!list->item(i)->data(Qt::AccessibleTextRole)
                 .toString().trimmed().isEmpty());
    }

    QListWidgetItem *last = list->takeItem(list->count() - 1);
    list->insertItem(0, last);
    QCoreApplication::processEvents();
    QCOMPARE(card.settings().actionOrder.first(), selectionContextActionSave());
}

void SelectionContextSettingsCardTests::
itemWidgetsSuppressDelegateTextButKeepAccessibleNames()
{
    SelectionContextSettings settings;
    SelectionContextActionCustomization search =
        settings.actionCustomizations.value(selectionContextActionAiSearch());
    search.displayName = QString::fromUtf8("AI 搜索：分析并回答");
    settings.actionCustomizations.insert(selectionContextActionAiSearch(), search);
    SelectionContextSettingsCard card(settings);
    QListWidget *list = required<QListWidget>(
        &card, "selectionContextActionList");

    for (int row = 0; row < list->count(); ++row) {
        QListWidgetItem *item = list->item(row);
        const QString id = item->data(Qt::UserRole).toString();
        const SelectionContextActionEditor *editor = actionEditor(&card, id);
        QVERIFY2(editor, qPrintable(id));
        QVERIFY2(item->data(Qt::DisplayRole).toString().isEmpty(),
                 qPrintable(id));
        QCOMPARE(item->data(Qt::AccessibleTextRole).toString(),
                 editor->customization().displayName);
    }
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
        QVERIFY(listItem->data(Qt::DisplayRole).toString().isEmpty());
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
customizedActionsRenderWithoutClippingAt100_125_150Percent()
{
    struct VisualCase
    {
        int percent;
        QString expandedActionId;
        QString displayName;
        QString fileName;
    };
    const QVector<VisualCase> cases = QVector<VisualCase>()
        << VisualCase{
            100,
            selectionContextActionAiSearch(),
            QString::fromUtf8("AI 搜索：分析并回答"),
            QStringLiteral("selection-actions-ai-100.png")
        }
        << VisualCase{
            125,
            selectionContextActionTranslate(),
            QString::fromUtf8("翻译成我指定的目标语言"),
            QStringLiteral("selection-actions-translate-125.png")
        }
        << VisualCase{
            150,
            selectionContextActionSave(),
            QString::fromUtf8("保存到词库并确认作用范围"),
            QStringLiteral("selection-actions-save-150.png")
        };

    ApplicationFontGuard fontGuard;
    for (const VisualCase &visual : cases) {
        QFont font(QStringLiteral("Microsoft YaHei UI"));
        font.setPixelSize(qMax(12, (14 * visual.percent) / 100));
        QApplication::setFont(font);

#ifdef Q_OS_WIN
        const QFontMetrics cjkMetrics(QApplication::font());
        QVERIFY2(cjkMetrics.inFont(QChar(0x9009))
                 && cjkMetrics.inFont(QChar(0x4E2D))
                 && cjkMetrics.inFont(QChar(0x6587)),
                 "Windows visual gate requires a font with Chinese glyphs");
#endif

        SelectionContextSettings settings;
        settings.networkConsentAcknowledged = true;
        SelectionContextActionCustomization customization =
            settings.actionCustomizations.value(visual.expandedActionId);
        customization.displayName = visual.displayName;
        if (visual.expandedActionId == selectionContextActionAiSearch()
            || visual.expandedActionId == selectionContextActionTranslate()) {
            customization.modelId = QStringLiteral("alpha");
            customization.promptOverride = QString::fromUtf8(
                "请根据选中文字给出准确、简洁且可核对的回答。"
            );
        }
        if (visual.expandedActionId == selectionContextActionTranslate()) {
            customization.targetLanguage = QStringLiteral("English");
        }
        if (visual.expandedActionId == selectionContextActionSave()) {
            customization.vocabularyScopeId = QStringLiteral("writing");
        }
        settings.actionCustomizations.insert(
            visual.expandedActionId, customization);

        SelectionContextSettingsCard *card =
            new SelectionContextSettingsCard(settings);
        card->setCatalogs(testCatalogs());
        card->setExpandedAction(visual.expandedActionId);
        QWidget host;
        host.setObjectName(QStringLiteral("selectionContextVisualHost"));
        QVBoxLayout *hostLayout = new QVBoxLayout(&host);
        QScrollArea *scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setWidget(card);
        hostLayout->addWidget(scroll);
        host.resize(1180, 900);
        host.show();
        QTRY_VERIFY(host.isVisible());
        QTRY_VERIFY(card->isVisibleTo(&host));
        QListWidget *actions = required<QListWidget>(
            card, "selectionContextActionList");
        QListWidgetItem *expandedItem = nullptr;
        for (int row = 0; row < actions->count(); ++row) {
            if (actions->item(row)->data(Qt::UserRole).toString()
                == visual.expandedActionId) {
                expandedItem = actions->item(row);
                break;
            }
        }
        QVERIFY2(expandedItem, qPrintable(visual.expandedActionId));
        QTRY_VERIFY(actionEditor(card, visual.expandedActionId)->isExpanded());
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        if (host.layout()) {
            host.layout()->activate();
        }
        if (card->layout()) {
            card->layout()->activate();
        }
        actions->doItemsLayout();
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        actions->scrollToItem(
            expandedItem, QAbstractItemView::PositionAtCenter);
        const int actionsTop = actions->mapTo(card, QPoint(0, 0)).y();
        scroll->verticalScrollBar()->setValue(qMax(0, actionsTop - 90));
        QCoreApplication::processEvents(QEventLoop::AllEvents);

        const QString heightError = visibleControlHeightError(
            card, visual.percent);
        QVERIFY2(heightError.isEmpty(), qPrintable(heightError));
        SelectionContextActionEditor *expanded = actionEditor(
            card, visual.expandedActionId);
        QVERIFY(isFullyVisibleIn(
            required<QCheckBox>(expanded, "selectionActionVisible"), &host));
        QVERIFY(isFullyVisibleIn(
            required<QPushButton>(expanded, "selectionActionRestore"), &host));
        if (visual.expandedActionId == selectionContextActionAiSearch()
            || visual.expandedActionId == selectionContextActionTranslate()) {
            QVERIFY(isFullyVisibleIn(
                required<QComboBox>(expanded, "selectionActionModel"), &host));
            QVERIFY(isFullyVisibleIn(
                required<QPlainTextEdit>(expanded, "selectionActionPrompt"),
                &host));
            QVERIFY(isFullyVisibleIn(
                required<QLabel>(expanded, "selectionActionPromptCount"),
                &host));
        }
        if (visual.expandedActionId == selectionContextActionTranslate()) {
            QVERIFY(isFullyVisibleIn(required<QComboBox>(
                expanded, "selectionActionTargetLanguage"), &host));
        }
        if (visual.expandedActionId == selectionContextActionSave()) {
            QVERIFY(isFullyVisibleIn(required<QComboBox>(
                expanded, "selectionActionVocabularyScope"), &host));
        }
        const QPixmap screenshot = host.grab();
#ifdef Q_OS_WIN
        QVERIFY2(hasDarkForegroundPixels(screenshot),
                 "Windows visual gate requires dark rendered text pixels");
#endif
        const QString outputPath = visualOutputPath(visual.fileName);
        if (!outputPath.isEmpty()) {
            QVERIFY2(screenshot.save(outputPath), qPrintable(outputPath));
            QVERIFY(QFileInfo(outputPath).size() > 1000);
        }
    }
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
