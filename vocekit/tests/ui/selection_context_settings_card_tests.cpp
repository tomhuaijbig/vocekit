#include <QtTest>

#include "../../src/domain/selection_context_actions.h"
#include "../../src/ui/selection_context_settings_card.h"

#include <QAbstractItemModel>
#include <QCheckBox>
#include <QComboBox>
#include <QFileInfo>
#include <QFontInfo>
#include <QLabel>
#include <QImage>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTextEdit>
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

QVector<QPair<QString, QString>> denseCatalog()
{
    QVector<QPair<QString, QString>> catalog;
    for (int i = 0; i < 24; ++i) {
        catalog.append(qMakePair(
            QString::fromUtf8("自定义功能 %1").arg(i + 1),
            QStringLiteral("function:custom_%1").arg(i + 1)
        ));
    }
    return catalog;
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
    void buttonsAndChineseLabelsDoNotClipAt100_125_150_200Percent();
    void smallWindowAndManyCustomFunctionsRemainScrollableAndReachable();
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
        QTest::qWait(20);
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
            QVERIFY(button->minimumHeight()
                    >= qMax(40, QFontMetrics(button->font()).height() + 16));
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

        SelectionContextSettingsCard *dense =
            new SelectionContextSettingsCard(settings);
        dense->setActionCatalog(denseCatalog());
        QWidget denseHost;
        QVBoxLayout *denseLayout = new QVBoxLayout(&denseHost);
        QScrollArea *denseScroll = new QScrollArea;
        denseScroll->setWidgetResizable(true);
        denseScroll->setWidget(dense);
        denseLayout->addWidget(denseScroll);
        denseHost.resize(qMax(620, (620 * scale) / 100), 520);
        denseHost.show();
        QTest::qWait(20);
        QCOMPARE(required<QListWidget>(
            dense,
            "selectionContextActionList"
        )->count(), 24);
        QVERIFY(denseScroll->verticalScrollBar()->maximum() > 0);
        denseScroll->verticalScrollBar()->setValue(
            denseScroll->verticalScrollBar()->maximum()
        );
        QCoreApplication::processEvents();
        const QString densePath = visualOutputPath(
            QStringLiteral("selection-context-settings-%1-dense.png").arg(scale),
            &fallback
        );
        QVERIFY(denseHost.grab().save(densePath));
        QVERIFY(QFileInfo(densePath).size() > 1000);
    }
    QApplication::setFont(originalFont);
}

void SelectionContextSettingsCardTests::
smallWindowAndManyCustomFunctionsRemainScrollableAndReachable()
{
    SelectionContextSettingsCard *card = new SelectionContextSettingsCard(
        SelectionContextSettings()
    );
    card->setActionCatalog(denseCatalog());
    QWidget host;
    QVBoxLayout *layout = new QVBoxLayout(&host);
    QScrollArea *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setWidget(card);
    layout->addWidget(scroll);
    host.resize(520, 360);
    host.show();
    QTest::qWait(20);

    QListWidget *actions = required<QListWidget>(
        card,
        "selectionContextActionList"
    );
    QCOMPARE(actions->count(), 24);
    QVERIFY(scroll->verticalScrollBar()->maximum() > 0);
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
