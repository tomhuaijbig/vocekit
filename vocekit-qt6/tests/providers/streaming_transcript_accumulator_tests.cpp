#include <QtTest>

#include "../../src/providers/streaming_transcript_accumulator.h"

class StreamingTranscriptAccumulatorTests : public QObject
{
    Q_OBJECT

private slots:
    void appendsNumberedCommittedPiecesInSequence()
    {
        StreamingTranscriptAccumulator text;

        QVERIFY(text.appendCommitted(2, QString::fromUtf8("天气")));
        QVERIFY(text.appendCommitted(1, QString::fromUtf8("今天")));

        const StreamingTranscriptSnapshot snapshot = text.snapshot();
        QCOMPARE(
            snapshot.committedText,
            QString::fromUtf8("今天天气")
        );
        QCOMPARE(snapshot.displayText(), snapshot.committedText);
        QCOMPARE(snapshot.revision, quint64(2));
    }

    void replacesInclusiveNumberedRangeWithoutDuplicatingText()
    {
        StreamingTranscriptAccumulator text;
        QVERIFY(text.appendCommitted(1, QString::fromUtf8("今天")));
        QVERIFY(text.appendCommitted(2, QString::fromUtf8("天气")));

        QVERIFY(text.replaceCommittedRange(
            1,
            2,
            3,
            QString::fromUtf8("今天天气很好")
        ));

        QCOMPARE(
            text.snapshot().committedText,
            QString::fromUtf8("今天天气很好")
        );
        QCOMPARE(text.snapshot().revision, quint64(3));
    }

    void rejectsInvalidReplacementWithoutChangingSnapshot()
    {
        StreamingTranscriptAccumulator text;
        QVERIFY(text.appendCommitted(1, QString::fromUtf8("保留")));
        const StreamingTranscriptSnapshot before = text.snapshot();

        QVERIFY(!text.replaceCommittedRange(
            3,
            2,
            4,
            QString::fromUtf8("错误")
        ));

        QCOMPARE(text.snapshot().committedText, before.committedText);
        QCOMPARE(text.snapshot().revision, before.revision);
    }

    void replacesBaiduProvisionalAndCommitsFinalSentence()
    {
        StreamingTranscriptAccumulator text;

        QVERIFY(text.setProvisional(QString::fromUtf8("北京天气怎")));
        QVERIFY(text.setProvisional(QString::fromUtf8("北京天气怎么样")));
        QCOMPARE(
            text.snapshot().provisionalText,
            QString::fromUtf8("北京天气怎么样")
        );
        QVERIFY(text.commitProvisional(
            QString::fromUtf8("北京天气怎么样？")
        ));

        const StreamingTranscriptSnapshot snapshot = text.snapshot();
        QCOMPARE(
            snapshot.committedText,
            QString::fromUtf8("北京天气怎么样？")
        );
        QVERIFY(snapshot.provisionalText.isEmpty());
        QCOMPARE(snapshot.displayText(), snapshot.committedText);
        QCOMPARE(snapshot.revision, quint64(3));
    }

    void sealsPreviousProviderSessionBeforeAcceptingNewPieces()
    {
        StreamingTranscriptAccumulator text;
        QVERIFY(text.appendCommitted(1, QString::fromUtf8("第一段。")));
        QVERIFY(text.sealCurrentSession());
        QVERIFY(text.appendCommitted(1, QString::fromUtf8("第二段。")));

        QCOMPARE(
            text.snapshot().committedText,
            QString::fromUtf8("第一段。第二段。")
        );
        QCOMPARE(text.snapshot().revision, quint64(3));
    }

    void ignoresDuplicateNumberedPieceAndUnchangedProvisional()
    {
        StreamingTranscriptAccumulator text;
        QVERIFY(text.appendCommitted(1, QString::fromUtf8("唯一")));
        QVERIFY(!text.appendCommitted(1, QString::fromUtf8("唯一")));
        QVERIFY(text.setProvisional(QString::fromUtf8("临时")));
        QVERIFY(!text.setProvisional(QString::fromUtf8("临时")));

        QCOMPARE(text.snapshot().revision, quint64(2));
        QCOMPARE(
            text.snapshot().displayText(),
            QString::fromUtf8("唯一临时")
        );
    }
};

QTEST_MAIN(StreamingTranscriptAccumulatorTests)

#include "streaming_transcript_accumulator_tests.moc"
