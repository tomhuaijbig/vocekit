#include <QtTest>

#include "../../src/config/app_settings_defaults.h"
#include "../../src/ui/floating_bar.h"

#include <QLabel>
#include <QFont>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>

class FloatingBarStreamingTests : public QObject
{
    Q_OBJECT

private slots:
    void statusPillIsCompactAndMapsStages()
    {
        FloatingBar bar;
        bar.setStyle(floatingBarStyleStatusPill());
        bar.setStage(FloatingBarStage::Recording);
        bar.setWaveformVisible(true);
        bar.show();

        QVERIFY(bar.width() <= 420);
        QVERIFY(bar.height() <= 180);
        QVERIFY(bar.findChild<QPushButton *>(
            QStringLiteral("floatingCancelButton")));
        QVERIFY(bar.findChild<QPushButton *>(
            QStringLiteral("floatingConfirmButton")));
        QVERIFY(bar.findChild<QWidget *>(
            QStringLiteral("floatingWaveform")));
        QVERIFY(!bar.findChild<QLabel *>(
            QStringLiteral("streamingCommittedText")));

        QLabel *title = bar.findChild<QLabel *>(
            QStringLiteral("floatingBarTitle")
        );
        QVERIFY(title);
        QCOMPARE(title->text(), QString::fromUtf8("正在聆听"));
        bar.setStage(FloatingBarStage::Recognizing);
        QCOMPARE(title->text(), QString::fromUtf8("正在转录"));
        bar.setStage(FloatingBarStage::ModelProcessing);
        QCOMPARE(title->text(), QString::fromUtf8("AI 处理中"));
        bar.setStage(FloatingBarStage::Writing);
        QCOMPARE(title->text(), QString::fromUtf8("正在写入"));
        bar.setStage(
            FloatingBarStage::Completed,
            QString::fromUtf8("已写入")
        );
        QCOMPARE(title->text(), QString::fromUtf8("已写入"));
    }

    void liveTranscriptKeepsCommittedTextAcrossFallbackAndCapsHeight()
    {
        FloatingBar bar;
        bar.setStyle(floatingBarStyleLiveTranscriptCard());
        bar.setStage(FloatingBarStage::Streaming);
        const QString committed = QString(240, QChar(0x8bc6));
        bar.setStreamingTranscript(
            committed,
            QStringLiteral(" streaming provisional text that keeps changing")
        );
        bar.show();

        QLabel *committedLabel = bar.findChild<QLabel *>(
            QStringLiteral("streamingCommittedText")
        );
        QLabel *provisional = bar.findChild<QLabel *>(
            QStringLiteral("streamingProvisionalText")
        );
        QVERIFY(committedLabel);
        QVERIFY(provisional);
        QCOMPARE(committedLabel->text(), committed);
        QVERIFY(provisional->styleSheet().contains(QStringLiteral("#60a5fa")));
        QVERIFY(QRect(provisional->mapTo(&bar, QPoint()), provisional->size())
                    .intersects(bar.rect()));
        QVERIFY(bar.height() <= 240);
        QVERIFY(bar.findChild<QPushButton *>(
            QStringLiteral("floatingCancelButton"))->isVisible());
        QVERIFY(bar.findChild<QPushButton *>(
            QStringLiteral("floatingConfirmButton"))->isVisible());

        bar.setStreamingFallback();
        QCOMPARE(committedLabel->text(), committed);
        QLabel *title = bar.findChild<QLabel *>(
            QStringLiteral("floatingBarTitle")
        );
        QVERIFY(title->text().contains(QString::fromUtf8("整段识别")));
    }

    void actionsFireOnceAndOldSurfaceCannotFireAfterSwitch()
    {
        FloatingBar bar;
        int cancelled = 0;
        int confirmed = 0;
        FloatingBarActions actions;
        actions.cancel = [&cancelled]() { ++cancelled; };
        actions.confirm = [&confirmed]() { ++confirmed; };
        bar.setActions(actions);
        bar.setStyle(floatingBarStyleStatusPill());
        bar.setStage(FloatingBarStage::Recording);
        bar.setWaveformVisible(true);
        bar.show();

        QPushButton *oldCancel = bar.findChild<QPushButton *>(
            QStringLiteral("floatingCancelButton")
        );
        QPushButton *oldConfirm = bar.findChild<QPushButton *>(
            QStringLiteral("floatingConfirmButton")
        );
        QVERIFY(oldCancel);
        QVERIFY(oldConfirm);
        QTest::mouseClick(oldCancel, Qt::LeftButton);
        QTest::mouseClick(oldConfirm, Qt::LeftButton);
        QCOMPARE(cancelled, 1);
        QCOMPARE(confirmed, 1);

        bar.setStage(FloatingBarStage::Completed);
        bar.setStyle(floatingBarStyleLiveTranscriptCard());
        QVERIFY(oldCancel->parent() == nullptr || oldCancel->isHidden());
        QPushButton *newCancel = bar.findChild<QPushButton *>(
            QStringLiteral("floatingCancelButton")
        );
        QVERIFY(newCancel);
        QVERIFY(newCancel != oldCancel);
        bar.setActions(actions);
        bar.setStage(FloatingBarStage::Recording);
        QTest::mouseClick(newCancel, Qt::LeftButton);
        QCOMPARE(cancelled, 2);
    }

