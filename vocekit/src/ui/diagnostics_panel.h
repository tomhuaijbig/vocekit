#ifndef VOCEKIT_DIAGNOSTICS_PANEL_H
#define VOCEKIT_DIAGNOSTICS_PANEL_H

#include "../config/secret_config.h"
#include "diagnostics_settings_snapshot.h"

#include <QWidget>
#include <QString>

#include <functional>

class FloatingBar;
class InterfaceSelfCheckCard;
class NetworkDiagnosticsCard;
class QHideEvent;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpacerItem;
class QVBoxLayout;

struct DiagnosticsPanelDefaultCards
{
    typedef std::function<QString()> PathProvider;
    typedef std::function<SecretConfig()> SecretConfigProvider;
    typedef std::function<DiagnosticsSettingsSnapshot()> SettingsProvider;

    FloatingBar *floatingBar = nullptr;
    SettingsProvider settingsProvider;
    PathProvider appBasePathProvider;
    PathProvider recordDirectoryProvider;
    SecretConfigProvider secretsProvider;
    QWidget *vocabularyTestCard = nullptr;
    QWidget *resultPopupTestCard = nullptr;
};

class DiagnosticsPanel : public QWidget
{
public:
    typedef std::function<int(const QString &)> FaqMatchCounter;
    typedef std::function<void(const QString &)> FaqOpener;

    explicit DiagnosticsPanel(
        const FaqMatchCounter &faqMatchCounter = FaqMatchCounter(),
        const FaqOpener &faqOpener = FaqOpener(),
        QWidget *parent = nullptr
    );

    void addTestCard(QWidget *card);
    void addDefaultCards(const DiagnosticsPanelDefaultCards &cards);
    void finalizeCards();
    void setSearchText(const QString &keyword);
    QString searchText() const;
    void refreshSearch();
    void refreshRuntimeTargets();

protected:
    void hideEvent(QHideEvent *event) override;

private:
    FaqMatchCounter m_faqMatchCounter;
    FaqOpener m_faqOpener;
    InterfaceSelfCheckCard *m_interfaceSelfCheckCard = nullptr;
    NetworkDiagnosticsCard *m_networkDiagnosticsCard = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QVBoxLayout *m_itemsLayout = nullptr;
    QPushButton *m_faqMatchButton = nullptr;
    QLabel *m_emptyLabel = nullptr;
    QSpacerItem *m_tailStretch = nullptr;
    bool m_finalized = false;
};

#endif // VOCEKIT_DIAGNOSTICS_PANEL_H
