#include <QtTest>

#include "../../src/ui/selection_context_placement.h"
#include "../../src/ui/selection_context_toolbar.h"
#include "../../src/config/app_settings_data.h"
#include "../../src/domain/selection_context_actions.h"

#include <QAction>
#include <QApplication>
#include <QHBoxLayout>
#include <QMenu>
#include <QMouseEvent>
#include <QPointer>
#include <QToolButton>

class SelectionContextToolbarTests : public QObject
{
    Q_OBJECT

private slots:
    void preferredBelowAndAbovePositionsAreDeterministic()
    {
        const QRect screen(0, 0, 1200, 900);
        const QSize toolbar(560, 48);
        SelectionSurfacePlacement placed = placeSelectionSurfaces(
            QRect(400, 100, 120, 24),
            QPoint(450, 112),
            toolbar,
            QSize(560, 240),
            screen,
            8
        );
        QCOMPARE(placed.toolbarTopLeft, QPoint(180, 132));
        QVERIFY(!placed.toolbarAbove);

        placed = placeSelectionSurfaces(
            QRect(400, 850, 120, 24),
            QPoint(450, 860),
            toolbar,
            QSize(560, 240),
            screen,
            8
        );
        QCOMPARE(placed.toolbarTopLeft, QPoint(180, 794));
        QVERIFY(placed.toolbarAbove);
    }

    void invalidAnchorFallsBackToCursorWithoutCoveringHotspot()
    {
        const SelectionSurfacePlacement placed = placeSelectionSurfaces(
            QRect(),
            QPoint(300, 240),
            QSize(400, 48),
            QSize(400, 220),
            QRect(0, 0, 1000, 700),
            8
        );
        QCOMPARE(placed.toolbarTopLeft, QPoint(308, 248));
        QVERIFY(!placed.toolbarAbove);
    }

    void placementStaysInsideAvailableGeometry_data()
    {
        QTest::addColumn<QRect>("screen");
        QTest::addColumn<QRect>("anchor");
        QTest::addColumn<QPoint>("cursor");
        QTest::addColumn<QSize>("toolbar");
        QTest::newRow("right-edge")
            << QRect(0, 0, 1920, 1040)
            << QRect(1870, 300, 45, 22)
            << QPoint(1900, 315)
            << QSize(560, 48);
        QTest::newRow("left-edge")
            << QRect(0, 0, 1920, 1040)
            << QRect(2, 300, 45, 22)
            << QPoint(5, 315)
            << QSize(560, 48);
        QTest::newRow("taskbar-bottom")
            << QRect(0, 0, 1920, 1040)
            << QRect(800, 1014, 140, 22)
            << QPoint(900, 1025)
            << QSize(560, 48);
        QTest::newRow("negative-monitor")
            << QRect(-1920, 0, 1920, 1040)
            << QRect(-80, 980, 70, 20)
            << QPoint(-20, 990)
            << QSize(560, 48);
        QTest::newRow("two-hundred-percent-logical")
            << QRect(-960, -40, 960, 520)
            << QRect(-140, 400, 80, 18)
            << QPoint(-90, 410)
            << QSize(560, 48);
    }

    void placementStaysInsideAvailableGeometry()
    {
        QFETCH(QRect, screen);
        QFETCH(QRect, anchor);
        QFETCH(QPoint, cursor);
        QFETCH(QSize, toolbar);
        const QSize card(
            qMin(toolbar.width(), screen.width()),
            qMin(320, screen.height())
        );
        const SelectionSurfacePlacement placed = placeSelectionSurfaces(
            anchor,
            cursor,
            toolbar,
            card,
            screen,
            8
        );
        QVERIFY(screen.contains(QRect(placed.toolbarTopLeft, toolbar)));
        QVERIFY(screen.contains(QRect(placed.cardTopLeft, card)));
    }

    void cardIsCenteredBelowToolbarWhenItFits()
    {
        const QSize toolbar(400, 48);
        const QSize card(560, 220);
        const SelectionSurfacePlacement placed = placeSelectionSurfaces(
            QRect(500, 80, 100, 20),
            QPoint(550, 90),
            toolbar,
            card,
            QRect(0, 0, 1400, 900),
            8
        );
        QCOMPARE(
            placed.cardTopLeft.x(),
            placed.toolbarTopLeft.x()
                + (toolbar.width() - card.width()) / 2
        );
        QCOMPARE(
            placed.cardTopLeft.y(),
            placed.toolbarTopLeft.y() + toolbar.height() + 8
        );
        QVERIFY(!placed.cardAbove);
    }

