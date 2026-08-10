#ifndef VOCEKIT_STREAMING_TRANSCRIPT_ACCUMULATOR_H
#define VOCEKIT_STREAMING_TRANSCRIPT_ACCUMULATOR_H

#include "streaming_speech_session.h"

#include <QMap>
#include <QStringList>

// 将服务商增量统一归并成完整快照，界面不需要理解服务商协议。
class StreamingTranscriptAccumulator
{
public:
    bool appendCommitted(int sequence, const QString &text);
    bool replaceCommittedRange(
        int firstSequence,
        int lastSequence,
        int newSequence,
        const QString &text
    );
    bool setProvisional(const QString &text);
    bool commitProvisional(const QString &finalText);
    bool sealCurrentSession();

    StreamingTranscriptSnapshot snapshot() const;

private:
    QString currentCommittedText() const;
    void advanceRevision();

    QString m_sealedText;
    QStringList m_linearCommitted;
    QMap<int, QString> m_numberedCommitted;
    QString m_provisional;
    quint64 m_revision = 0;
};

#endif // VOCEKIT_STREAMING_TRANSCRIPT_ACCUMULATOR_H
