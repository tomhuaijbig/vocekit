# Selectable Floating Bar And Write Fallback Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add selectable B status-pill and C live-transcript floating bars, with a global default, per-function override, and a default-on result-popup fallback for failed classic auto-write.

**Architecture:** Keep one `FloatingBar` window and one action/state contract, but move its B/C rendering into focused child surfaces selected from normalized settings. Store the global style and write-fallback policy in `AppSettingsData`, store only the per-function style override in `FunctionOutputSettings`, and resolve the effective style before each run. Change the classic write callback to return the existing `ClipboardWriteResult`; on failure, the presentation controller reuses the existing result-popup constructor exactly once, while canvas node fallback remains node-owned.

**Tech Stack:** Qt 5.9, C++11, Qt Widgets, Qt Test, qmake/MinGW, Windows clipboard/input injection.

---

## File map

- `src/config/app_settings_defaults.{h,cpp}`: stable IDs, normalization, display titles, and effective-style resolution.
- `src/config/app_settings_data.h`, `src/domain/function_settings.{h,cpp}`, `src/config/app_settings_json.cpp`: typed configuration, migration defaults, and JSON round-trip.
- `src/ui/floating_bar_style_selector.{h,cpp}`: reusable clickable B/C preview cards; optional `inherit` card for function-level use.
- `src/ui/floating_bar_surface.{h,cpp}`: shared view-state/action contract plus B and C child surfaces.
- `src/ui/floating_bar.h`: existing top-level window, drag persistence, auto-hide, and compatibility methods; delegates visual rendering to the selected surface.
- `src/ui/basic_settings_section.{h,cpp}`, `src/ui/settings_panel.cpp`: global style card selector and default-on write-fallback switch.
- `src/ui/hub_settings_state.{h,cpp}`, `src/domain/app_legacy_types.h`, `src/ui/function_command_page.{h,cpp}`: per-function style override and summary/edit UI.
- `src/output/classic_auto_write_executor.{h,cpp}`: checked classic auto-write, success/failure presentation plan, and one-shot popup fallback through injected callbacks.
- `src/controllers/function_command_controller.cpp`, `src/controllers/voice_controller.cpp`, `src/controllers/voice_recording_workflow_controller.cpp`, `src/controllers/voice_result_presentation_controller.{h,cpp}`: resolve the style per run, map actions, and delegate checked-write fallback.
- `vocekit.pro` and focused `.pro` files: register production files and real dependencies.

### Task 1: Configuration IDs, migration, and effective style

**Files:**
- Modify: `src/config/app_settings_defaults.h`
- Modify: `src/config/app_settings_defaults.cpp`
- Modify: `src/config/app_settings_data.h`
- Modify: `src/domain/function_settings.h`
- Modify: `src/domain/function_settings.cpp`
- Modify: `src/config/app_settings_json.cpp`
- Modify: `tests/config/app_settings_defaults_tests.cpp`
- Modify: `tests/config/app_settings_defaults_tests.pro`
- Modify: `tests/config/app_settings_json_tests.cpp`

- [ ] **Step 1: Write normalization and inheritance tests**

Add tests requiring stable values and safe fallbacks:

```cpp
QCOMPARE(floatingBarStyleStatusPill(), QStringLiteral("statusPill"));
QCOMPARE(floatingBarStyleLiveTranscriptCard(),
         QStringLiteral("liveTranscriptCard"));
QCOMPARE(floatingBarStyleInherit(), QStringLiteral("inherit"));
QCOMPARE(normalizeGlobalFloatingBarStyle(QStringLiteral("bad")),
         floatingBarStyleStatusPill());
QCOMPARE(normalizeFunctionFloatingBarStyle(QString()),
         floatingBarStyleInherit());
QCOMPARE(resolveFloatingBarStyle(floatingBarStyleInherit(),
                                 floatingBarStyleLiveTranscriptCard()),
         floatingBarStyleLiveTranscriptCard());
```

- [ ] **Step 2: Write JSON migration and round-trip tests**

Extend `app_settings_json_tests.cpp` with an old JSON object that lacks all new fields and assert:

