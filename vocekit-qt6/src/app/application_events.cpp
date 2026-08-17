#include "application_events.h"

QString functionDefinitionsSettingsKey()
{
    return QStringLiteral("functionDefinitions");
}

QString functionFlowDraftSettingsKey()
{
    return QStringLiteral("functionFlowDraft");
}

QString functionFlowEditorStateSettingsKey()
{
    return QStringLiteral("functionFlowEditorState");
}

QString functionFlowPublishedSettingsKey()
{
    return QStringLiteral("functionFlowPublished");
}

QString functionExecutionModeSettingsKey()
{
    return QStringLiteral("function.executionMode");
}

ApplicationEvents::ApplicationEvents(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<SettingsChangeSet>("SettingsChangeSet");
    qRegisterMetaType<HistoryChangeSet>("HistoryChangeSet");
    qRegisterMetaType<VocabularyChangeSet>("VocabularyChangeSet");
}

void ApplicationEvents::publishSettingsChanged(
    const SettingsChangeSet &change)
{
    Q_EMIT this->settingsChanged(change);
}

void ApplicationEvents::publishHistoryChanged(
    const HistoryChangeSet &change)
{
    Q_EMIT this->historyChanged(change);
}

void ApplicationEvents::publishVocabularyChanged(
    const VocabularyChangeSet &change)
{
    Q_EMIT this->vocabularyChanged(change);
}
