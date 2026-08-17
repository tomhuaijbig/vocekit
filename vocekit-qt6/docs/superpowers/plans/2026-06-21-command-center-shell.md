# vocekit Command Center Shell Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the current main-window navigation shell with a Qt command-center layout while preserving all existing business pages and settings behavior.

**Architecture:** Keep `HubWindow` as the integration owner, but move command-center navigation and shared styling into a focused include module. The left rail owns function actions, the top toolbar owns tool pages, and the existing `QStackedWidget` continues to host the established pages.

**Tech Stack:** Qt 5.9 Widgets, C++11, QStackedWidget, QScrollArea, QPropertyAnimation/QGraphicsOpacityEffect.

---

### Task 1: Command-center navigation model

**Files:**
- Create: `src/modules/command_center_shell.inc`
- Modify: `src/voiceassistant.cpp`
- Modify: `vocekit.pro`

- [ ] Add shared command-center color, button and scroll-area style helpers.
- [ ] Build the left function rail with home, built-in functions, custom-function scroll area and add-function action.
- [ ] Build the top toolbar with search, tool-page buttons and help entry.
- [ ] Wire function actions to existing built-in/custom editors and wire tool actions to `selectPage`.
- [ ] Refresh custom-function buttons whenever settings change.

### Task 2: Main-window composition

**Files:**
- Modify: `src/voiceassistant.cpp`

- [ ] Replace the old horizontal root with sidebar plus a shell containing top toolbar and page stack.
- [ ] Remove duplicate tool-page buttons from the left rail.
- [ ] Keep the existing page order and all page-refresh side effects.
- [ ] Synchronize active states between the current page, left function selection and top tool selection.

### Task 3: Shared visual normalization

**Files:**
- Modify: `src/voiceassistant.cpp`
- Modify: `src/modules/settings_panel.inc`
- Modify: `DESIGN.md`

- [ ] Apply the command-center background, hairline, typography and compact radius tokens to the main shell.
- [ ] Normalize page title margins and the main search/input/button vocabulary without changing page behavior.
- [ ] Ensure local scroll areas remain local and do not introduce nested main-window scrolling.
- [ ] Record the command-center tokens and motion rules in `DESIGN.md`.

### Task 4: Restrained Qt transitions

**Files:**
- Create: `src/modules/command_center_shell.inc`
- Modify: `src/voiceassistant.cpp`

- [ ] Add a reusable page/list fade helper with a 180ms OutCubic transition.
- [ ] Apply it to page changes and custom-function rail refreshes.
- [ ] Prevent repeated transitions from accumulating graphics effects or animations.

### Task 5: Build and visual verification

**Files:**
- Test: `vocekit.pro`
- Test: existing Qt tests under `tests/`

- [ ] Run qmake and Debug build with the installed Qt 5.9 MinGW toolchain.
- [ ] Run the existing test executables.
- [ ] Launch the application and verify left function navigation, top tools, settings, history and custom-function editing.
- [ ] Capture small-window and maximized screenshots and check Chinese text clipping, overlap and scroll behavior.
- [ ] Run Release build after the UI smoke test.

## Self-review

- Spec coverage: navigation split, page reuse, custom-function scrolling, restrained motion, responsive clipping and build verification are all covered.
- Placeholder scan: no implementation placeholders remain.
- Type consistency: the plan reuses `HubWindow`, `selectPage`, `refreshCustomFunctionsPage` and existing editor methods instead of introducing conflicting business types.