```cpp
QCOMPARE(restored.floatingBarStyle, floatingBarStyleStatusPill());
QVERIFY(restored.writeFailurePopupFallbackEnabled);
QCOMPARE(restored.function(QStringLiteral("dictate"))
             .output.floatingBarStyleOverride,
         floatingBarStyleInherit());
```

Then set global C, disable fallback, set one custom function to B, serialize, restore, and assert all three exact values survive without changing `outputMode` or flow JSON.

- [ ] **Step 3: Run the focused tests and verify RED**

Run from `tests/config` with Qt and MinGW on `PATH`:

```powershell
qmake app_settings_defaults_tests.pro -spec win32-g++ "CONFIG+=release"
mingw32-make -j1
.\release\app_settings_defaults_tests.exe -maxwarnings 0
qmake app_settings_json_tests.pro -spec win32-g++ "CONFIG+=release"
mingw32-make -j1
.\release\app_settings_json_tests.exe -maxwarnings 0
```

Expected: compilation fails because the new IDs and fields do not exist.

- [ ] **Step 4: Add the typed settings and normalizers**

Add these declarations and matching C++11 implementations:

```cpp
QString floatingBarStyleStatusPill();
QString floatingBarStyleLiveTranscriptCard();
QString floatingBarStyleInherit();
QString normalizeGlobalFloatingBarStyle(const QString &value);
QString normalizeFunctionFloatingBarStyle(const QString &value);
QString resolveFloatingBarStyle(const QString &overrideValue,
                                const QString &globalValue);
QString floatingBarStyleTitle(const QString &value, bool allowInherit);
```

Add fields with migration-safe defaults:

```cpp
// AppSettingsData
QString floatingBarStyle = QStringLiteral("statusPill");
bool writeFailurePopupFallbackEnabled = true;

// FunctionOutputSettings
QString floatingBarStyleOverride = QStringLiteral("inherit");
```

Normalize the function override in `normalizeFunctionSettings()`.

- [ ] **Step 5: Read and write the exact JSON keys**

At root level use:

```cpp
floatingBarStyle
writeFailurePopupFallbackEnabled
```

Inside each built-in/custom function display settings use:

```cpp
floatingBarStyleOverride
```

On read, absence uses the field defaults above. On write, insert the normalized values and continue preserving unrelated retained root values and orphan flow values.

- [ ] **Step 6: Run GREEN and commit**

Require both test programs to pass and run `git diff --check`, then:

```powershell
git add vocekit/src/config vocekit/src/domain/function_settings.* `
  vocekit/tests/config
git commit -m "feat: persist floating bar preferences"
```

### Task 2: Reusable B/C preview-card selector

**Files:**
- Create: `src/ui/floating_bar_style_selector.h`
- Create: `src/ui/floating_bar_style_selector.cpp`
- Create: `tests/ui/floating_bar_style_selector_tests.cpp`
- Create: `tests/ui/floating_bar_style_selector_tests.pro`
- Modify: `vocekit.pro`

- [ ] **Step 1: Write the real-widget selector tests**

Construct a selector with `allowInherit=false`, assert it has exactly two clickable cards with data `statusPill` and `liveTranscriptCard`, select C, and require one callback with C. Construct another with `allowInherit=true`, assert the third `inherit` card exists, and require keyboard activation (`Space`) and mouse click to produce the same selection.

Also assert every card has a minimum height derived from its font and no fixed height that clips 150% text:

```cpp
QVERIFY(card->minimumHeight() >=
        QFontMetrics(card->font()).height() * 3 + 28);
