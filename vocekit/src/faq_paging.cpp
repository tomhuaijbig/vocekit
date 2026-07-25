#include "faq_paging.h"

#include <QtGlobal>

static bool faqItemMatches(
    const FaqPagingItem &item,
    const QString &keyword,
    const QString &category
)
{
    const bool categoryMatched = category.isEmpty()
        || category == QStringLiteral("all")
        || item.category == category;
    return categoryMatched
        && (keyword.isEmpty() || item.searchText.contains(keyword, Qt::CaseInsensitive));
}

QVector<int> faqVisibleIndexes(
    const QVector<FaqPagingItem> &items,
    const QString &keyword,
    const QString &category,
    int visibleLimit
)
{
    QVector<int> indexes;
    const int limit = qMax(0, visibleLimit);
    indexes.reserve(qMin(limit, items.size()));
    for (int i = 0; i < items.size() && indexes.size() < limit; ++i) {
        if (faqItemMatches(items.at(i), keyword, category)) {
            indexes.append(i);
        }
    }
    return indexes;
}

int faqMatchCount(
    const QVector<FaqPagingItem> &items,
    const QString &keyword,
    const QString &category
)
{
    int count = 0;
    for (const FaqPagingItem &item : items) {
        if (faqItemMatches(item, keyword, category)) {
            ++count;
        }
    }
    return count;
}

bool faqHasMoreMatches(
    const QVector<FaqPagingItem> &items,
    const QString &keyword,
    const QString &category,
    int visibleLimit
)
{
    return faqMatchCount(items, keyword, category) > qMax(0, visibleLimit);
}

int faqNextVisibleLimit(int currentLimit, int totalMatches, int batchSize)
{
    return qMin(qMax(0, totalMatches), qMax(0, currentLimit) + qMax(1, batchSize));
}
