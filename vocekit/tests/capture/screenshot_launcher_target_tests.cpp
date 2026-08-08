#include <QtTest>

#include "../../src/capture/screenshot_launcher.h"

#include <QApplication>
#include <QMenu>
#include <QPushButton>
#include <QTimer>

class ScreenshotLauncherTargetTests : public QObject
{
    Q_OBJECT

private slots:
    void directTriggerUsesTargetCapturedBeforeClick()
    {
        ScreenshotLauncher launcher;
        launcher.setFunctions({
            qMakePair(
                QStringLiteral("translate"),
                QStringLiteral("翻译")
            )
        });
        launcher.show();
        QTest::qWait(1);

        int captureCount = 0;
        void *const expected =
            reinterpret_cast<void *>(quintptr(0x1234));
        launcher.captureTargetWindowCallback =
            [&captureCount, expected]() {
                ++captureCount;
                return expected;
            };
        QString triggeredId;
        void *triggeredTarget = nullptr;
        launcher.functionTriggeredCallback =
            [&triggeredId, &triggeredTarget](
                const QString &id,
                void *target) {
                triggeredId = id;
                triggeredTarget = target;
            };

        QPushButton *button =
            launcher.findChild<QPushButton *>();
        QVERIFY(button);
        QTest::mouseClick(button, Qt::LeftButton);

        QCOMPARE(captureCount, 1);
        QCOMPARE(triggeredId, QStringLiteral("translate"));
        QCOMPARE(triggeredTarget, expected);
    }

    void menuActionsShareThePreMenuTarget()
    {
        ScreenshotLauncher launcher;
        launcher.setFunctions({
            qMakePair(QStringLiteral("one"), QStringLiteral("一")),
            qMakePair(QStringLiteral("two"), QStringLiteral("二"))
        });
        launcher.show();
        QTest::qWait(1);

        void *const expected =
            reinterpret_cast<void *>(quintptr(0x5678));
        int captureCount = 0;
        launcher.captureTargetWindowCallback =
            [&captureCount, expected]() {
                ++captureCount;
                return expected;
            };
        QString triggeredId;
        void *triggeredTarget = nullptr;
        launcher.functionTriggeredCallback =
            [&triggeredId, &triggeredTarget](
                const QString &id,
                void *target) {
                triggeredId = id;
                triggeredTarget = target;
            };

        QPushButton *button =
            launcher.findChild<QPushButton *>();
        QVERIFY(button);
        QTest::mouseClick(button, Qt::LeftButton);
        QCoreApplication::processEvents();
        QMenu *menu = launcher.findChild<QMenu *>();
        QVERIFY(menu);
        QCOMPARE(menu->actions().size(), 2);
        menu->actions().at(1)->trigger();

        QCOMPARE(captureCount, 1);
        QCOMPARE(triggeredId, QStringLiteral("two"));
        QCOMPARE(triggeredTarget, expected);
    }
};

QTEST_MAIN(ScreenshotLauncherTargetTests)
#include "screenshot_launcher_target_tests.moc"
