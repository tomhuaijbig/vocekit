#include "vocabulary_candidates.h"

#include "history_modes.h"
#include "../storage/vocabulary_store.h"

#include <QMap>
#include <QRegExp>
#include <QSet>
#include <QString>
#include <QtGlobal>

#include <algorithm>

namespace {

QString text(const char *value)
{
    return QString::fromUtf8(value);
}

} // namespace

QVector<VocabularyCandidate> buildVocabularyCandidates(const VocabularyCandidateRequest &request)
{
    QMap<QString, VocabularyCandidate> byKey;
    QSet<QString> existingKeys;
    for (const VocabularyEntry &entry : request.existingEntries) {
        existingKeys.insert(vocabularyEntryUniqueKey(entry));
    }

    const int maxHistory = qMin(request.history.size(), qMax(0, request.maxHistoryRecords));
    QRegExp tokenRx(QStringLiteral("[A-Za-z][A-Za-z0-9_.-]{2,}"));
    for (int i = 0; i < maxHistory; ++i) {
        const HistoryEntry &record = request.history.at(i);
        const QString recordText = record.input + QStringLiteral("\n") + record.output;
        int pos = 0;
        while ((pos = tokenRx.indexIn(recordText, pos)) >= 0) {
            QString token = tokenRx.cap(0).trimmed();
            pos += qMax(1, tokenRx.matchedLength());
            token.remove(QRegExp(QStringLiteral("^[._-]+|[._-]+$")));
            if (token.size() < 3 || token == token.toLower()) {
                continue;
            }

            VocabularyEntry entry;
            entry.source = token.toLower();
            entry.target = token;
            entry.scopeId = historyEntryEffectiveModeId(
                record,
                request.customFunctions
            );
            if (entry.scopeId.trimmed().isEmpty()) {
                entry.scopeId = QStringLiteral("__global");
            }
            entry.matchMode = QStringLiteral("caseInsensitive");
            entry.enabled = true;
            entry.note = text("从历史记录候选推荐生成，请保存前确认。");
            if (!vocabularyEntryHasCorrection(entry)) {
                continue;
            }

            const QString key = vocabularyEntryUniqueKey(entry);
            if (existingKeys.contains(key)) {
                continue;
            }

            VocabularyCandidate candidate = byKey.value(key);
            if (candidate.score <= 0) {
                candidate.entry = entry;
                candidate.reason = text("历史记录里多次出现的英文或专有名词大小写。");
            }
            candidate.score += 1;
            byKey.insert(key, candidate);
        }
    }

    QVector<VocabularyCandidate> candidates = QVector<VocabularyCandidate>::fromList(byKey.values());
    std::sort(candidates.begin(), candidates.end(), [](const VocabularyCandidate &a, const VocabularyCandidate &b) {
        if (a.score != b.score) {
            return a.score > b.score;
        }
        return a.entry.target < b.entry.target;
    });
    if (request.maxCandidates >= 0 && candidates.size() > request.maxCandidates) {
        candidates.resize(request.maxCandidates);
    }
    return candidates;
}
