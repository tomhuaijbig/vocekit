#include "streaming_transcript_accumulator.h"

bool StreamingTranscriptAccumulator::appendCommitted(
    int sequence,
    const QString &text
)
{
    if (sequence <= 0 || text.isEmpty()
        || m_numberedCommitted.contains(sequence)) {
        return false;
    }
    m_numberedCommitted.insert(sequence, text);
    advanceRevision();
    return true;
}

bool StreamingTranscriptAccumulator::replaceCommittedRange(
    int firstSequence,
    int lastSequence,
    int newSequence,
    const QString &text
)
{
    if (firstSequence <= 0 || lastSequence < firstSequence
        || newSequence <= 0 || text.isEmpty()) {
        return false;
    }

    for (int sequence = firstSequence;
         sequence <= lastSequence;
         ++sequence) {
        m_numberedCommitted.remove(sequence);
    }
    m_numberedCommitted.insert(newSequence, text);
    advanceRevision();
    return true;
}

bool StreamingTranscriptAccumulator::setProvisional(const QString &text)
{
    if (m_provisional == text) {
        return false;
    }
    m_provisional = text;
    advanceRevision();
    return true;
}

bool StreamingTranscriptAccumulator::commitProvisional(
    const QString &finalText
)
{
    if (finalText.isEmpty() && m_provisional.isEmpty()) {
        return false;
    }
    if (!finalText.isEmpty()) {
        m_linearCommitted.append(finalText);
    }
    m_provisional.clear();
    advanceRevision();
    return true;
}

bool StreamingTranscriptAccumulator::sealCurrentSession()
{
    const QString committed = currentCommittedText();
    if (committed.isEmpty() && m_provisional.isEmpty()) {
        return false;
    }
    m_sealedText += committed;
    m_linearCommitted.clear();
    m_numberedCommitted.clear();
    m_provisional.clear();
    advanceRevision();
    return true;
}

StreamingTranscriptSnapshot
StreamingTranscriptAccumulator::snapshot() const
{
    StreamingTranscriptSnapshot result;
    result.revision = m_revision;
    result.committedText = m_sealedText + currentCommittedText();
    result.provisionalText = m_provisional;
    return result;
}

QString StreamingTranscriptAccumulator::currentCommittedText() const
{
    QString result = m_linearCommitted.join(QString());
    for (auto it = m_numberedCommitted.constBegin();
         it != m_numberedCommitted.constEnd();
         ++it) {
        result += it.value();
    }
    return result;
}

void StreamingTranscriptAccumulator::advanceRevision()
{
    ++m_revision;
}