    void cardMovesAboveBothSurfacesWhenBelowWouldOverflow()
    {
        const QSize toolbar(400, 48);
        const QSize card(560, 300);
        const QRect screen(0, 0, 1200, 700);
        const SelectionSurfacePlacement placed = placeSelectionSurfaces(
            QRect(500, 620, 100, 20),
            QPoint(550, 630),
            toolbar,
            card,
            screen,
            8
        );
        QVERIFY(placed.cardAbove);
        QVERIFY(
            placed.cardTopLeft.y() + card.height()
                <= placed.toolbarTopLeft.y() - 8
        );
        QVERIFY(screen.contains(QRect(placed.cardTopLeft, card)));
    }

    void oversizedSurfacePinsToAvailableGeometryOrigin()
    {
        const QRect screen(-500, 20, 300, 180);
        const SelectionSurfacePlacement placed = placeSelectionSurfaces(
            QRect(-400, 80, 20, 20),
            QPoint(-390, 90),
            QSize(600, 240),
            QSize(700, 300),
            screen,
            8
        );
        QCOMPARE(placed.toolbarTopLeft, screen.topLeft());
        QCOMPARE(placed.cardTopLeft, screen.topLeft());
    }

    void toolbarUsesCatalogOrderWithoutTakingFocus()
    {
        SelectionContextToolbar toolbar;
        toolbar.setActionOrder(
            QStringList() << selectionContextActionCopy()
                          << selectionContextActionTranslate()
                          << selectionContextActionAiSearch()
        );
        QCOMPARE(
            toolbar.objectName(),
            QStringLiteral("selectionContextToolbar")
        );
        QVERIFY(toolbar.testAttribute(Qt::WA_ShowWithoutActivating));
        QVERIFY(toolbar.windowFlags() & Qt::Tool);
        QVERIFY(toolbar.windowFlags() & Qt::FramelessWindowHint);
        QVERIFY(toolbar.windowFlags() & Qt::WindowDoesNotAcceptFocus);
        QVERIFY(toolbar.findChild<QWidget *>(
            QStringLiteral("selectionContextIdentity")));
        QVERIFY(toolbar.findChild<QToolButton *>(
            QStringLiteral("selectionContextMoreButton")));

        QStringList ordered;
        QHBoxLayout *layout = qobject_cast<QHBoxLayout *>(toolbar.layout());
        QVERIFY(layout);
        for (int i = 0; i < layout->count(); ++i) {
            QToolButton *button = qobject_cast<QToolButton *>(
                layout->itemAt(i)->widget()
            );
            if (button
                && !button->property("selectionActionId")
                        .toString().isEmpty()
                && button->isVisibleTo(&toolbar)) {
                ordered.append(button->property("selectionActionId").toString());
            }
        }
        QCOMPARE(
            ordered,
            QStringList() << selectionContextActionCopy()
                          << selectionContextActionTranslate()
                          << selectionContextActionAiSearch()
                          << selectionContextActionExplain()
                          << selectionContextActionSave()
        );
        for (const QString &id : defaultSelectionContextActionOrder()) {
            QToolButton *button = nullptr;
            for (QToolButton *candidate :
                 toolbar.findChildren<QToolButton *>()) {
                if (candidate->property("selectionActionId").toString()
                    == id) {
                    button = candidate;
                    break;
                }
            }
            QVERIFY2(button, qPrintable(id));
            QCOMPARE(button->text(), selectionContextActionTitle(id));
        }
    }