QVERIFY(card->maximumHeight() == QWIDGETSIZE_MAX);
```

- [ ] **Step 2: Run RED**

Create the test `.pro` with `QT += core gui widgets testlib`, real `ui_style.cpp`, and the new production sources. Build Release/offscreen. Expected: compile failure because `FloatingBarStyleSelector` does not exist.

- [ ] **Step 3: Implement the selector boundary**

Expose only normalized style IDs and a callback:

```cpp
class FloatingBarStyleSelector : public QWidget
{
public:
    struct Options { bool allowInherit = false; };
    explicit FloatingBarStyleSelector(const Options &options,
                                      QWidget *parent = nullptr);
    void setCurrentStyle(const QString &style);
    QString currentStyle() const;
    void setStyleChangedCallback(
        const std::function<void(const QString &)> &callback);
};
```

Use B/C miniature previews containing Chinese labels, wave bars, cancel/check glyphs, and committed/provisional sample text. Mark selected cards with an accessible property and visible border; provide object names `floatingBarStyleCard_statusPill`, `floatingBarStyleCard_liveTranscriptCard`, and optional `_inherit`.

- [ ] **Step 4: Run GREEN and commit**

Require all selector tests to pass under `QT_QPA_PLATFORM=offscreen`, add the two production files to `vocekit.pro`, run `git diff --check`, then commit:

```powershell
git commit -m "feat: add floating bar style cards"
```

### Task 3: B and C runtime surfaces with shared actions

**Files:**
- Create: `src/ui/floating_bar_surface.h`
- Create: `src/ui/floating_bar_surface.cpp`
- Modify: `src/ui/floating_bar.h`
- Modify: `tests/ui/floating_bar_streaming_tests.cpp`
- Modify: `tests/ui/floating_bar_streaming_tests.pro`
- Modify: `vocekit.pro`

- [ ] **Step 1: Write B/C runtime widget tests**

Extend the existing suite to require:

- B base size is compact, contains visible `floatingCancelButton`, `floatingConfirmButton`, and `floatingWaveform`, and hides live text.
- B maps Recording to `正在聆听`, Recognizing to `正在转录`, ModelProcessing to `AI 处理中`, Writing to `正在写入`, and Completed to the supplied completion text.
- C exposes `streamingCommittedText` and `streamingProvisionalText`; provisional text uses blue; long mixed Chinese/English is height-capped and action buttons remain visible.
- `setStreamingFallback()` in C retains the last committed text and shows an accurate batch-recognition status.
- mouse clicks on cancel/confirm invoke their callbacks once; after switching from B to C between runs, the old child cannot invoke callbacks.

- [ ] **Step 2: Run RED**

Build and run `floating_bar_streaming_tests` Release/offscreen. Expected: existing 720 px legacy surface and three text action buttons fail the new B/C/action assertions.

- [ ] **Step 3: Introduce the shared view contract**

Define:

```cpp
enum class FloatingBarStage {
    Preparing, Recording, Recognizing, ModelProcessing, Writing,
    Completed, Failed, Streaming, StreamingFinalizing,
    StreamingFallback
};

struct FloatingBarViewState {
    FloatingBarStage stage = FloatingBarStage::Preparing;
    QString title;
    QString detail;
    QString committedText;
    QString provisionalText;
    int waveformPeak = 0;
    bool waveformVisible = false;
    bool cancelEnabled = false;
    bool confirmEnabled = false;
};

struct FloatingBarActions {
    std::function<void()> cancel;
    std::function<void()> confirm;
};
```

Move the existing `FloatingBarStage` declaration from `floating_bar.h` into `floating_bar_surface.h`; callers continue including it transitively through `floating_bar.h`, while diagnostics that need the enum directly may include the focused header. Implement `StatusPillFloatingBarSurface` and `LiveTranscriptFloatingBarSurface` as child widgets. They render state only; they do not own timers, saved positions, speech sessions, or output routing.

- [ ] **Step 4: Make `FloatingBar` the stable host**

Keep the public compatibility methods used by controllers, but add:

```cpp
void setStyle(const QString &style);
QString style() const;
void setActions(const FloatingBarActions &actions);
void setStage(FloatingBarStage stage,
              const QString &title = QString(),
              const QString &detail = QString());