    void compatibilityStreamingMethodsRemainDiagnostic()
    {
        FloatingBar bar;
        bar.setStyle(floatingBarStyleLiveTranscriptCard());
        bar.setStreamingFinalizing();
        QLabel *title = bar.findChild<QLabel *>(
            QStringLiteral("floatingBarTitle")
        );
        QVERIFY(title);
        QCOMPARE(title->text(), QString::fromUtf8("正在完成识别"));
        bar.setStreamingFallback();
        QVERIFY(title->text().contains(QString::fromUtf8("整段识别")));
        bar.clearStreamingTranscript();
        QCOMPARE(
            bar.findChild<QLabel *>(
                QStringLiteral("streamingCommittedText"))->text(),
            QString()
        );
    }

    void enlargedFontKeepsRecordingActionsReadable()
    {
        FloatingBar bar;
        QFont large = bar.font();
        large.setPointSize(qMax(18, large.pointSize() + 8));
        bar.setFont(large);
        bar.setStyle(floatingBarStyleLiveTranscriptCard());
        FloatingBarActions actions;
        actions.cancel = []() {};
        actions.confirm = []() {};
        bar.setActions(actions);
        bar.setStage(FloatingBarStage::Streaming);
        bar.setWaveformVisible(true);
        bar.setStreamingTranscript(
            QString::fromUtf8("150% 字体下的已确认文字"),
            QString::fromUtf8("临时文字")
        );
        bar.show();
        QCoreApplication::processEvents();

        QPushButton *cancel = bar.findChild<QPushButton *>(
            QStringLiteral("floatingCancelButton")
        );
        QPushButton *confirm = bar.findChild<QPushButton *>(
            QStringLiteral("floatingConfirmButton")
        );
        QVERIFY(cancel);
        QVERIFY(confirm);
        QVERIFY(cancel->text().isEmpty());
        QVERIFY(confirm->text().isEmpty());
        QVERIFY(!cancel->icon().isNull());
        QVERIFY(!confirm->icon().isNull());
        QVERIFY(cancel->height() >= cancel->fontMetrics().height() + 12);
        QVERIFY(confirm->height() >= confirm->fontMetrics().height() + 12);
        QVERIFY(cancel->geometry().right()
                < confirm->geometry().left());
        QVERIFY(bar.height() <= 260);
    }

    void longTranscriptUsesBoundedScrollAreaWithoutClippingLabels()
    {
        FloatingBar bar;
        bar.setStyle(floatingBarStyleLiveTranscriptCard());
        bar.setStreamingTranscript(
            QString(240, QChar(0x8bc6)),
            QStringLiteral("a long provisional English transcript that wraps")
        );
        bar.show();
        QCoreApplication::processEvents();

        QScrollArea *scroll = bar.findChild<QScrollArea *>(
            QStringLiteral("streamingTranscriptScrollArea"));
        QLabel *committed = bar.findChild<QLabel *>(
            QStringLiteral("streamingCommittedText"));
        QLabel *provisional = bar.findChild<QLabel *>(
            QStringLiteral("streamingProvisionalText"));
        QVERIFY(scroll);
        QVERIFY(committed);
        QVERIFY(provisional);
        QVERIFY(committed->height() >= committed->sizeHint().height());
        QVERIFY(provisional->height() >= provisional->sizeHint().height());
        QVERIFY(scroll->verticalScrollBar()->maximum() > 0);
        QVERIFY(bar.height() <= 240);
    }

    void compactFailureDetailIsNotClipped()
    {
        FloatingBar bar;
        QFont large = bar.font();
        large.setPointSizeF(qMax(15.0, large.pointSizeF() * 1.5));
        bar.setFont(large);
        bar.setStyle(floatingBarStyleStatusPill());
        bar.setStage(
            FloatingBarStage::Failed,
            QString::fromUtf8("识别失败"),
            QString::fromUtf8("请检查 Windows 语言设置")
        );
        bar.show();
        QCoreApplication::processEvents();
        QLabel *detail = bar.findChild<QLabel *>(
            QStringLiteral("floatingBarSubtitle"));
        QLabel *title = bar.findChild<QLabel *>(
            QStringLiteral("floatingBarTitle"));
        QVERIFY(detail);
        QVERIFY(title);
        QVERIFY(detail->height() >= detail->sizeHint().height());
        QVERIFY(title->height() >= title->sizeHint().height());
        QVERIFY(bar.height() <= 180);
    }

    void floatingWindowDoesNotStealFocus()
    {
        FloatingBar bar;
        QVERIFY(bar.testAttribute(Qt::WA_ShowWithoutActivating));
        QVERIFY(bar.windowFlags().testFlag(Qt::WindowDoesNotAcceptFocus));
    }
};

QTEST_MAIN(FloatingBarStreamingTests)
#include "floating_bar_streaming_tests.moc"