    void customPresentationFiltersAndRenamesWithoutChangingIds()
    {
        SelectionContextToolbar toolbar;
        SelectionContextSettings settings;
        SelectionContextActionCustomization search =
            settings.actionCustomizations.value(
                selectionContextActionAiSearch()
            );
        search.displayName = QString::fromUtf8("问 AI");
        settings.actionCustomizations.insert(
            selectionContextActionAiSearch(),
            search
        );
        SelectionContextActionCustomization save =
            settings.actionCustomizations.value(selectionContextActionSave());
        save.visible = false;
        settings.actionCustomizations.insert(selectionContextActionSave(), save);

        QString clicked;
        SelectionContextToolbarCallbacks callbacks;
        callbacks.actionRequested = [&clicked](const QString &id) {
            clicked = id;
        };
        toolbar.setCallbacks(callbacks);
        toolbar.setActionPresentation(
            settings.actionOrder,
            settings.actionCustomizations
        );

        QToolButton *searchButton = toolbar.findChild<QToolButton *>(
            QStringLiteral("selectionActionAiSearchButton")
        );
        QVERIFY(searchButton);
        QCOMPARE(searchButton->text(), QString::fromUtf8("问 AI"));
        QVERIFY(!toolbar.findChild<QToolButton *>(
            QStringLiteral("selectionActionSaveButton")
        ));
        QMenu *menu = toolbar.findChild<QMenu *>(
            QStringLiteral("selectionContextMoreMenu")
        );
        QVERIFY(menu);
        for (QAction *action : menu->actions()) {
            QVERIFY(action->data().toString() != selectionContextActionSave());
        }

        searchButton->click();
        QCOMPARE(clicked, selectionContextActionAiSearch());
    }

    void compatibilityOrderSetterRestoresDefaultPresentation()
    {
        SelectionContextToolbar toolbar;
        SelectionContextActionCustomizationMap customizations =
            defaultSelectionContextActionCustomizations();
        SelectionContextActionCustomization search = customizations.value(
            selectionContextActionAiSearch()
        );
        search.displayName = QString::fromUtf8("临时名称");
        customizations.insert(selectionContextActionAiSearch(), search);
        SelectionContextActionCustomization save = customizations.value(
            selectionContextActionSave()
        );
        save.visible = false;
        customizations.insert(selectionContextActionSave(), save);
        toolbar.setActionPresentation(
            defaultSelectionContextActionOrder(),
            customizations
        );

        toolbar.setActionOrder(
            QStringList() << selectionContextActionCopy()
        );

        for (const QString &id : defaultSelectionContextActionOrder()) {
            QToolButton *button = nullptr;
            for (QToolButton *candidate :
                 toolbar.findChildren<QToolButton *>()) {
                if (candidate->property("selectionActionId").toString()
                    == id) {
                    button = candidate;
                    break;
                }
            }
            QVERIFY2(button, qPrintable(id));
            QCOMPARE(button->text(), selectionContextActionTitle(id));
        }
    }

    void invalidOrderDuplicateTitlesAndHiddenActionsKeepStableIds()
    {
        SelectionContextToolbar toolbar;
        SelectionContextActionCustomizationMap customizations =
            defaultSelectionContextActionCustomizations();
        SelectionContextActionCustomization search = customizations.value(
            selectionContextActionAiSearch()
        );
        search.displayName = QString::fromUtf8("同名操作");
        customizations.insert(selectionContextActionAiSearch(), search);
        SelectionContextActionCustomization translate = customizations.value(
            selectionContextActionTranslate()
        );
        translate.displayName = QString::fromUtf8("同名操作");
        customizations.insert(selectionContextActionTranslate(), translate);
        for (const QString &id : QStringList()
             << selectionContextActionExplain()
             << selectionContextActionSave()) {
            SelectionContextActionCustomization hidden =
                customizations.value(id);
            hidden.visible = false;
            customizations.insert(id, hidden);
        }

        toolbar.setActionPresentation(
            QStringList() << selectionContextActionCopy()
                          << QStringLiteral("unknown")
                          << selectionContextActionCopy()
                          << selectionContextActionTranslate(),
            customizations
        );
        SelectionSnapshot snapshot;
        snapshot.anchorRect = QRect(120, 80, 60, 20);
        snapshot.cursorPosition = QPoint(150, 90);
        toolbar.showForSnapshot(snapshot, QRect(0, 0, 320, 400));

        const QStringList expected = QStringList()
            << selectionContextActionCopy()
            << selectionContextActionTranslate()
            << selectionContextActionAiSearch();
        QMap<QString, int> counts;
        for (QToolButton *button : toolbar.findChildren<QToolButton *>()) {
            const QString id = button->property("selectionActionId").toString();
            if (!id.isEmpty() && button->isVisible()) {
                ++counts[id];
            }
        }
        QMenu *menu = toolbar.findChild<QMenu *>(
            QStringLiteral("selectionContextMoreMenu")
        );
        QVERIFY(menu);
        for (QAction *action : menu->actions()) {
            const QString id = action->data().toString();
            if (defaultSelectionContextActionOrder().contains(id)) {
                ++counts[id];
            }
        }
        for (const QString &id : expected) {
            QCOMPARE(counts.value(id), 1);
        }
        for (const QString &id : QStringList()
             << selectionContextActionExplain()
             << selectionContextActionSave()) {
            QCOMPARE(counts.value(id), 0);
            for (QToolButton *button :
                 toolbar.findChildren<QToolButton *>()) {
                QVERIFY(button->property("selectionActionId").toString()
                    != id);
            }
            for (QAction *action : menu->actions()) {
                QVERIFY(action->data().toString() != id);
            }
        }

        QStringList clicked;
        SelectionContextToolbarCallbacks callbacks;
        callbacks.actionRequested = [&clicked](const QString &id) {
            clicked.append(id);
        };
        toolbar.setCallbacks(callbacks);
        for (const QString &id : QStringList()
             << selectionContextActionTranslate()
             << selectionContextActionAiSearch()) {
            QToolButton *button = nullptr;
            for (QToolButton *candidate :
                 toolbar.findChildren<QToolButton *>()) {
                if (candidate->property("selectionActionId").toString()
                    == id) {
                    button = candidate;
                    break;
                }
            }
            QVERIFY2(button, qPrintable(id));
            button->click();
        }
        QCOMPARE(
            clicked,
            QStringList() << selectionContextActionTranslate()
                          << selectionContextActionAiSearch()
        );
    }

