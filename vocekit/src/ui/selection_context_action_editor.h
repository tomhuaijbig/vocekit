#ifndef VOCEKIT_SELECTION_CONTEXT_ACTION_EDITOR_H
#define VOCEKIT_SELECTION_CONTEXT_ACTION_EDITOR_H

#include "../config/selection_context_action_customization.h"

#include <QFrame>
#include <QPair>
#include <QQueue>
#include <QVector>

#include <functional>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QToolButton;
class QWidget;

class SelectionContextActionEditor : public QFrame
{
public:
    struct Catalogs {
        QVector<QPair<QString, QString>> models;
        QVector<QPair<QString, QString>> vocabularyScopes;
        QVector<QPair<QString, QString>> targetLanguages;
    };

    struct Callbacks {
        std::function<void(const SelectionContextActionCustomization &)> changed;
        std::function<void()> restoreRequested;
        std::function<void(const QString &)> validationWarning;
    };

    explicit SelectionContextActionEditor(
        const QString &actionId,
        const Catalogs &catalogs = Catalogs(),
        const Callbacks &callbacks = Callbacks(),
        QWidget *parent = nullptr
    );

    void setCustomization(const SelectionContextActionCustomization &value);
    SelectionContextActionCustomization customization() const;
    void setExpanded(bool expanded);
    bool isExpanded() const;

private:
    void notifyChanged();
    void scheduleChangeDrain();
    void drainOnePendingChange(quint64 generation);
    void updatePromptCount(int length);
    void selectCatalogValue(
        QComboBox *combo,
        const QString &value,
        const QString &unavailableSuffix,
        bool allowEditableValue
    );

    QString actionId_;
    Callbacks callbacks_;
    SelectionContextActionCustomization value_;
    SelectionContextActionCustomization lastNotifiedValue_;
    bool hasLastNotifiedValue_ = false;
    bool updating_ = false;
    bool expanded_ = false;
    QString lastValidPrompt_;
    QQueue<SelectionContextActionCustomization> pendingChanges_;
    quint64 changeDeliveryGeneration_ = 0;
    bool changeDrainScheduled_ = false;

    QLineEdit *displayNameEdit_ = nullptr;
    QLineEdit *usageHintEdit_ = nullptr;
    QCheckBox *visibleCheck_ = nullptr;
    QToolButton *expandButton_ = nullptr;
    QWidget *specificFields_ = nullptr;
    QComboBox *modelCombo_ = nullptr;
    QPlainTextEdit *promptEdit_ = nullptr;
    QLabel *promptCountLabel_ = nullptr;
    QComboBox *targetLanguageCombo_ = nullptr;
    QComboBox *vocabularyScopeCombo_ = nullptr;
    QComboBox *copyModeCombo_ = nullptr;
};

#endif // VOCEKIT_SELECTION_CONTEXT_ACTION_EDITOR_H
