#include "vocabulary_page_access_factory.h"

#include "hub_settings_state.h"

VocabularyPageAccess createVocabularyPageAccess(
    const VocabularyPageAccessFactoryDependencies &dependencies
)
{
    VocabularyPageAccess access;
    HubSettingsState *settings = dependencies.settings;
    access.settingsSnapshotProvider = [settings]() {
        VocabularyPageSettingsSnapshot snapshot;
        if (settings) {
            snapshot.customFunctions = settings->customFunctions();
        }
        return snapshot;
    };
    access.vocabularyAi = dependencies.vocabularyAi
        ? dependencies.vocabularyAi
        : [](
            const QString &,
            const QString &,
            QString *,
            const QString &,
            const QString &
        ) {
            return VocabularySuggestion();
        };
    access.historyEntries = dependencies.historyEntries
        ? dependencies.historyEntries
        : []() { return QVector<HistoryEntry>(); };
    access.vocabularyChanged = dependencies.vocabularyChanged
        ? dependencies.vocabularyChanged
        : [](const QStringList &, bool) {};
    return access;
}