    void longCustomNameKeepsStableAccessibilityAndFlexibleHeight()
    {
        SelectionContextToolbar toolbar;
        SelectionContextActionCustomizationMap customizations =
            defaultSelectionContextActionCustomizations();
        SelectionContextActionCustomization search = customizations.value(
            selectionContextActionAiSearch()
        );
        const QString title = QString::fromUtf8(
            "这是一个很长的中文自定义动作名称"
        );
        search.displayName = title;
        customizations.insert(selectionContextActionAiSearch(), search);
        toolbar.setActionPresentation(
            defaultSelectionContextActionOrder(),
            customizations
        );

        QToolButton *button = toolbar.findChild<QToolButton *>(
            QStringLiteral("selectionActionAiSearchButton")
        );
        QVERIFY(button);
        QCOMPARE(button->objectName(),
                 QStringLiteral("selectionActionAiSearchButton"));
        QCOMPARE(button->text(), title);
        QCOMPARE(button->toolTip(), title);
        QCOMPARE(button->accessibleName(), title);
        QVERIFY(button->minimumHeight()
            >= qMax(40, button->fontMetrics().height() + 16));
        QVERIFY(button->maximumHeight() > button->minimumHeight());
    }

    void rebuiltToolbarCallbackMayDeleteToolbarSynchronously()
    {
        SelectionContextToolbar *toolbar = new SelectionContextToolbar;
        QPointer<SelectionContextToolbar> guard(toolbar);
        SelectionContextActionCustomizationMap customizations =
            defaultSelectionContextActionCustomizations();
        SelectionContextActionCustomization translate = customizations.value(
            selectionContextActionTranslate()
        );
        translate.displayName = QString::fromUtf8("重建后的翻译");
        customizations.insert(selectionContextActionTranslate(), translate);
        toolbar->setActionPresentation(
            defaultSelectionContextActionOrder(),
            customizations
        );
        SelectionContextToolbarCallbacks callbacks;
        callbacks.actionRequested = [toolbar](const QString &id) {
            QCOMPARE(id, selectionContextActionTranslate());
            delete toolbar;
        };
        toolbar->setCallbacks(callbacks);
        QToolButton *button = toolbar->findChild<QToolButton *>(
            QStringLiteral("selectionActionTranslateButton")
        );
        QVERIFY(button);
        button->click();
        QVERIFY(!guard);
    }

