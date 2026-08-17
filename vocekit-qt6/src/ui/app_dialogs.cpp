#include "app_dialogs.h"

#include "attention_message.h"

#include <QEvent>
#include <QWhatsThis>

AppDialog::AppDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
}

HelpDialog::HelpDialog(const QString &helpTitle, const QString &helpText, QWidget *parent)
    : AppDialog(parent), m_helpTitle(helpTitle), m_helpText(helpText)
{
}

bool HelpDialog::event(QEvent *qtEvent)
{
    if (qtEvent->type() == QEvent::EnterWhatsThisMode) {
        QWhatsThis::leaveWhatsThisMode();
        showAttentionInformation(this, m_helpTitle, m_helpText);
        return true;
    }
    return AppDialog::event(qtEvent);
}
