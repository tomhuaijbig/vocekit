#ifndef VOCEKIT_SELECTION_CONTEXT_SETTINGS_CARD_H
#define VOCEKIT_SELECTION_CONTEXT_SETTINGS_CARD_H

#include "../config/app_settings_data.h"
#include "selection_context_action_editor.h"

#include <QFrame>
#include <QMap>
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
        std::function<bool()> confirmRestoreAllSelectionActions;
        std::function<void(const QString &)> validationWarning;
    };

    explicit SelectionContextSettingsCard(
        const SelectionContextSettings &settings,
        const Callbacks &callbacks = Callbacks(),
        QWidget *parent = nullptr
    );

    SelectionContextSettings settings() const;
    void setSettings(const SelectionContextSettings &settings);
    void setCatalogs(const SelectionContextActionEditor::Catalogs &catalogs);
    void rebuildActionEditors();
    void setExpandedAction(const QString &actionId);
    void restoreActionDefaults(const QString &actionId);
    void restoreAllActionDefaults();
    bool applyCustomization(
        const QString &actionId,
        const SelectionContextActionCustomization &value
    );

protected:
    void changeEvent(QEvent *event) override;

private:
    void buildUi();
    void updateButtonMetrics();
    void updateActionListMetrics();
    void updateItemPresentation(const QString &actionId);
    void updateConsentResetVisibility();
    void readWidgets();
    SelectionContextSettings snapshotFromWidgets() const;
    bool wouldHideLastVisibleAction(
        const QString &actionId,
        const SelectionContextActionCustomization &value
    ) const;
    void warnLastVisibleAction();
    void notifyChanged();
    void queueActionOrderChanged();

    SelectionContextSettings m_settings;
    Callbacks m_callbacks;
    SelectionContextActionEditor::Catalogs m_catalogs;
    QMap<QString, SelectionContextActionEditor *> m_actionEditors;
    QString m_expandedActionId;
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
    QPushButton *m_restoreAllActions = nullptr;
    QPushButton *m_resetConsent = nullptr;
    QPushButton *m_strongSelectionLink = nullptr;
};

#endif // VOCEKIT_SELECTION_CONTEXT_SETTINGS_CARD_H
