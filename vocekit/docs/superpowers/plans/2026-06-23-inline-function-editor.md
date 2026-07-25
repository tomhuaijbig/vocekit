# Function Inline Editor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove the separate function editor dialog and make every built-in and custom function editable directly on its command-center page with immediate persistence.

**Architecture:** Keep `AppSettings` as the single source of truth. Add compact identity controls to the function page header, fill the existing input/output accordions with the few settings that only existed in the dialog, and save each field from its own signal handler. All legacy editor entry points navigate to the function page instead of opening a dialog.

**Tech Stack:** Qt 5.9 Widgets, C++11, PowerShell contract tests, qmake and MinGW 5.3.

---

### Task 1: Add the regression contract

**Files:**
- Create: `tests/ui/function_inline_editor_contract.ps1`
- Test: `src/voiceassistant.cpp`

- [ ] Assert that the function page contains an inline name editor for custom functions and an inline `QKeySequenceEdit`.
- [ ] Assert that edits are connected to immediate persistence handlers.
- [ ] Assert that `showFunctionEditorDialog` and the “管理功能” button no longer exist.
- [ ] Run the script and confirm it fails before implementation.

### Task 2: Add inline identity editing

**Files:**
- Modify: `src/voiceassistant.cpp`

- [ ] Replace the title badge and custom management button with a header containing the title/name editor and shortcut editor.
- [ ] Save valid shortcut changes immediately; on empty or conflicting shortcuts, show a warning and restore the previous shortcut.
- [ ] Save custom function name changes immediately; reject an empty name and restore the previous value.
- [ ] Add a custom-function delete action with confirmation, then navigate back to the home page.

### Task 3: Complete the page settings

**Files:**
- Modify: `src/voiceassistant.cpp`

- [ ] Add the long-recording segment length control missing from the current voice accordion.
- [ ] Add recording prompt-sound selection and clearing controls missing from the current voice accordion.
- [ ] Change prompt editing to debounced immediate persistence while respecting prompt locking.
- [ ] Refresh navigation, cards, shortcuts and settings views after identity changes without rebuilding the active editor while the user is typing.

### Task 4: Remove all legacy dialog routes

**Files:**
- Modify: `src/voiceassistant.cpp`

- [ ] Change new custom-function creation to append defaults and open its function page.
- [ ] Change summary-card clicks, edit buttons and home-card double-clicks to open the function page.
- [ ] Delete `showFunctionEditorDialog`.
- [ ] Run the contract test and confirm it passes.

### Task 5: Verify and package

**Files:**
- Verify: `vocekit.pro`
- Verify: `tests/capture`, `tests/ocr`, `tests/recording`, `tests/runtime`

- [ ] Build the release executable with Qt 5.9 MinGW.
- [ ] Run the UI contract, screenshot, OCR, recording and SSL tests.
- [ ] Visually verify built-in shortcut editing, custom name editing, conflict rollback, deletion, and manual accordion expansion.
- [ ] Deploy runtime dependencies and rebuild `dist/vocekit-test.zip`.
