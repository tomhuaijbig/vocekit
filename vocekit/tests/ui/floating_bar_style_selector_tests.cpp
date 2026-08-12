#include <QtTest>

#include "../../src/ui/floating_bar_style_selector.h"

#include <QAbstractButton>
#include <QFontMetrics>

class FloatingBarStyleSelectorTests : public QObject
{
    Q_OBJECT

private slots:
    void exposesTwoGlobalCardsAndEmitsOneMouseSelection()
    {
        FloatingBarStyleSelector::Options options;
        FloatingBarStyleSelector selector(options);
        selector.show();
        QTest::qWaitForWindowExposed(&selector);

        QAbstractButton *pill = selector.findChild<QAbstractButton *>(
            QStringLiteral("floatingBarStyleCard_statusPill")
        );
        QAbstractButton *transcript = selector.findChild<QAbstractButton *>(
            QStringLiteral("floatingBarStyleCard_liveTranscriptCard")
        );
        QVERIFY(pill);
        QVERIFY(transcript);
        QVERIFY(!selector.findChild<QAbstractButton *>(
            QStringLiteral("floatingBarStyleCard_inherit")
        ));
        QCOMPARE(selector.findChildren<QAbstractButton *>().size(), 2);

        QStringList changes;
        selector.setStyleChangedCallback(
            [&changes](const QString &style) { changes.append(style); }
        );
        QTest::mouseClick(transcript, Qt::LeftButton);

        QCOMPARE(selector.currentStyle(), QStringLiteral("liveTranscriptCard"));
        QCOMPARE(changes, QStringList() << QStringLiteral("liveTranscriptCard"));
        QCOMPARE(transcript->property("selected").toBool(), true);
        QCOMPARE(pill->property("selected").toBool(), false);
    }

    void inheritCardSupportsKeyboardAndMouseEqually()
    {
        FloatingBarStyleSelector::Options options;
        options.allowInherit = true;
        FloatingBarStyleSelector selector(options);
        selector.show();
        QTest::qWaitForWindowExposed(&selector);

        QAbstractButton *inherit = selector.findChild<QAbstractButton *>(
            QStringLiteral("floatingBarStyleCard_inherit")
        );
        QAbstractButton *pill = selector.findChild<QAbstractButton *>(
            QStringLiteral("floatingBarStyleCard_statusPill")
        );
        QVERIFY(inherit);
        QVERIFY(pill);
        QCOMPARE(selector.findChildren<QAbstractButton *>().size(), 3);

        QStringList changes;
        selector.setStyleChangedCallback(
            [&changes](const QString &style) { changes.append(style); }
        );
        selector.setCurrentStyle(QStringLiteral("liveTranscriptCard"));
        inherit->setFocus();
        QTest::keyClick(inherit, Qt::Key_Space);
        QTest::mouseClick(pill, Qt::LeftButton);

        QCOMPARE(
            changes,
            QStringList()
                << QStringLiteral("inherit")
                << QStringLiteral("statusPill")
        );
        QCOMPARE(selector.currentStyle(), QStringLiteral("statusPill"));
    }

    void cardsGrowWithLargeFontsAndNeverUseFixedHeight()
    {
        FloatingBarStyleSelector::Options options;
        options.allowInherit = true;
        FloatingBarStyleSelector selector(options);
        QFont font = selector.font();
        font.setPointSizeF(font.pointSizeF() * 1.5);
        selector.setFont(font);
        selector.show();

        const QList<QAbstractButton *> cards =
            selector.findChildren<QAbstractButton *>();
        QCOMPARE(cards.size(), 3);
        for (QAbstractButton *card : cards) {
            QVERIFY(card->minimumHeight()
                >= QFontMetrics(card->font()).height() * 3 + 28);
            QCOMPARE(card->maximumHeight(), QWIDGETSIZE_MAX);
        }
    }
};

QTEST_MAIN(FloatingBarStyleSelectorTests)
#include "floating_bar_style_selector_tests.moc"
