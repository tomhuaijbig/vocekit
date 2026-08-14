#ifndef VOCEKIT_SELECTION_CONTEXT_SETTINGS_CARD_H
#define VOCEKIT_SELECTION_CONTEXT_SETTINGS_CARD_H

#include "../config/app_settings_data.h"

#include <QFrame>
#include <QPair>
#include <QVector>

#include <functional>

class QCheckBox;
class QListWidget;
class QPushButton;
class QSpinBox;
class QTextEdit;

class SelectionContextSettingsCard : public QFrame
{
public:
    struct Callbacks
    {
        std::function<void(const SelectionContextSettings &)> settingsChanged;
        std::function<void()> showStrongSelectionDetails;
    };

    explicit SelectionContextSettingsCard(
        const SelectionContextSettings &settings,
        const Callbacks &callbacks = Callbacks(),
        QWidget *parent = nullptr
    );

    SelectionContextSettings settings() const;
    void setSettings(const SelectionContextSettings &settings);
    void setActionCatalog(
        const QVector<QPair<QString, QString>> &catalog
    );

protected:
    void changeEvent(QEvent *event) override;

private:
    void buildUi();
    void rebuildActionList();
    void updateButtonMetrics();
    void updateConsentResetVisibility();
    void readWidgets();
    void notifyChanged();
    void queueActionOrderChanged();

    SelectionContextSettings m_settings;
    Callbacks m_callbacks;
    QVector<QPair<QString, QString>> m_actionCatalog;
    bool m_updating = false;
    bool m_actionChangeQueued = false;
    QCheckBox *m_enabled = nullptr;
    QCheckBox *m_keyboard = nullptr;
    QSpinBox *m_minimumLength = nullptr;
    QCheckBox *m_closeOutside = nullptr;
    QCheckBox *m_pin = nullptr;
    QSpinBox *m_pauseMinutes = nullptr;
    QListWidget *m_actions = nullptr;
    QTextEdit *m_blockedApplications = nullptr;
    QPushButton *m_resetConsent = nullptr;
    QPushButton *m_strongSelectionLink = nullptr;
};

#endif // VOCEKIT_SELECTION_CONTEXT_SETTINGS_CARD_H