    void fiveDefaultActionsExistExactlyOnceAndHaveFlexibleHeight()
    {
        SelectionContextToolbar toolbar;
        const QStringList expected = defaultSelectionContextActionOrder();
        for (const QString &id : expected) {
            int count = 0;
            for (QToolButton *button : toolbar.findChildren<QToolButton *>()) {
                if (button->property("selectionActionId").toString() == id) {
                    ++count;
                    QVERIFY(button->minimumHeight()
                        >= qMax(40, button->fontMetrics().height() + 16));
                    QVERIFY(button->maximumHeight() > button->minimumHeight());
                }
            }
            QCOMPARE(count, 1);
        }
    }

    void translucentWindowStillPaintsItsCardBackground()
    {
        SelectionContextToolbar toolbar;
        SelectionSnapshot snapshot;
        snapshot.anchorRect = QRect(300, 80, 100, 24);
        snapshot.cursorPosition = QPoint(350, 90);
        toolbar.showForSnapshot(snapshot, QRect(0, 0, 1000, 700));
        QVERIFY(QTest::qWaitForWindowExposed(&toolbar));
        const QImage image = toolbar.grab().toImage().convertToFormat(
            QImage::Format_ARGB32
        );
        QVERIFY(!image.isNull());
        QVERIFY(qAlpha(image.pixel(image.width() / 2, 5)) > 240);
    }

    void everyClickEmitsExactlyOneStableActionId()
    {
        SelectionContextToolbar toolbar;
        QStringList calls;
        SelectionContextToolbarCallbacks callbacks;
        callbacks.actionRequested = [&calls](const QString &id) {
            calls.append(id);
        };
        toolbar.setCallbacks(callbacks);
        for (const QString &id : defaultSelectionContextActionOrder()) {
            QToolButton *button = nullptr;
            for (QToolButton *candidate :
                 toolbar.findChildren<QToolButton *>()) {
                if (candidate->property("selectionActionId").toString()
                    == id) {
                    button = candidate;
                    break;
                }
            }
            QVERIFY(button);
            QTest::mouseClick(button, Qt::LeftButton);
        }
        QCOMPARE(calls, defaultSelectionContextActionOrder());
    }

    void busyStateDisablesOtherActionsAndKeepsCloseAvailable()
    {
        SelectionContextToolbar toolbar;
        toolbar.setBusyAction(selectionContextActionTranslate());
        for (QToolButton *button : toolbar.findChildren<QToolButton *>()) {
            const QString id = button->property("selectionActionId").toString();
            if (!id.isEmpty()) {
                QVERIFY(!button->isEnabled());
            }
        }
        QToolButton *close = toolbar.findChild<QToolButton *>(
            QStringLiteral("selectionToolbarCloseButton")
        );
        QVERIFY(close);
        QVERIFY(close->isEnabled());
        toolbar.setBusyAction(QString());
        for (QToolButton *button : toolbar.findChildren<QToolButton *>()) {
            if (!button->property("selectionActionId").toString().isEmpty()) {
                QVERIFY(button->isEnabled());
            }
        }
    }

    void dragHandleMovesTheToolbarButDraggingAButtonDoesNot()
    {
        SelectionContextToolbar toolbar;
        toolbar.move(200, 200);
        toolbar.show();
        QVERIFY(QTest::qWaitForWindowExposed(&toolbar));
        QWidget *handle = toolbar.findChild<QWidget *>(
            QStringLiteral("selectionContextDragHandle")
        );
        QVERIFY(handle);
        const QPoint before = toolbar.pos();
        const QPoint localPress(4, handle->height() / 2);
        const QPoint globalPress = handle->mapToGlobal(localPress);
        QMouseEvent press(
            QEvent::MouseButtonPress,
            localPress,
            globalPress,
            Qt::LeftButton,
            Qt::LeftButton,
            Qt::NoModifier
        );
        QApplication::sendEvent(handle, &press);
        const QPoint globalMove = globalPress + QPoint(26, 0);
        QMouseEvent move(
            QEvent::MouseMove,
            localPress + QPoint(26, 0),
            globalMove,
            Qt::NoButton,
            Qt::LeftButton,
            Qt::NoModifier
        );
        QApplication::sendEvent(handle, &move);
        QMouseEvent release(
            QEvent::MouseButtonRelease,
            localPress + QPoint(26, 0),
            globalMove,
            Qt::LeftButton,
            Qt::NoButton,
            Qt::NoModifier
        );
        QApplication::sendEvent(handle, &release);
        QVERIFY(toolbar.pos() != before);

        QToolButton *copy = nullptr;
        for (QToolButton *button : toolbar.findChildren<QToolButton *>()) {
            if (button->property("selectionActionId").toString()
                == selectionContextActionCopy()) {
                copy = button;
                break;
            }
        }
        QVERIFY(copy);
        const QPoint afterHandle = toolbar.pos();
        QTest::mousePress(copy, Qt::LeftButton, Qt::NoModifier,
                          copy->rect().center());
        QTest::mouseMove(copy, copy->rect().center() + QPoint(12, 0));
        QTest::mouseRelease(copy, Qt::LeftButton, Qt::NoModifier,
                            copy->rect().center() + QPoint(12, 0));
        QCOMPARE(toolbar.pos(), afterHandle);
    }