```

Each existing method updates one stored `FloatingBarViewState` and rerenders the active surface. `setStyle()` uses `normalizeGlobalFloatingBarStyle()`, replaces the child only while no recording is active, and clamps the new host size/position to the saved screen. Remove the inert legacy `复制/撤销/重试` buttons from the floating window; full-result actions remain in the result popup.

- [ ] **Step 5: Run GREEN and commit**

Require all widget tests to pass, including existing streaming-finalizing/fallback expectations adapted to the new surfaces. Run `git diff --check` and commit:

```powershell
git commit -m "feat: add selectable floating bar surfaces"
```

### Task 4: Global settings UI and write-fallback switch

**Files:**
- Modify: `src/ui/basic_settings_section.h`
- Modify: `src/ui/basic_settings_section.cpp`
- Modify: `src/ui/settings_panel.h`
- Modify: `src/ui/settings_panel.cpp`
- Modify: `src/ui/settings_panel_access_factory.h`
- Modify: `src/ui/settings_panel_access_factory.cpp`
- Modify: `src/ui/hub_utility_pages_controller.cpp`
- Modify: `tests/ui/settings_panel_header_tests.cpp`
- Modify: `tests/ui/settings_panel_header_tests.pro`
- Create: `tests/ui/basic_settings_section_floating_bar_tests.cpp`
- Create: `tests/ui/basic_settings_section_floating_bar_tests.pro`

- [ ] **Step 1: Write settings snapshot and real-widget tests**

Extend `BasicSettingsSection::Kind` with `Write` and extend `BasicSettingsSnapshot` with:

```cpp
QString floatingBarStyle;
bool writeFailurePopupFallbackEnabled = true;
```

Test the Voice tab with B selected, click C, and assert `applySnapshot` receives C and `saveAndRefresh` runs once. Click “预览所选样式” and assert `SettingsPanelAccess::previewFloatingBarStyle(C)` runs once. Test the new Write tab contains `writeFailurePopupFallbackToggle`, defaults checked for a missing/old snapshot, and persists false when toggled.

The header/contract test must assert `SettingsPanel` maps both fields in both snapshot directions; do not use only a fragile total-string-count assertion.

- [ ] **Step 2: Run RED**

Build the new widget suite and existing settings header suite Release/offscreen. Expected: selector and fallback toggle are absent.

- [ ] **Step 3: Wire the global controls**

Extend `SettingsPanelAccess` with `std::function<void(const QString &)> previewFloatingBarStyle`. Pass `HubUtilityPagesControllerAccess::floatingBar` into `SettingsPanelAccessFactoryDependencies`, and let the factory callback temporarily call `FloatingBar::setStyle(normalizedStyle)`, `setStage(Recording, ...)`, show a simulated waveform, and auto-hide after five seconds. It must not save a different style, start recording, or change the current target window.

Embed `FloatingBarStyleSelector(allowInherit=false)` below “启用浮动条” on the Voice tab with heading “漂浮窗样式” and explanatory text. Add a “预览所选样式” button that invokes the access callback with the currently selected normalized style.

Add `BasicSettingsSection::addWriteRows()` and a dedicated `SettingsPanel::writeSettingsTab()` labeled “写入”. Put the default-on toggle “写入失败时弹出结果小框” there. Its detail text must list target missing, activation failure, unreliable input position, and injection failure, and state that cancel/recognition/AI failures do not trigger it.

Insert the new tab immediately after “语音录音”. Add a widget test that asserts the exact tab title sequence `常用设置, 词库, 语音录音, 写入, 网络, 历史记录, 快捷键, 接口`. Existing callers currently open only the default index 0; if a future named entry is found during implementation, migrate it to a named enum instead of preserving a shifted magic number.

Map both values in `SettingsPanel::newBasicSettingsSection()` and `refreshFromSettings()` through the typed snapshot; never write JSON directly from the widget.

- [ ] **Step 4: Run GREEN, check clipping, and commit**

Run both suites at the normal test font and with an enlarged test font. Require all labels to have `sizeHint().width()`/word wrap that fits the scroll viewport and no fixed-height clipping. Commit:

```powershell
git commit -m "feat: expose global floating bar settings"
```

### Task 5: Per-function style override and summary

**Files:**
- Modify: `src/domain/app_legacy_types.h`
- Modify: `src/ui/hub_settings_state.h`
- Modify: `src/ui/hub_settings_state.cpp`
- Modify: `src/ui/function_command_page.h`
- Modify: `src/ui/function_command_page.cpp`
- Modify: `src/ui/function_editor_coordinator.cpp`
- Modify: `src/ui/function_summary_formatter.h`
- Modify: `src/ui/function_summary_formatter.cpp`
- Modify: `tests/ui/hub_settings_state_tests.cpp`
- Modify: `tests/ui/function_command_page_tests.cpp`
- Modify: `tests/ui/function_summary_formatter_tests.cpp`

- [ ] **Step 1: Write state conversion and UI tests**

Require `FunctionSettings <-> CustomFunctionDef` to preserve `floatingBarStyleOverride`. Add state accessors:

```cpp
QString floatingBarStyleOverrideFor(const QString &id) const;
void setFloatingBarStyleOverrideFor(const QString &id,
                                    const QString &style);
