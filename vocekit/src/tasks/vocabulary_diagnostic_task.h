#ifndef VOCEKIT_VOCABULARY_DIAGNOSTIC_TASK_H
#define VOCEKIT_VOCABULARY_DIAGNOSTIC_TASK_H

#include "../config/app_settings_data.h"

#include <QString>
#include <QStringList>

struct VocabularyDiagnosticRequest
{
    QString storePath;
    bool vocabularyEnabled = false;
    QString vocabularyAddMode;
    bool vocabularyOnlyForVoiceInput = false;
    int vocabularyPromptEntryLimit = 16;
};

VocabularyDiagnosticRequest buildVocabularyDiagnosticRequest(
    const AppSettingsData &settings,
    const QString &storePath
);

QStringList runVocabularyDiagnosticTask(const VocabularyDiagnosticRequest &request);

#endif // VOCEKIT_VOCABULARY_DIAGNOSTIC_TASK_H