    void narrowScreenMovesTrailingActionsIntoMoreWithoutDuplication()
    {
        SelectionContextToolbar toolbar;
        SelectionSnapshot snapshot;
        snapshot.anchorRect = QRect(140, 80, 60, 20);
        snapshot.cursorPosition = QPoint(170, 90);
        const QRect screen(0, 0, 360, 400);
        toolbar.showForSnapshot(snapshot, screen);
        QVERIFY(screen.contains(toolbar.geometry()));

        QMap<QString, int> counts;
        for (QToolButton *button : toolbar.findChildren<QToolButton *>()) {
            const QString id = button->property("selectionActionId").toString();
            if (!id.isEmpty() && button->isVisible()) {
                ++counts[id];
            }
        }
        QMenu *menu = toolbar.findChild<QMenu *>(
            QStringLiteral("selectionContextMoreMenu")
        );
        QVERIFY(menu);
        for (QAction *action : menu->actions()) {
            const QString id = action->data().toString();
            if (defaultSelectionContextActionOrder().contains(id)) {
                ++counts[id];
            }
        }
        for (const QString &id : defaultSelectionContextActionOrder()) {
            QCOMPARE(counts.value(id), 1);
        }
    }

    void twoHundredPercentFontStillFitsTheAvailableScreen()
    {
        SelectionContextToolbar toolbar;
        QFont large = toolbar.font();
        large.setPointSizeF(qMax(18.0, large.pointSizeF() * 2.0));
        toolbar.setFont(large);
        SelectionSnapshot snapshot;
        snapshot.anchorRect = QRect(300, 80, 100, 24);
        snapshot.cursorPosition = QPoint(350, 90);
        const QRect screen(0, 0, 760, 500);
        toolbar.showForSnapshot(snapshot, screen);
        QVERIFY(screen.contains(toolbar.geometry()));
        for (QToolButton *button : toolbar.findChildren<QToolButton *>()) {
            if (!button->isVisible()) {
                continue;
            }
            QVERIFY(button->height() >= button->sizeHint().height());
            QVERIFY(button->minimumHeight()
                >= qMax(40, button->fontMetrics().height() + 16));
        }
    }

    void fallbackShortcutModeReceivesKeyboardEscape()
    {
        SelectionContextToolbar *toolbar = new SelectionContextToolbar;
        QPointer<SelectionContextToolbar> guard(toolbar);
        int closeCount = 0;
        SelectionContextToolbarCallbacks callbacks;
        callbacks.closeRequested = [&closeCount]() { ++closeCount; };
        toolbar->setCallbacks(callbacks);
        SelectionSnapshot snapshot;
        snapshot.anchorRect = QRect(300, 80, 100, 24);
        snapshot.cursorPosition = QPoint(350, 90);
        toolbar->showForSnapshot(snapshot, QRect(0, 0, 1000, 700), true);
        QVERIFY(!(toolbar->windowFlags() & Qt::WindowDoesNotAcceptFocus));
        QTRY_VERIFY(toolbar->focusWidget());
        QTest::keyClick(toolbar->focusWidget(), Qt::Key_Escape);
        QTRY_COMPARE(closeCount, 1);
        QVERIFY(guard);
        toolbar->hideToolbar();
        QVERIFY(toolbar->windowFlags() & Qt::WindowDoesNotAcceptFocus);
        delete toolbar;
    }

