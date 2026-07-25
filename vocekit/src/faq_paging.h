#ifndef FAQ_PAGING_H
#define FAQ_PAGING_H

#include <QString>
#include <QVector>

struct FaqPagingItem
{
    QString searchText;
    QString category;
};

QVector<int> faqVisibleIndexes(
    const QVector<FaqPagingItem> &items,
    const QString &keyword,
    const QString &category,
    int visibleLimit
);
int faqMatchCount(
    const QVector<FaqPagingItem> &items,
    const QString &keyword,
    const QString &category
);
bool faqHasMoreMatches(
    const QVector<FaqPagingItem> &items,
    const QString &keyword,
    const QString &category,
    int visibleLimit
);
int faqNextVisibleLimit(int currentLimit, int totalMatches, int batchSize);

#endif // FAQ_PAGING_H