```

In the real command-page test, open a custom function in classic mode, locate all three function cards (`inherit`, B, C), choose C, assert state is C and `saveSettings` ran once, refresh, and assert C remains selected without another save. Open a built-in function and assert it has no per-function selector, so built-ins always use the global style as specified.

Require the summary to say `漂浮窗：跟随全局`, `状态胶囊`, or `实时文字卡片` for the stored override.

- [ ] **Step 2: Run RED**

Build `hub_settings_state_tests`, `function_command_page_tests`, and `function_summary_formatter_tests` Release/offscreen. Expected: missing field/accessor/cards/summary.

- [ ] **Step 3: Implement the state boundary**

Add `floatingBarStyleOverride` to `CustomFunctionDef`, copy it in both conversion directions, and normalize through `normalizeFunctionFloatingBarStyle()`. Add the two `HubSettingsState` accessors above and refresh custom functions after mutation.

- [ ] **Step 4: Add the function-level selector**

In the custom function's classic output-control area, add a “漂浮窗样式” subsection using `FloatingBarStyleSelector(allowInherit=true)`. Do not add it to built-in functions. Save only the override ID; do not change `outputMode`, global style, or canvas draft. Extend `FunctionSummaryData` with `floatingBarStyleTitle` and render it beside existing floating-bar duration for custom functions.

- [ ] **Step 5: Run GREEN and commit**

Require all three suites and `function_editor_coordinator_tests` to pass. Run `git diff --check`, then:

```powershell
git commit -m "feat: allow per-function floating bar styles"
```

### Task 6: Resolve style per run and wire cancel/confirm actions

**Files:**
- Modify: `src/controllers/function_command_controller.h`
- Modify: `src/controllers/function_command_controller.cpp`
- Modify: `src/controllers/voice_controller.cpp`
- Modify: `src/controllers/voice_recording_workflow_controller.h`
- Modify: `src/controllers/voice_recording_workflow_controller.cpp`
- Modify: `src/app/vocekit_application_runtime.cpp`
- Modify: `tests/controllers/function_command_controller_tests.cpp`
- Modify: `tests/controllers/voice_recording_workflow_controller_tests.cpp`

- [ ] **Step 1: Write style-resolution and action tests**

Change `FunctionCommandAccess::prepareFloatingBar` to receive the effective style:

```cpp
std::function<void(bool enabled,
                   int autoHideMsec,
                   const QString &style)> prepareFloatingBar;
```

Test a custom function using `inherit` with global C receives C, a custom function override B receives B, a built-in function with even an accidentally stored C override still follows global B, and invalid global values resolve B. In recording tests, click/call confirm while recording and require the existing `stopAndProcess()` path exactly once; call cancel and require capture/session cancellation with no batch fallback, AI execution, write, or result popup.

Define the public UI-action boundary before wiring:

```cpp
bool confirmActiveRecording();
bool cancelActiveRecording();
```

Both return `true` only when they consumed an active preparation/recording/finalizing run; repeated or stale calls return `false` and have no side effects.

- [ ] **Step 2: Run RED**

Build both controller suites. Expected: callback signature mismatch and missing floating action handlers.

- [ ] **Step 3: Apply style before showing each run**

In `FunctionCommandController`, compute:

```cpp
const QString functionStyle = function.builtIn
    ? floatingBarStyleInherit()
    : function.output.floatingBarStyleOverride;
const QString style = resolveFloatingBarStyle(
    functionStyle,
    m_settings.floatingBarStyle);
