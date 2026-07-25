#ifndef ATTENTION_MESSAGE_H
#define ATTENTION_MESSAGE_H

#include <functional>

#include <QString>

class QWidget;

using AttentionFaqCallback = std::function<void(const QString &)>;

void setAttentionFaqCallback(const AttentionFaqCallback &callback);
QString attentionFaqIdForTitle(const QString &title);
void showAttentionWarning(QWidget *parent, const QString &title, const QString &text);
void showAttentionInformation(QWidget *parent, const QString &title, const QString &text);

#endif
