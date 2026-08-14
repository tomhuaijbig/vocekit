#ifndef VOCEKIT_SELECTED_TEXT_READER_H
#define VOCEKIT_SELECTED_TEXT_READER_H

#include "selection_snapshot.h"

#include <QString>

// Reads selected text without exposing Windows-specific APIs to callers.
class SelectedTextReader
{
public:
    static QString read(
        bool strongSelectionEnabled = false,
        SelectedTextNativeWindowHandle window = nullptr
    );
    static bool hasSelectionInWindow(
        SelectedTextNativeWindowHandle window
    );
    static SelectionPhysicalProbeResult probeUiAutomationPhysical(
        const SelectionProbeRequest &request
    );
};

#endif // VOCEKIT_SELECTED_TEXT_READER_H
