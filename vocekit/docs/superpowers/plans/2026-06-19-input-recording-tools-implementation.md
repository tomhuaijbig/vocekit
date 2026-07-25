# vocekit Input, Recording and Test Tools Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add safe input/write tests, optional microphone sample retention, selectable floating-bar states, hold-to-talk and resilient segmented recordings longer than 60 seconds.

**Architecture:** Keep low-level keyboard state, recording segmentation and UI tests in separate modules. The keyboard hook only emits press/release events; `SegmentedRecordingController` owns segment numbering, serial recognition, retry and final merge. Existing toggle recording remains the default and is not rewritten.

**Tech Stack:** Qt 5.9 Widgets/Multimedia/Concurrent, Windows low-level keyboard hook, existing `AudioRecorder`, `ApiClient`, `FloatingBar`, history and runtime logging.

---

## File Map

- Create `src/input/hold_to_talk.h/.cpp`: low-level press/release tracking.
- Create `src/recording/segmented_recording.h/.cpp`: segment state and result ordering.
- Create `src/modules/hub_input_tests.inc`: selection and write tests.
- Create `tests/recording/recording_core_tests.cpp/.pro`: deterministic state tests.
- Modify `src/modules/audio_recorder.inc`, `floating_bar.inc`, `settings_panel.inc`, `hub_history_page.inc`.
- Modify `src/voiceassistant.cpp`, settings examples, FAQ and packaging docs.

### Task 1: Recording Configuration and State Types

**Files:**
- Create: `src/recording/segmented_recording.h`
- Create: `tests/recording/recording_core_tests.cpp`
- Create: `tests/recording/recording_core_tests.pro`
- Modify: `src/voiceassistant.cpp`
- Modify: `config/settings.example.json`
- Modify: `vocekit.pro`

- [x] **Step 1: Add failing defaults tests**

```cpp
QCOMPARE(settings.recordingTriggerModeFor("dictate"), QStringLiteral("toggle"));
QVERIFY(!settings.longRecordingEnabledFor("dictate"));
QCOMPARE(settings.segmentSecondsFor("dictate"), 55);
QCOMPARE(settings.maxRecordingMinutesFor("dictate"), 30);
```

- [x] **Step 2: Add settings fields**

Built-in and custom functions persist:

```json
{
  "recordingTriggerMode": "toggle",
  "longRecordingEnabled": false,
  "segmentSeconds": 55,
  "maxRecordingMinutes": 30
}
```

Clamp segment seconds to 20–55 and max minutes to 1–30.

- [x] **Step 3: Define segment types**

```cpp
struct RecordingSegment {
    int index = 0;
    QString wavPath;
    QString text;
    QString error;
    qint64 recognitionElapsedMs = -1;
    int attempts = 0;
};
```

- [x] **Step 4: Run tests and main build**

Expected: defaults load for old configurations without changing existing behavior.

- [ ] **Step 5: Commit**

```bash
git add src/recording tests/recording src/voiceassistant.cpp config/settings.example.json vocekit.pro
git commit -m "feat: add recording mode configuration"
```

### Task 2: Selection and Write Tests

**Files:**
- Create: `src/modules/hub_input_tests.inc`
- Modify: `src/voiceassistant.cpp`

- [x] **Step 1: Add test cards**

Add “选中文字测试” and “写入测试” to the existing test-tool layout and search index.

- [x] **Step 2: Implement selection test**

Hide the hub for five seconds, display a visible countdown on the floating bar, then call ordinary selection reading. Provide a second explicit “强力选中测试” action instead of invoking strong selection automatically.

- [x] **Step 3: Implement internal write test**

Create a card containing a `QTextEdit`. Buttons:

```text
写入光标
替换选中
恢复测试内容
```

Use existing `ClipboardBridge`, but target only the embedded test editor.

- [x] **Step 4: Verify no external side effects**

Keep Notepad focused while pressing the test buttons. Notepad must remain unchanged.

- [ ] **Step 5: Add FAQ entries and commit**

```bash
git add src/modules/hub_input_tests.inc src/voiceassistant.cpp
git commit -m "feat: add selection and write diagnostics"
```

### Task 3: Microphone Test Sample Retention

**Files:**
- Modify: `src/voiceassistant.cpp`
- Modify: `src/modules/audio_recorder.inc`

- [x] **Step 1: Add a “保留测试样本” switch**

Default false. When false, record in `QDir::tempPath()` and delete after measurement.

- [x] **Step 2: Save retained samples**

Path:

```text
历史记录/测试样本/麦克风/YYYY-MM-DD/microphone_HHmmss.wav
```

- [x] **Step 3: Show metrics**

Display peak percentage, average percentage, clipping state and saved path. Calculate from signed 16-bit PCM.

- [x] **Step 4: Verify both modes**

Temporary mode leaves no WAV. Retain mode leaves one playable WAV.

- [ ] **Step 5: Commit**

```bash
git add src/voiceassistant.cpp src/modules/audio_recorder.inc
git commit -m "feat: retain optional microphone test samples"
```

### Task 4: Floating-Bar State Preview

**Files:**
- Modify: `src/modules/floating_bar.inc`
- Modify: `src/voiceassistant.cpp`

- [x] **Step 1: Add state enum**

```cpp
enum class FloatingBarStage {
    Preparing,
    Recording,
    Recognizing,
    ModelProcessing,
    Writing,
    Completed,
    Failed
};
```

- [x] **Step 2: Add state selector to test tools**

Every item uses the same right-side button text “开始测试”.

- [x] **Step 3: Simulate elapsed time and waveform**

Recording preview updates waveform every 80 ms for 10 seconds. Processing states display a timer. No API or microphone is used.

