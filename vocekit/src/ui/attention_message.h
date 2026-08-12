#ifndef ATTENTION_MESSAGE_H
#define ATTENTION_MESSAGE_H

#include <functional>

#include <QString>

class QWidget;

using AttentionFaqCallback = std::function<void(const QString &)>;
using AttentionActionCallback = std::function<void()>;
#ifdef VOCEKIT_TESTING
using AttentionActionDialogCallback = std::function<bool(
    QWidget *,
    const QString &,
    const QString &,
    const QString &
)>;
#endif

void setAttentionFaqCallback(const AttentionFaqCallback &callback);
QString attentionFaqIdForTitle(const QString &title);
void showAttentionWarning(QWidget *parent, const QString &title, const QString &text);
void showAttentionInformation(QWidget *parent, const QString &title, const QString &text);
void showAttentionWarningWithAction(
    QWidget *parent,
    const QString &title,
    const QString &text,
    const QString &actionText,
    const AttentionActionCallback &action
);
#ifdef VOCEKIT_TESTING
void setAttentionActionDialogCallbackForTests(
    const AttentionActionDialogCallback &callback
);
void setAttentionMessageBoxClickCallbackForTests(
    const std::function<void(QWidget *)> &callback
);
#endif

#endif