    void moreMenuEmitsCustomPauseAndSettingsIdsExactlyOnce()
    {
        SelectionContextToolbar toolbar;
        QVector<SelectionContextMenuItem> items;
        SelectionContextMenuItem custom;
        custom.actionId = QStringLiteral("function:demo");
        custom.title = QStringLiteral("Demo");
        items.append(custom);
        SelectionContextMenuItem pause;
        pause.actionId = QStringLiteral("pause-30");
        pause.title = QString::fromUtf8("暂停 30 分钟");
        items.append(pause);
        toolbar.setMoreActions(items);
        QStringList calls;
        SelectionContextToolbarCallbacks callbacks;
        callbacks.actionRequested = [&calls](const QString &id) {
            calls.append(id);
        };
        toolbar.setCallbacks(callbacks);

        const QStringList expected = QStringList()
            << QStringLiteral("function:demo")
            << QStringLiteral("pause-30")
            << selectionContextMenuBlockApplication()
            << selectionContextMenuOpenSettings();
        QMenu *menu = toolbar.findChild<QMenu *>(
            QStringLiteral("selectionContextMoreMenu")
        );
        QVERIFY(menu);
        for (const QString &id : expected) {
            QAction *matched = nullptr;
            for (QAction *action : menu->actions()) {
                if (action->data().toString() == id) {
                    matched = action;
                    break;
                }
            }
            QVERIFY2(matched, qPrintable(id));
            matched->trigger();
        }
        QCOMPARE(calls, expected);
    }

    void ownsNativeWindowIncludesTransientMoreMenu()
    {
        SelectionContextToolbar toolbar;
        toolbar.show();
        QVERIFY(QTest::qWaitForWindowExposed(&toolbar));
        QVERIFY(toolbar.ownsNativeWindow(
            reinterpret_cast<SelectedTextNativeWindowHandle>(
                quintptr(toolbar.winId())
            )
        ));
        QMenu *menu = toolbar.findChild<QMenu *>(
            QStringLiteral("selectionContextMoreMenu")
        );
        QVERIFY(menu);
        menu->popup(toolbar.mapToGlobal(QPoint(0, toolbar.height())));
        QTRY_VERIFY(menu->isVisible());
        QVERIFY(toolbar.ownsNativeWindow(
            reinterpret_cast<SelectedTextNativeWindowHandle>(
                quintptr(menu->winId())
            )
        ));
        menu->hide();
    }

    void moreActionCanEnterBusyStateWithoutDeletingItsSender()
    {
        SelectionContextToolbar toolbar;
        QVector<SelectionContextMenuItem> items;
        SelectionContextMenuItem custom;
        custom.actionId = QStringLiteral("function:busy-demo");
        custom.title = QStringLiteral("Busy Demo");
        items.append(custom);
        toolbar.setMoreActions(items);
        QMenu *menu = toolbar.findChild<QMenu *>(
            QStringLiteral("selectionContextMoreMenu")
        );
        QVERIFY(menu);
        QAction *rawAction = nullptr;
        for (QAction *action : menu->actions()) {
            if (action->data().toString() == custom.actionId) {
                rawAction = action;
                break;
            }
        }
        QVERIFY(rawAction);
        QPointer<QAction> actionGuard(rawAction);
        SelectionContextToolbarCallbacks callbacks;
        callbacks.actionRequested = [&toolbar](const QString &id) {
            toolbar.setBusyAction(id);
        };
        toolbar.setCallbacks(callbacks);
        rawAction->trigger();
        QVERIFY(actionGuard);
        QVERIFY(!actionGuard->isEnabled());
    }

    void callbackMayDeleteToolbarSynchronously()
    {
        SelectionContextToolbar *toolbar = new SelectionContextToolbar;
        QPointer<SelectionContextToolbar> guard(toolbar);
        SelectionContextToolbarCallbacks callbacks;
        callbacks.actionRequested = [toolbar](const QString &) {
            delete toolbar;
        };
        toolbar->setCallbacks(callbacks);
        QToolButton *button = toolbar->findChild<QToolButton *>(
            QStringLiteral("selectionActionCopyButton")
        );
        QVERIFY(button);
        button->click();
        QVERIFY(!guard);
    }
};

QTEST_MAIN(SelectionContextToolbarTests)
#include "selection_context_toolbar_tests.moc"