- [x] **Step 4: Verify all states and auto-close**

Set floating display time to zero and confirm the preview reports “未启用显示” rather than opening.

- [ ] **Step 5: Commit**

```bash
git add src/modules/floating_bar.inc src/voiceassistant.cpp
git commit -m "feat: preview floating bar processing states"
```

### Task 5: Hold-to-Talk Keyboard Hook

**Files:**
- Create: `src/input/hold_to_talk.h`
- Create: `src/input/hold_to_talk.cpp`
- Modify: `src/voiceassistant.cpp`
- Modify: `vocekit.pro`

- [x] **Step 1: Add key-state unit tests**

Feed synthetic modifier/key transitions into a platform-independent matcher. Assert one press and one release, ignore auto-repeat and unrelated keys.

- [x] **Step 2: Implement Windows hook**

Use `SetWindowsHookExW(WH_KEYBOARD_LL, ...)`. The callback only updates modifier state and posts a custom Qt event. It must not start recording directly.

- [x] **Step 3: Install only when required**

Install the hook when at least one enabled function uses `hold`; remove it when none do, settings change, or the app exits.

- [x] **Step 4: Add fallback**

If installation fails, log `GetLastError()`, show the numbered FAQ popup and use toggle mode for that invocation.

- [ ] **Step 5: Manual verification**

Hold configured shortcut for two seconds, release, and confirm recording stops. Type normal text and confirm no characters appear in logs.

- [ ] **Step 6: Commit**

```bash
git add src/input src/voiceassistant.cpp vocekit.pro tests/recording
git commit -m "feat: add hold-to-talk recording"
```

### Task 6: Segment Rotation and Serial Recognition

**Files:**
- Create: `src/recording/segmented_recording.cpp`
- Modify: `src/modules/audio_recorder.inc`
- Modify: `src/voiceassistant.cpp`
- Modify: `tests/recording/recording_core_tests.cpp`

- [x] **Step 1: Add failing ordering and retry tests**

Provide segment results in order 2, 1, 3 and expect merged order 1, 2, 3. Fail segment 2 once and expect exactly one retry.

- [x] **Step 2: Implement segment rotation**

At `segmentSeconds`, stop current recorder, enqueue its WAV, and immediately start a new segment recorder. Keep a total duration timer independent of segment timers.

- [x] **Step 3: Implement serial worker queue**

Only one `speechAsr()` call runs at a time. Store results by segment index.

- [x] **Step 4: Implement limits**

Stop at 30 minutes or 33 segments. Display the reason and continue processing queued segments.

- [x] **Step 5: Implement retry and merge**

Retry each failed segment once. Merge with:

```text
successful segment text
[第 2 段识别失败]
next successful segment text
```

- [x] **Step 6: Run deterministic tests**

Expected: no reordering, no more than one retry, no model call when every segment failed.

- [ ] **Step 7: Commit**

```bash
git add src/recording src/modules/audio_recorder.inc src/voiceassistant.cpp tests/recording
git commit -m "feat: add segmented long recording"
```

### Task 7: Complete WAV, History and Recovery

**Files:**
- Modify: `src/recording/segmented_recording.cpp`
- Modify: `src/modules/hub_history_page.inc`
- Modify: `src/voiceassistant.cpp`

- [x] **Step 1: Merge PCM segments**

Validate identical sample rate/channels/bit depth, concatenate PCM data and write one correct WAV header atomically.

- [x] **Step 2: Save segment metadata**

Persist `segmentCount`, `failedSegments`, `segments`, trigger mode and complete WAV path.

- [x] **Step 3: Add history detail UI**

Show each segment with play, copy recognized text and retry-failed-segment actions.

- [ ] **Step 4: Save cancelled work as draft**

Cancellation keeps completed segment text and files, marks the history item `draft=true`, and never calls the model automatically.

- [ ] **Step 5: Verify complete and partial cases**

Test three success segments, one failed segment and user cancellation.

- [ ] **Step 6: Commit**

```bash
git add src/recording src/modules/hub_history_page.inc src/voiceassistant.cpp
git commit -m "feat: add segmented recording history and recovery"
```

### Task 8: Function Editor, FAQ, Packaging and Regression

**Files:**
- Modify: `src/voiceassistant.cpp`
- Modify: `src/modules/settings_panel.inc`
- Modify: `docs/DEVELOPMENT_LOG.md`
- Modify: `docs/AI_PROJECT_GUIDE.md`
- Modify: `docs/TESTING.md`
- Modify: `scripts/package-test.ps1`

- [x] **Step 1: Add function-level controls**

For functions with voice input:

```text
录音方式：切换 / 按住说话
允许长录音：开关
每段时长：20–55 秒
最长录音：1–30 分钟
```

Hide these controls when voice input is disabled.

- [x] **Step 2: Add current-status rows**

Show recording mode and long-recording state only when they affect runtime behavior.

- [x] **Step 3: Add numbered FAQ entries**

Map every error listed in the design to popup IDs and add direct “去测试” actions.

- [ ] **Step 4: Run full regression**

Verify:

- Existing toggle Dictate/Translate/Ask.
- Hold-to-talk.
- 70-second segmented recording.
- One injected failed segment.
- Selection/write tests.
- Temporary and retained microphone tests.
- Every floating-bar state.
- History details and export.

- [ ] **Step 5: Build and analyze**

Run debug/release builds and `cppcheck`. Note the existing LLVM 22 versus MinGW 5.3 limitation for clang-tidy/clazy.

- [ ] **Step 6: Generate portable test package**

Expected: no user settings, keys, recordings or logs.

- [ ] **Step 7: Commit**

```bash
git add src docs scripts config vocekit.pro
git commit -m "feat: complete input and long recording tools"
```
