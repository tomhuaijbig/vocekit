#include "vocabulary_quick_add_controller.h"

#include "../input/selected_text_reader.h"
#include "../storage/vocabulary_store.h"
#include "../tasks/vocabulary_suggestion_task.h"

VocabularyQuickAddAccess defaultVocabularyQuickAddAccess()
{
    VocabularyQuickAddAccess access;
    access.readSelectedText = [](
        bool strongSelectionEnabled,
        SelectedTextNativeWindowHandle targetWindow
    ) {
        return SelectedTextReader::read(
            strongSelectionEnabled,
            targetWindow
        );
    };
    access.requestSuggestion = [](
        const VocabularySuggestionTaskRequest &request,
        QString *error
    ) {
        return requestVocabularySuggestion(request, error);
    };
    access.appendEntry = [](VocabularyEntry *entry, QString *error) {
        return appendVocabularyEntry(entry, error);
    };
    return access;
}