```

Pass it with enabled/duration to `VoiceController`, which calls `m_bar->setStyle(style)` before the first status. Update the tray preview to use the current global style.

- [ ] **Step 4: Map UI actions to the active recording lifecycle**

Implement `VoiceRecordingWorkflowController::confirmActiveRecording()` and `cancelActiveRecording()` as thin delegates to `Impl`. At recording start, install `FloatingBarActions` with `QPointer<VoiceRecordingWorkflowController>` and generation-guarded callbacks; confirm reuses the same release/stop route as the configured trigger mode, and cancel reuses the existing explicit cancellation route. Clear actions on terminal state, destruction, or generation change.

- [ ] **Step 5: Run GREEN and commit**

Require both controller suites plus `floating_bar_streaming_tests` to pass without duplicate stop/cancel counts or QObject/timer warnings. Commit:

```powershell
git commit -m "feat: apply floating bar styles per run"
```

### Task 7: Checked classic auto-write and popup fallback

**Files:**
- Create: `src/output/classic_auto_write_executor.h`
- Create: `src/output/classic_auto_write_executor.cpp`
- Modify: `src/controllers/voice_result_presentation_controller.h`
- Modify: `src/controllers/voice_result_presentation_controller.cpp`
- Modify: `src/controllers/voice_controller.cpp`
- Modify: `src/output/voice_result_output_dispatcher.h`
- Modify: `src/output/voice_result_output_dispatcher.cpp`
- Create: `tests/output/classic_auto_write_executor_tests.cpp`
- Create: `tests/output/classic_auto_write_executor_tests.pro`
- Modify: `tests/controllers/voice_result_presentation_controller_tests.cpp`
- Modify: `tests/controllers/voice_result_presentation_controller_tests.pro`
- Modify: `tests/result_flow/result_flow_tests.cpp`

- [ ] **Step 1: Define the focused executor contract in a failing test**

Create a Qt Core test linked to the real new executor source, `clipboard_writer.h`, and no window/network implementation. Define the intended contract:

```cpp
struct ClassicAutoWriteRequest {
    QString text;
    bool replaceSelection = true;
    bool hasSelection = false;
    bool popupFallbackEnabled = true;
};

struct ClassicAutoWriteAccess {
    std::function<ClipboardWriteResult(
        const QString &, bool, bool)> checkedWrite;
    std::function<void(const QString &, const QString &)> setStatus;
    std::function<void(const QString &)> showFallbackPopup;
    std::function<void(const QString &, const QString &)> log;
};

class ClassicAutoWriteExecutor {
public:
    static ClipboardWriteResult execute(
        const ClassicAutoWriteRequest &request,
        const ClassicAutoWriteAccess &access);
};
```

Use fakes to cover each error code:

```cpp
flow_target_window_unavailable
flow_target_window_activation_failed
flow_input_injection_failed
flow_clipboard_unavailable
```

For each, assert one checked write, no “已写入” status, and exactly one popup containing the complete output; the log contains the error code but never the output text. Also assert fallback-disabled and success produce zero fallback popups. Cancellation, recognition failure, and model failure never call this executor and are covered at their existing controller entry points.

- [ ] **Step 2: Run RED**

Build/run `classic_auto_write_executor_tests`. Expected: compile failure because the executor does not exist.

- [ ] **Step 3: Implement the executor and run GREEN**

Implement the executor so missing `checkedWrite` returns `flow_auto_write_failed`; success emits only done status/log; failure emits one failure status/log and, only when enabled, calls `showFallbackPopup(request.text)` once. It never retains state across executions and never owns a window.

- [ ] **Step 4: Return the real checked-write result and delegate from presentation**

In `VoiceController`, wire:

```cpp
return ClipboardWriter::pasteTextToWindowChecked(
    text,
    m_functionCommands ? m_functionCommands->targetWindow() : nullptr,
    replaceSelection,
    hasSelection);
