# Screenshot Workbench Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Keep the frozen desktop visible after selecting a screenshot, automatically run OCR, and expose the existing organize, translate, polish, and summarize tools in a toolbar beside the blue selection.

**Architecture:** `ScreenCaptureOverlay` becomes a two-stage capture workbench. It owns selection movement, resizing, screenshot copying, saving, OCR status, and toolbar interaction, while `VoiceAssistant` keeps ownership of OCR engines, API calls, history, and error handling. Selection changes emit a fresh image and global rectangle only after mouse release, so OCR restarts without repeated work during dragging.

**Tech Stack:** Qt 5.9 Widgets, Qt Network, Qt Concurrent, RapidOCR/Windows OCR helpers, Qt Test.

---

### Task 1: Selection geometry

**Files:**
- Modify: `src/capture/screenshot_types.h`
- Modify: `src/capture/screenshot_types.cpp`
- Test: `tests/capture/screenshot_core_tests.cpp`

- [ ] Add tests for detecting eight resize handles, moving a selection inside desktop bounds, and resizing without creating an invalid rectangle.
- [ ] Run `screenshot_core_tests.exe` and verify the new tests fail because the geometry helpers do not exist.
- [ ] Implement pure geometry helpers used by the overlay.
- [ ] Rebuild and verify all screenshot core tests pass.

### Task 2: Interactive screenshot workbench

**Files:**
- Modify: `src/capture/screen_capture_overlay.h`
- Modify: `src/capture/screen_capture_overlay.cpp`

- [ ] Replace the confirmation-only toolbar with copy screenshot, save screenshot, reselect, close, smart organize, translate, polish, and summarize.
- [ ] Draw a blue selection border with eight blue resize handles.
- [ ] Support moving and resizing the selection; emit the selected image only after release.
- [ ] Add OCR status and enable AI actions only after OCR succeeds.

### Task 3: OCR and AI flow

**Files:**
- Modify: `src/voiceassistant.cpp`

- [ ] Keep the overlay alive after selection instead of closing it.
- [ ] Start OCR automatically after the first selection and restart OCR after a move or resize.
- [ ] Ignore stale OCR results by associating each recognition request with the latest selection.
- [ ] Route smart organize, translate, polish, and summarize through the configured model and display the result inside the selected rectangle.
- [ ] Keep errors on the workbench and preserve the selection so the user can retry.

### Task 4: Verification

**Files:**
- Verify: `tests/capture/screenshot_core_tests.cpp`
- Verify: `tests/ocr/ocr_core_tests.cpp`
- Verify: `vocekit.pro`

- [ ] Run screenshot core tests.
- [ ] Run OCR core tests.
- [ ] Build debug and release application targets.
- [ ] Launch the release build and manually verify selection, automatic OCR, resize OCR, toolbar actions, save, copy, reselect, and close.
