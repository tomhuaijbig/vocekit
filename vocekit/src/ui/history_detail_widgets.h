#ifndef VOCEKIT_HISTORY_DETAIL_WIDGETS_H
#define VOCEKIT_HISTORY_DETAIL_WIDGETS_H

#include "../domain/history_types.h"

#include <QString>

class QFrame;
class QScrollArea;
class QVBoxLayout;
class QWidget;

struct HistoryDetailMetaTexts
{
    QString timeText;
    QString elapsedText;
    QString speechElapsedText;
    QString modelElapsedText;
    QString modelText;
    QString ocrElapsedText;
};

// 历史详情页的纯展示控件：元信息卡、文本区块和滚动区域。
QFrame *createHistoryDetailMetaCard(
    const HistoryEntry &entry,
    const HistoryDetailMetaTexts &texts,
    QWidget *parent = nullptr
);

QFrame *createHistoryDetailTextSection(
    const QString &sectionTitle,
    const QString &text,
    int minimumHeight,
    QWidget *parent = nullptr
);

QScrollArea *createHistoryDetailScrollArea(QWidget *contentWidget);

#endif // VOCEKIT_HISTORY_DETAIL_WIDGETS_H
