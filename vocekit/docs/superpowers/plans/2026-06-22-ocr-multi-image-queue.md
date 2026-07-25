# OCR Multi-Image Queue Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Allow the image-recognition page to select and process many images sequentially, switch between images, and preserve an independent editable result for each image.

**Architecture:** Add a focused `OcrBatchQueue` state module that owns image paths, current index, per-image status, result, and error. The existing `OcrManager` remains single-task and processes one image at a time; the page advances to the next pending image after each result, keeping memory bounded. Image validation is relaxed to 200 MB and 120 million pixels per image while still rejecting corrupt or pathological files.

**Tech Stack:** Qt 5.9 Widgets, Qt Test, C++11, existing `OcrManager`.

---

### Task 1: Multi-image queue state

**Files:**
- Create: `src/ocr/ocr_batch_queue.h`
- Create: `src/ocr/ocr_batch_queue.cpp`
- Modify: `tests/ocr/ocr_core_tests.cpp`
- Modify: `tests/ocr/ocr_core_tests.pro`

- [ ] Write failing tests covering path de-duplication, previous/next navigation, independent editable text, result/error storage, and locating the next pending image.
- [ ] Run `ocr_core_tests.exe` and verify the new tests fail because `OcrBatchQueue` does not exist.
- [ ] Implement `OcrBatchQueue` with sequential, bounded state only; do not load image pixels.
- [ ] Rebuild and verify all OCR tests pass.

### Task 2: Relax practical image limits

**Files:**
- Modify: `src/ocr/ocr_types.h`
- Modify: `tests/ocr/ocr_core_tests.cpp`

- [ ] Change validation tests to accept images wider than 8000 pixels and reject files over 200 MB or images over 120 million pixels.
- [ ] Run tests and verify they fail against the old 25 MB / 8000 pixel limits.
- [ ] Implement the new limits and clear Chinese error messages.
- [ ] Re-run OCR tests.

### Task 3: Multi-image page controls and result switching

**Files:**
- Modify: `src/voiceassistant.cpp`
- Modify: `src/modules/hub_ocr_page.inc`
- Modify: `vocekit.pro`

- [ ] Replace single-file selection with `QFileDialog::getOpenFileNames`.
- [ ] Add previous/next controls and an `当前张数 / 总张数` label below the preview.
- [ ] Save current edited text before switching images and restore the selected image's own text/status.
- [ ] Start a sequential batch from the current image, then process each remaining pending image automatically.
- [ ] Keep `OcrManager` single-task; cancellation stops the current task and the remaining queue.
- [ ] Save history once per completed or failed image using that item's path.

### Task 4: Verification

**Files:**
- Verify: `release/vocekit.exe`
- Verify: `debug/vocekit.exe`

- [ ] Build Debug and Release.
- [ ] Run OCR, screenshot, recording, and FAQ pagination tests.
- [ ] Manually select multiple images, start recognition, switch previous/next, edit one result, switch away and back, and confirm the edit remains associated with that image.
- [ ] Confirm selecting many images does not decode them all into memory at once.
