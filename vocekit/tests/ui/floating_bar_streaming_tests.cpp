#include <QtTest>

#include "../../src/ui/floating_bar.h"

class FloatingBarStreamingTests : public QObject
{
    Q_OBJECT

private slots:
    void expandsForCommittedAndProvisionalTextAndReturnsToBaseSize()
    {
        FloatingBar bar;
        QCOMPARE(bar.size(), QSize(720, 76));

        bar.setStreamingTranscript(
            QString::fromUtf8("已经确认，"),
            QString::fromUtf8("正在识别")
        );

        QVERIFY(bar.height() > 76);
        QLabel *committed = bar.findChild<QLabel *>(
            QStringLiteral("streamingCommittedText")
        );
        QLabel *provisional = bar.findChild<QLabel *>(
            QStringLiteral("streamingProvisionalText")
        );
        QVERIFY(committed);
        QVERIFY(provisional);
        QCOMPARE(committed->text(), QString::fromUtf8("已经确认，"));
        QCOMPARE(provisional->text(), QString::fromUtf8("正在识别"));
        QVERIFY(committed->wordWrap());
        QVERIFY(provisional->wordWrap());
        QVERIFY(provisional->styleSheet().contains(QStringLiteral("#60a5fa")));
        QCOMPARE(committed->textInteractionFlags(), Qt::NoTextInteraction);
        QCOMPARE(provisional->textInteractionFlags(), Qt::NoTextInteraction);

        bar.clearStreamingTranscript();
        QCOMPARE(bar.size(), QSize(720, 76));
    }

    void finalizingAndFallbackExposeDiagnosticStatus()
    {
        FloatingBar bar;
        bar.setStreamingFinalizing();
        QLabel *title = bar.findChild<QLabel *>(QStringLiteral("floatingBarTitle"));
        QLabel *subtitle = bar.findChild<QLabel *>(QStringLiteral("floatingBarSubtitle"));
        QVERIFY(title);
        QVERIFY(subtitle);
        QCOMPARE(title->text(), QString::fromUtf8("正在完成识别"));
        QVERIFY(subtitle->text().contains(QString::fromUtf8("最终文字")));

        bar.setStreamingFallback();
        QCOMPARE(title->text(), QString::fromUtf8("实时识别已切换"));
        QVERIFY(subtitle->text().contains(QString::fromUtf8("整段识别")));
    }

    void longMixedTextIsCappedAndActionButtonsRemainAvailable()
    {
        FloatingBar bar;
        bar.setStreamingTranscript(
            QString(240, QChar(0x8bc6)),
            QStringLiteral(" streaming provisional text that keeps changing")
        );
        bar.show();
        QTest::qWait(20);

        const QList<QPushButton *> buttons = bar.findChildren<QPushButton *>();
        QCOMPARE(buttons.size(), 3);
        for (QPushButton *button : buttons) {
            QVERIFY(button->isVisible());
            QVERIFY(button->height() >= 32);
        }
        QLabel *committed = bar.findChild<QLabel *>(
            QStringLiteral("streamingCommittedText")
        );
        QLabel *provisional = bar.findChild<QLabel *>(
            QStringLiteral("streamingProvisionalText")
        );
        const int lineHeight = QFontMetrics(committed->font()).lineSpacing();
        QVERIFY(committed->maximumHeight() <= lineHeight * 2 + 2);
        QVERIFY(provisional->maximumHeight() <= lineHeight + 2);
    }
};

QTEST_MAIN(FloatingBarStreamingTests)

#include "floating_bar_streaming_tests.moc"