```

Change `VoiceResultPresentationAccess::writeText` to return `ClipboardWriteResult`. In `present()`, build `ClassicAutoWriteRequest`, map `checkedWrite` to `m_access.writeText`, and map `showFallbackPopup` to `showResultPopup(context, text)`. Only hide as successful when `result.ok`; on failure let the existing popup own the preserved result. Because `present()` dispatches one final output once and the executor invokes the popup callback at most once, one execution creates at most one fallback popup.

Use the existing popup presentation/configuration path so complete text, target window, selected-text state, actions, geometry, opacity, timeout, retry, and draft behavior remain unchanged. The UI message may say `未能自动写入，结果已保留` plus the mapped failure reason; do not expose raw internal codes as the only user text.

- [ ] **Step 5: Add a real delegation contract test and preserve canvas ownership**

Update the existing presentation-controller source contract test to require `ClassicAutoWriteExecutor::execute` and forbid the old unchecked `void writeText` signature. This remains a wiring test; behavior is covered through the real executor, avoiding duplicate test symbols or a fake copy of the algorithm.

Run `function_flow_result_controller_tests` and `result_flow_tests`. Require node-level `fallbackToPopup`, explicit-popup deduplication, draft immutability, and existing error codes to remain unchanged; do not read the global classic fallback flag inside `FunctionFlowResultController`.

- [ ] **Step 6: Run GREEN and commit**

Require executor, presentation contract, clipboard writer, result flow, and function flow result suites to pass. Register the executor in `vocekit.pro`, run `git diff --check`, then:

```powershell
git commit -m "fix: preserve results when automatic write fails"
```

### Task 8: Full regression, visual gates, package, and deployment

**Files:**
- Modify only if a verified build/test defect is found: `vocekit.pro`, affected test `.pro`, or files from Tasks 1-7
- Build output: `build/selectable-floating-bar-release/`
- Visual proof: `build/selectable-floating-bar-visual/`
- Backup output: `build/selectable-floating-bar-backup-<timestamp>/`

- [ ] **Step 1: Run focused suites from clean Release builds**

Run at minimum:

```text
app_settings_defaults_tests
app_settings_json_tests
floating_bar_style_selector_tests
floating_bar_streaming_tests
basic_settings_section_floating_bar_tests
settings_panel_header_tests
hub_settings_state_tests
function_command_page_tests
function_summary_formatter_tests
function_command_controller_tests
voice_recording_workflow_controller_tests
voice_result_presentation_controller_tests
function_flow_result_controller_tests
result_flow_tests
clipboard_writer_tests
```

Expected: all pass, zero skipped network/key-dependent cases, no real API calls.

- [ ] **Step 2: Run the full repository suite**

```powershell
powershell -ExecutionPolicy Bypass -File `
  .\vocekit\scripts\run-all-tests.ps1 -Configuration release
```

Expected: `Failed = 0`, `Skipped = 0`, `InfrastructureFailures = 0`. If isolated checkout lacks ignored OCR assets, stage read-only temporary junctions, report them explicitly, and remove them in `finally`.

- [ ] **Step 3: Build the main Release target in an isolated directory**

Use Qt 5.9/MinGW with separate `OBJECTS_DIR`, `MOC_DIR`, `RCC_DIR`, `UI_DIR`, and `DESTDIR`. Expected: qmake, compile, moc, and final link all exit 0. Record executable size and SHA-256.

- [ ] **Step 4: Produce real-widget visual proof**

Build a Qt harness from the real production selector/surfaces/settings widgets and save PNGs for:

```text
B recording
B transcribing/model/writing
C committed + provisional text
C long Chinese/English text
global B/C cards
function inherit/B/C cards
write-fallback switch and detail
```

Capture normal and compact windows at 100%, plus enlarged-font equivalents representing 125% and 150%. Inspect every PNG: no clipped Chinese, no overlapping cancel/check buttons, selected borders visible, C height bounded, and compact screens remain reachable by scrolling.

- [ ] **Step 5: Package and isolated-start test**

Stage `vocekit.exe`, Qt Core/Gui/Widgets/Network/Multimedia/WebSockets/Svg, platform/media plugins, MinGW runtime, OpenSSL, and graphics runtime dependencies. Start the package with `PATH=C:\Windows\System32;C:\Windows`; require the process remains alive and logs show the selected style/fallback settings without missing-DLL errors.

- [ ] **Step 6: Safely deploy after backup**

Verify the exact currently running executable path, back up the deployed `release/` directory and root configuration with hashes, stop only that process, copy the verified package, and restart. Do not overwrite secrets. Confirm the existing `settings.json` migrates to B/default-on only when keys are absent, and that later user selections persist after restart.

- [ ] **Step 7: Final Git and behavior audit**

Require:

```powershell
git diff --check
git status --short
```

Tracked/staged changes must match planned files; preserve pre-existing untracked build artifacts. Recheck one B run, one C run, successful auto-write, and a deterministic invalid-target run that opens exactly one result popup. Commit any verified final project-registration-only change separately, then report all commits, test totals, screenshots, executable hash, backup path, deployment path, and the one remaining user-only live speech/input test if automation cannot supply real speech.
