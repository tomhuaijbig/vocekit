#include "selected_text_workflow_controller.h"

SelectedTextWorkflowAccess defaultSelectedTextWorkflowAccess()
{
    SelectedTextWorkflowAccess access;
    access.readSelectedText = [](
        const SelectedTextReadRequest &request,
        const VocabularyPreCorrectionCallback &preCorrect
    ) {
        return VoiceInputCollector::readSelectedText(
            request,
            preCorrect
        );
    };
    return access;
}
