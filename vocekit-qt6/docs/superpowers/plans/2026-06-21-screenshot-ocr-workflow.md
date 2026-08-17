# Screenshot OCR Workflow Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Integrate region screenshot OCR into every built-in and custom function, with configurable triggers and a Youdao-style screenshot comparison result window.

**Architecture:** Add focused capture and screenshot-result modules, extend OCR results with optional text blocks, and route screenshot text through the existing `VoiceController` model pipeline. Function settings remain the source of truth; global hotkeys register separate screenshot IDs without executing heavy work inside the native callback.

**Tech Stack:** Qt 5.9 Widgets, C++11, Windows `RegisterHotKey`, QtConcurrent, RapidOCR helper, Windows OCR helper.

---

### Task 1: Screenshot behavior core

**Files:**
- Create: `src/capture/screenshot_types.h`
- Create: `src/capture/screenshot_types.cpp`
- Create: `tests/capture/screenshot_core_tests.cpp`
- Create: `tests/capture/screenshot_core_tests.pro`

- [ ] Write tests for trigger normalization, logical screenshot hotkey IDs, launcher visibility and line-to-block mapping.
- [ ] Run the capture tests and verify they fail because the new module does not exist.
- [ ] Implement the minimal pure functions.
- [ ] Re-run the capture tests and verify all cases pass.

### Task 2: OCR text blocks

**Files:**
- Modify: `src/ocr/ocr_types.h`
- Modify: `src/ocr/ocr_helper_process.cpp`
- Modify: `helpers/rapidocr/main.cpp`
- Modify: `helpers/windows_ocr/main.cpp`
- Modify: `tests/ocr/fake_ocr_helper.cpp`
- Modify: `tests/ocr/ocr_core_tests.cpp`

- [ ] Add a failing helper-response test with text block coordinates.
- [ ] Extend `OcrResult` with optional `OcrTextBlock` items.
- [ ] Parse optional `blocks` without breaking helpers that only return text.
- [ ] Emit RapidOCR block coordinates and Windows OCR line rectangles.
- [ ] Run OCR tests and confirm existing fallback behavior remains green.

### Task 3: Region capture overlay

**Files:**
- Create: `src/capture/screen_capture_overlay.h`
- Create: `src/capture/screen_capture_overlay.cpp`
- Modify: `vocekit.pro`

- [ ] Implement virtual-desktop capture and region normalization helpers with tests.
- [ ] Implement a frameless overlay with drag selection, size label, confirmation and cancellation.
- [ ] Reject selections smaller than 8 x 8 logical pixels.
- [ ] Save the selected image through a callback; do not perform OCR in the widget.

### Task 4: Screenshot result window and launcher

**Files:**
- Create: `src/capture/screenshot_result_window.h`
- Create: `src/capture/screenshot_result_window.cpp`
- Create: `src/capture/screenshot_launcher.h`
- Create: `src/capture/screenshot_launcher.cpp`
- Modify: `vocekit.pro`

- [ ] Build the draggable launcher that lists functions configured for floating screenshot access.
- [ ] Build the editable screenshot result window with original, overlay and comparison views.
- [ ] Add opacity control, geometry persistence callback and safe fallback when no blocks map to result lines.
- [ ] Add copy, write, replace, regenerate, model retry, follow-up and close callbacks.
- [ ] Verify minimum sizes prevent Chinese labels and bottom actions from clipping.

### Task 5: Function settings and persistence

**Files:**
- Modify: `src/voiceassistant.cpp`
- Modify: `src/modules/settings_panel.inc`
- Modify: `config/settings.example.json`

- [ ] Extend built-in maps and `CustomFunctionDef` with screenshot input, trigger, shortcut and display settings.
- [ ] Read and write all new fields with backward-compatible defaults.
- [ ] Add screenshot controls to the function editor and screenshot shortcut rows to the shortcut settings page.
- [ ] Validate that at least one input source is enabled and separate screenshot shortcuts do not conflict.
- [ ] Refresh home-card summaries after saving.

### Task 6: Hotkey and controller integration

**Files:**
- Modify: `src/voiceassistant.cpp`

- [ ] Register screenshot hotkeys as `screenshot:<functionId>`.
- [ ] Route primary, separate and launcher triggers to a shared screenshot workflow.
- [ ] Configure a dedicated `OcrManager` for screenshot jobs.
- [ ] Feed recognized screenshot text into the existing `LastRunContext`.
- [ ] Continue to voice recording after OCR when the function enables voice input.
- [ ] Preserve the target window before showing the overlay.

### Task 7: Result routing, history and errors

**Files:**
- Modify: `src/voiceassistant.cpp`
- Modify: `src/modules/hub_history_page.inc`

- [ ] Route screenshot output to automatic write, normal result popup or screenshot comparison window.
- [ ] Store OCR engine, OCR elapsed time and screenshot input marker in history details without copying the source image.
- [ ] Add a numbered FAQ entry for capture failure, empty OCR text and unavailable OCR engines.
- [ ] Ensure cancellation does not create an error history entry.

### Task 8: Verification

**Files:**
- Modify: `docs/DEVELOPMENT_LOG.md`
- Modify: `docs/AI_PROJECT_GUIDE.md`

- [ ] Run capture tests.
- [ ] Run OCR tests.
- [ ] Run recording tests.
- [ ] Stop a running `vocekit.exe`, then build Debug and Release.
- [ ] Launch Debug and manually verify capture cancel, capture OCR, screenshot translation, opacity, launcher drag, small-window layout and high-DPI text.
- [ ] Scan for hard-coded keys, user paths and generated screenshots before packaging.
