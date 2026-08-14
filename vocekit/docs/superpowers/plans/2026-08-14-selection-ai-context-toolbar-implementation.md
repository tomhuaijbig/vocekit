# Selected Text AI Context Toolbar Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 Windows 中实现选中文字后自动出现的不抢焦点工具条，并交付复制、保存、翻译、解释、普通 AI 降级搜索、流式结果卡、取消、替换、固定和继续追问。

**Architecture:** 以 Windows UI Automation 为普通读取路径，以用户显式启用的模拟复制为兼容兜底；全局观察、结构化选区、会话代际、工具条、结果卡和模型任务分别放在独立模块中。`vocekit_application_runtime.cpp` 只组装 `SelectionContextFeature`，不把新状态塞进 `VoiceController` 或录音 `FloatingBar`。

**Tech Stack:** Qt 5.9 Widgets、QtConcurrent、Windows UI Automation/Win32 hooks、C++11、qmake、QtTest、MinGW 5.3 32-bit。

---

## Scope boundary

本计划实现设计文档中的阶段一和阶段二，并完成阶段四中与这两阶段直接相关的安全、DPI 和回归门禁。

真实联网 Search Provider 不在本计划中。第一轮仍显示“AI 搜索”按钮；触发时结果卡必须明确显示“未联网，已使用普通 AI 解答”，并运行普通模型分析，不能显示来源或声称完成了联网检索。真实搜索、来源验证、Search Secret/Base URL 和网络限额使用独立实施计划。

设计依据：`docs/superpowers/specs/2026-08-14-selection-ai-context-toolbar-design.md`。

## File map

### New production files

- `src/input/selection_snapshot.h`：不可变选区快照、采集方式和敏感状态。
- `src/input/selection_observer.h/.cpp`：鼠标/键盘候选事件、Win32 hooks、去抖输入。
- `src/controllers/selection_context_policy.h/.cpp`：最小长度、黑名单、自身进程、敏感控件和重复选区决策。
- `src/controllers/selection_context_coordinator.h/.cpp`：自动/快捷键探测、generation、关闭、固定和取消状态机。
- `src/domain/selection_context_actions.h/.cpp`：固定动作 ID、标题和动作顺序规范化。
- `src/ui/selection_context_placement.h/.cpp`：工具条与结果卡的屏幕内定位。
- `src/ui/selection_context_toolbar.h/.cpp`：不激活横向工具条。
- `src/ui/selection_result_card.h/.cpp`：流式结果卡、内部滚动和动作按钮。
- `src/tasks/selection_context_model_request.h/.cpp`：翻译、解释、自定义动作和未联网搜索降级请求构造。
- `src/tasks/selection_context_model_runner.h/.cpp`：后台模型调用、delta 排队、取消和终态隔离。
- `src/controllers/selection_context_action_controller.h/.cpp`：复制、保存、AI 动作、替换和继续追问。
- `src/ui/selection_context_settings_card.h/.cpp`：自动弹出、键盘检测、最小长度、关闭、固定、动作顺序和黑名单设置。
- `src/app/selection_context_feature.h/.cpp`：组装 observer、coordinator、toolbar、card、runner 和 action controller。

### Modified production files

- `src/input/selected_text_reader.h/.cpp`：新增 `probe()`，保留 `read()` 兼容包装。
- `src/output/clipboard_writer.h/.cpp`：新增明确的持久复制 API，不模拟粘贴。
- `src/config/app_settings_data.h`：增加类型化 `SelectionContextSettings`。
- `src/config/app_settings_json.cpp`：读写选中文字工具条设置并保留旧配置。
- `src/input/hotkey_definitions.cpp`：增加工具条兜底快捷键并排除核心功能目录。
- `src/input/hotkey_settings_snapshot.cpp`：把工具条快捷键纳入全局注册快照。
- `src/ui/basic_settings_section.h/.cpp`：在常用设置中挂载专用设置卡。
- `src/ui/settings_panel.cpp`：在 `BasicSettingsSnapshot` 和 `AppSettingsData` 间传递新设置。
- `src/ui/hub_settings_state.h/.cpp`：提供整组选择工具条设置的读取、更新和持久化入口。
- `src/controllers/tray_controller.h/.cpp`：启用/暂停/恢复快捷入口。
- `src/app/vocekit_application_runtime.cpp`：创建 feature、拦截兜底快捷键、刷新设置和按顺序销毁。
- `vocekit.pro`：注册所有新增生产文件。
- `docs/AI_PROJECT_GUIDE.md`、`docs/TESTING.md`：记录使用方式、安全边界和验收矩阵。

### New focused test projects

- `tests/input/selected_text_probe_tests.{cpp,pro}`
- `tests/input/selection_observer_tests.{cpp,pro}`
- `tests/controllers/selection_context_policy_tests.{cpp,pro}`
- `tests/controllers/selection_context_coordinator_tests.{cpp,pro}`
- `tests/ui/selection_context_toolbar_tests.{cpp,pro}`
- `tests/ui/selection_result_card_tests.{cpp,pro}`
- `tests/tasks/selection_context_model_request_tests.{cpp,pro}`
- `tests/tasks/selection_context_model_runner_tests.{cpp,pro}`
- `tests/controllers/selection_context_action_controller_tests.{cpp,pro}`
- `tests/ui/selection_context_settings_card_tests.{cpp,pro}`
- `tests/app/selection_context_feature_tests.{cpp,pro}`

## Build convention used by every focused Qt test

Run from the test project directory, replacing `$target` and `$project` with the names shown in each task:

```powershell
$qt = 'D:\QQQQQT0001\5.9\mingw53_32\bin'
$mingw = 'D:\QQQQQT0001\Tools\mingw530_32\bin'
$env:PATH = "$qt;$mingw;$env:PATH"
& "$qt\qmake.exe" -o "Makefile.codex.$target" $project -spec win32-g++ CONFIG+=release `
  "OBJECTS_DIR=release/.codex/$target/obj" `
  "MOC_DIR=release/.codex/$target/moc" `
  "RCC_DIR=release/.codex/$target/rcc" `
  "UI_DIR=release/.codex/$target/ui"
& "$mingw\mingw32-make.exe" -f "Makefile.codex.$target" -j2
& ".\release\$target.exe" -maxwarnings 0
```

Expected GREEN output for every QtTest executable: exit code `0`, `0 failed`, `0 skipped`.

---

### Task 1: Persist settings, action catalog, and fallback hotkey

**Files:**
- Create: `src/domain/selection_context_actions.h`
- Create: `src/domain/selection_context_actions.cpp`
- Modify: `src/config/app_settings_data.h`
- Modify: `src/config/app_settings_json.cpp`
- Modify: `src/input/hotkey_definitions.cpp`
- Modify: `src/input/hotkey_settings_snapshot.cpp`
- Modify: `tests/config/app_settings_defaults_tests.cpp`
- Modify: `tests/config/app_settings_json_tests.cpp`
- Modify: `tests/input/hotkey_definitions_tests.cpp`
- Modify: `tests/input/hotkey_settings_snapshot_tests.cpp`
- Modify: the corresponding four existing `.pro` files to link `selection_context_actions.cpp` where required.

- [ ] **Step 1: Write RED settings and hotkey tests**

Add assertions equivalent to:

```cpp
void defaultsKeepAutomaticSelectionToolbarOptIn()
{
    const AppSettingsData data;
    QVERIFY(!data.selectionContext.enabled);
    QVERIFY(data.selectionContext.keyboardSelectionEnabled);
    QCOMPARE(data.selectionContext.minimumTextLength, 2);
    QVERIFY(data.selectionContext.closeOnOutsideClick);
    QVERIFY(data.selectionContext.pinEnabled);
    QVERIFY(!data.selectionContext.networkConsentAcknowledged);
    QCOMPARE(data.selectionContext.pauseMinutes, 30);
    QCOMPARE(
        data.selectionContext.actionOrder,
        QStringList()
            << selectionContextActionAiSearch()
            << selectionContextActionTranslate()
            << selectionContextActionExplain()
            << selectionContextActionSave()
            << selectionContextActionCopy()
    );
}

void selectionContextSettingsRoundTripAndNormalize()
{
    AppSettingsData data;
    data.selectionContext.enabled = true;
    data.selectionContext.networkConsentAcknowledged = true;
    data.selectionContext.minimumTextLength = 0;
    data.selectionContext.blockedApplications =
        QStringList() << QStringLiteral(" WeChat.exe ")
                      << QStringLiteral("wechat.exe");
    data.selectionContext.actionOrder =
        QStringList() << QStringLiteral("unknown")
                      << selectionContextActionCopy();

    const AppSettingsData restored = appSettingsDataFromJson(
        appSettingsDataToJson(data)
    );
    QVERIFY(restored.selectionContext.enabled);
    QVERIFY(restored.selectionContext.networkConsentAcknowledged);
    QCOMPARE(restored.selectionContext.minimumTextLength, 1);
    QCOMPARE(restored.selectionContext.blockedApplications,
             QStringList() << QStringLiteral("wechat.exe"));
    QCOMPARE(restored.selectionContext.actionOrder.first(),
             selectionContextActionCopy());
    QCOMPARE(restored.selectionContext.actionOrder.size(), 5);
}

void exposesSelectionToolbarFallbackWithoutMakingItAFunction()
{
    const QString id = QStringLiteral("selection_toolbar");
    QVERIFY(idsOf(hotkeyDefs()).contains(id));
    QVERIFY(!idsOf(coreFunctionDefs()).contains(id));
    const GlobalHotkeySettingsSnapshot snapshot =
        globalHotkeySnapshotFromData(AppSettingsData());
    QCOMPARE(countFunction(snapshot, id), 1);
}
```

- [ ] **Step 2: Run the four focused projects and record RED**

Run `app_settings_defaults_tests`, `app_settings_json_tests`, `hotkey_definitions_tests`, and `hotkey_settings_snapshot_tests` using the build convention. Expected RED: missing `selectionContext`, missing action functions, or missing `selection_toolbar` hotkey.

- [ ] **Step 3: Add the typed settings and action normalization**

Use these public types and IDs:

```cpp
// src/config/app_settings_data.h
struct SelectionContextSettings
{
    bool enabled = false;
    bool keyboardSelectionEnabled = true;
    bool closeOnOutsideClick = true;
    bool pinEnabled = true;
    bool networkConsentAcknowledged = false;
    int minimumTextLength = 2;
    int pauseMinutes = 30;
    QStringList blockedApplications;
    QStringList actionOrder;
};

struct AppSettingsData
{
    // existing fields remain unchanged
    SelectionContextSettings selectionContext;
};
```

```cpp
// src/domain/selection_context_actions.h
QString selectionContextActionAiSearch();
QString selectionContextActionTranslate();
QString selectionContextActionExplain();
QString selectionContextActionSave();
QString selectionContextActionCopy();
QString selectionContextActionForFunction(const QString &functionId);
QString selectionContextFunctionId(const QString &actionId);
bool isSelectionContextFunctionAction(const QString &actionId);
QString selectionContextMenuPauseApplication();
QString selectionContextMenuOpenSettings();
QStringList defaultSelectionContextActionOrder();
QStringList normalizeSelectionContextActionOrder(const QStringList &values);
QString selectionContextActionTitle(const QString &id);
```

Normalization must keep the first occurrence of every known primary ID and append any missing default IDs. Dynamic `function:<id>` actions live in the More menu and are not persisted in the primary row order. Add round-trip tests for `selectionContextActionForFunction`/`selectionContextFunctionId`, including empty and malformed IDs. Normalize blocked executable names with `QFileInfo(value.trimmed()).fileName().toLower()` and remove duplicates.

- [ ] **Step 4: Read and write one nested JSON object**

Use a root object named `selectionContextToolbar`:

```cpp
const QJsonObject selection =
    root.value(QStringLiteral("selectionContextToolbar")).toObject();
data.selectionContext.enabled =
    selection.value(QStringLiteral("enabled")).toBool(false);
data.selectionContext.keyboardSelectionEnabled =
    selection.value(QStringLiteral("keyboardSelectionEnabled")).toBool(true);
data.selectionContext.minimumTextLength = qBound(
    1,
    selection.value(QStringLiteral("minimumTextLength")).toInt(2),
    1000
);
data.selectionContext.closeOnOutsideClick =
    selection.value(QStringLiteral("closeOnOutsideClick")).toBool(true);
data.selectionContext.pinEnabled =
    selection.value(QStringLiteral("pinEnabled")).toBool(true);
data.selectionContext.networkConsentAcknowledged =
    selection.value(QStringLiteral("networkConsentAcknowledged")).toBool(false);
data.selectionContext.pauseMinutes = qBound(
    1,
    selection.value(QStringLiteral("pauseMinutes")).toInt(30),
    1440
);
data.selectionContext.actionOrder = normalizeSelectionContextActionOrder(
    stringListFromJson(selection.value(QStringLiteral("actionOrder")).toArray())
);
data.selectionContext.blockedApplications = normalizeExecutableList(
    stringListFromJson(selection.value(QStringLiteral("blockedApplications")).toArray())
);
```

When writing, insert every normalized field into the same nested object. Remove `selectionContextToolbar` from `retainedRootValues` before overlaying known values so legacy unknown root fields remain preserved without overwriting the typed object.

- [ ] **Step 5: Register the fallback hotkey**

Add this `HotkeyDef`:

```cpp
{
    QStringLiteral("selection_toolbar"),
    text8("选中文字工具条"),
    QStringLiteral("Ctrl+Alt+E"),
    text8("读取当前选中文字并显示快捷工具条")
}
```

Update `coreFunctionDefs()` to exclude `selection_toolbar`, `hub`, and `vocabulary_add`. `globalHotkeySnapshotFromData()` may keep treating it as a no-voice `GlobalHotkeyFunction`; runtime dispatch is added in Task 13.

- [ ] **Step 6: Re-run the four focused projects**

Expected: all four executables return `0`, with no skipped tests.

- [ ] **Step 7: Commit Task 1**

```powershell
git add src/domain/selection_context_actions.* src/config/app_settings_data.h `
  src/config/app_settings_json.cpp src/input/hotkey_definitions.cpp `
  src/input/hotkey_settings_snapshot.cpp `
  tests/config/app_settings_defaults_tests.cpp `
  tests/config/app_settings_defaults_tests.pro `
  tests/config/app_settings_json_tests.cpp `
  tests/config/app_settings_json_tests.pro `
  tests/input/hotkey_definitions_tests.cpp `
  tests/input/hotkey_definitions_tests.pro `
  tests/input/hotkey_settings_snapshot_tests.cpp `
  tests/input/hotkey_settings_snapshot_tests.pro
git commit -m "feat: define selection context settings"
```

---

### Task 2: Return a structured and privacy-aware selection snapshot

**Files:**
- Create: `src/input/selection_snapshot.h`
- Modify: `src/input/selected_text_reader.h`
- Modify: `src/input/selected_text_reader.cpp`
- Create: `tests/input/selected_text_probe_tests.cpp`
- Create: `tests/input/selected_text_probe_tests.pro`

- [ ] **Step 1: Write the RED contract tests**

The test project must expose injectable normalization helpers rather than requiring an external application. Add tests for multiple ranges, malformed SAFEARRAY-like values, password rejection, cursor fallback, clipboard method, and compatibility `read()`:

```cpp
void choosesLastValidRectangleNearestCursor()
{
    SelectionSnapshot snapshot;
    snapshot.cursorPosition = QPoint(430, 240);
    snapshot.rectangles = QVector<QRect>()
        << QRect(100, 100, 240, 20)
        << QRect(100, 122, 330, 20);
    snapshot.anchorRect = selectionAnchorRectangle(
        snapshot.rectangles,
        snapshot.cursorPosition
    );
    QCOMPARE(snapshot.anchorRect, QRect(100, 122, 330, 20));
}

void rejectsSensitiveSelectionEvenWhenTextExists()
{
    SelectionSnapshot snapshot;
    snapshot.text = QStringLiteral("secret-value");
    snapshot.sensitivity = SelectionSensitivity::Password;
    QVERIFY(!snapshot.isUsable());
}
```

- [ ] **Step 2: Build and verify RED**

Target: `selected_text_probe_tests`; project: `selected_text_probe_tests.pro`. Expected RED: `selection_snapshot.h` and `SelectedTextReader::probe` are missing.

- [ ] **Step 3: Add the immutable snapshot contract**

```cpp
enum class SelectionAcquisitionMethod
{
    None,
    UiAutomation,
    ClipboardFallback
};

enum class SelectionSensitivity
{
    Normal,
    Password,
    Protected,
    PermissionDenied,
    SecureDesktop
};

struct SelectionSnapshot
{
    quint64 generation = 0;
    QString text;
    QVector<QRect> rectangles;
    QRect anchorRect;
    QPoint cursorPosition;
    SelectedTextNativeWindowHandle targetWindow = nullptr;
    quint32 targetProcessId = 0;
    QString targetExecutable;
    SelectionAcquisitionMethod method = SelectionAcquisitionMethod::None;
    SelectionSensitivity sensitivity = SelectionSensitivity::Normal;

    bool isUsable() const
    {
        return sensitivity == SelectionSensitivity::Normal
            && !text.trimmed().isEmpty();
    }
};

struct SelectionProbeRequest
{
    bool strongSelectionEnabled = false;
    SelectedTextNativeWindowHandle targetWindow = nullptr;
    QPoint cursorPosition;
};

QRect selectionAnchorRectangle(
    const QVector<QRect> &rectangles,
    const QPoint &cursorPosition
);
QVector<QRect> selectionRectanglesFromFlatBounds(
    const QVector<double> &values
);
bool selectionInputDesktopIsSecure(
    bool inputDesktopOpened,
    const QString &desktopName
);
```

Add `static SelectionSnapshot probe(const SelectionProbeRequest &request);` to `SelectedTextReader`. Preserve current callers by implementing `read()` as `probe(...).text`.

- [ ] **Step 4: Extend UI Automation without duplicating the reader**

Inside the existing TextPattern range loop:

```cpp
SAFEARRAY *bounds = nullptr;
if (SUCCEEDED(range->GetBoundingRectangles(&bounds)) && bounds) {
    double *values = nullptr;
    if (SUCCEEDED(SafeArrayAccessData(
            bounds,
            reinterpret_cast<void **>(&values)))) {
        LONG lower = 0;
        LONG upper = -1;
        SafeArrayGetLBound(bounds, 1, &lower);
        SafeArrayGetUBound(bounds, 1, &upper);
        const LONG count = upper >= lower ? upper - lower + 1 : 0;
        for (LONG offset = 0; offset + 3 < count; offset += 4) {
            const QRect rect(
                qRound(values[offset]),
                qRound(values[offset + 1]),
                qRound(values[offset + 2]),
                qRound(values[offset + 3])
            );
            if (rect.width() > 0 && rect.height() > 0) {
                snapshot.rectangles.append(rect.normalized());
            }
        }
        SafeArrayUnaccessData(bounds);
    }
    SafeArrayDestroy(bounds);
}
```

Read `UIA_IsPasswordPropertyId` (`30019`) before TextPattern access. A `VT_BOOL/VARIANT_TRUE` value sets `Password`, clears text and rectangles, and skips clipboard fallback. Before UIA access, inspect the active input desktop with `OpenInputDesktop` and `GetUserObjectInformation(UOI_NAME)`; failure to open it or a non-default secure/lock desktop sets `SecureDesktop` and returns without UIA or clipboard access. Production must call the two pure helpers above so malformed bounds and desktop-name decisions are testable without opening another application's UI. Query foreground HWND process ID and executable basename with `GetWindowThreadProcessId` and `QueryFullProcessImageNameW`. Failure caused by access denial sets `PermissionDenied`; ordinary missing TextPattern stays `Normal` with empty text.

- [ ] **Step 5: Preserve strong-selection safety**

Only call the existing clipboard-copy path when all conditions are true:

```cpp
if (snapshot.sensitivity == SelectionSensitivity::Normal
    && snapshot.text.trimmed().isEmpty()
    && request.strongSelectionEnabled
    && request.targetWindow) {
    snapshot.text = selectedTextViaClipboardCopy(
        request.targetWindow
    ).trimmed();
    if (!snapshot.text.isEmpty()) {
        snapshot.method =
            SelectionAcquisitionMethod::ClipboardFallback;
    }
}
```

Never use clipboard fallback for password/protected/permission-denied/secure-desktop snapshots.

- [ ] **Step 6: Run the focused tests and existing selection tests**

Run `selected_text_probe_tests`, `selected_text_workflow_controller_tests`, and `selected_text_diagnostic_task_tests`. Expected: all GREEN.

- [ ] **Step 7: Commit Task 2**

```powershell
git add src/input/selection_snapshot.h src/input/selected_text_reader.* `
  tests/input/selected_text_probe_tests.*
git commit -m "feat: expose structured selected text snapshots"
```

---

### Task 3: Observe mouse and keyboard selection candidates safely

**Files:**
- Create: `src/input/selection_observer.h`
- Create: `src/input/selection_observer.cpp`
- Create: `tests/input/selection_observer_tests.cpp`
- Create: `tests/input/selection_observer_tests.pro`

- [ ] **Step 1: Write RED tests for the pure matcher**

```cpp
void mouseClickWithoutDragDoesNotProbe()
{
    SelectionObservationMatcher matcher;
    matcher.mousePressed(QPoint(40, 40));
    QVERIFY(!matcher.mouseReleased(QPoint(42, 42)));
}

void mouseDragAndKeyboardSelectionEachEmitOnce()
{
    SelectionObservationMatcher matcher;
    matcher.mousePressed(QPoint(10, 10));
    QVERIFY(matcher.mouseReleased(QPoint(60, 10)));
    QVERIFY(matcher.keyPressed(VK_SHIFT));
    QVERIFY(matcher.keyReleased(VK_RIGHT));
    QVERIFY(!matcher.keyReleased(VK_RIGHT));
}

void shortcutCandidateIsIndependentOfAutomaticPause()
{
    SelectionObservationMatcher matcher;
    matcher.setPaused(true);
    QVERIFY(!matcher.mouseReleased(QPoint(90, 90)));
    QVERIFY(matcher.fallbackShortcutReleased());
}

void plainPointerReleaseEmitsOutsideEventButNotSelectionProbe();
void foregroundChangeCarriesTheNewNativeWindowOnce();
```

- [ ] **Step 2: Build and verify RED**

Target: `selection_observer_tests`; expected RED: missing observer and matcher.

- [ ] **Step 3: Implement the public observer API**

```cpp
enum class SelectionObservationReason
{
    MouseSelection,
    KeyboardSelection,
    FallbackShortcut,
    OutsidePointerRelease,
    ForegroundChanged,
    EscapePressed
};

struct SelectionObservation
{
    SelectionObservationReason reason =
        SelectionObservationReason::MouseSelection;
    QPoint cursorPosition;
    SelectedTextNativeWindowHandle targetWindow = nullptr;
};

class SelectionObserver
{
public:
    ~SelectionObserver();
    bool install(QString *error = nullptr);
    void uninstall();
    void setAutomaticEnabled(bool enabled);
    void setKeyboardSelectionEnabled(bool enabled);
    void setPaused(bool paused);
    void setCallback(
        const std::function<void(const SelectionObservation &)> &callback
    );
    void processNativeMouse(unsigned int message, const QPoint &point);
    void processNativeKey(unsigned int message, unsigned int nativeKey);
    void processForegroundWindowChanged(
        SelectedTextNativeWindowHandle window
    );
};
```

The Win32 callback filters only left-button down/up and Shift-selection keys (`VK_LEFT`, `VK_RIGHT`, `VK_UP`, `VK_DOWN`, `VK_HOME`, `VK_END`, `VK_PRIOR`, `VK_NEXT`) plus Ctrl+A completion and Esc close. Every left-button release emits one `OutsidePointerRelease` carrying `WindowFromPoint`; a qualifying drag additionally emits one `MouseSelection`. Esc emits `EscapePressed` only while a selection surface is visible. It must not retain ordinary typed keys. Queue callbacks through `QTimer::singleShot(0, QCoreApplication::instance(), ...)`; do not run COM, clipboard, UI, or network work in a hook callback.

- [ ] **Step 4: Install and uninstall both hooks as one unit**

Use `WH_MOUSE_LL`, `WH_KEYBOARD_LL`, and `SetWinEventHook(EVENT_SYSTEM_FOREGROUND, ...)`. If any installation fails, unhook every earlier handle before returning failure. `uninstall()` must be idempotent, release all three handles, clear callbacks and reset matcher state. Keep the existing `HoldToTalkHook` untouched; Windows supports the additional keyboard hook chain.

- [ ] **Step 5: Run repeated lifecycle tests**

Add a test loop that calls `install()`/`uninstall()` 50 times under Windows, then asserts no callback after uninstall when invoking `processNativeMouse()` directly. Run `selection_observer_tests` three consecutive times. Expected: each run GREEN with no residual test process.

- [ ] **Step 6: Commit Task 3**

```powershell
git add src/input/selection_observer.* tests/input/selection_observer_tests.*
git commit -m "feat: observe selection completion candidates"
```

---

### Task 4: Enforce eligibility, deduplication, and privacy policy

**Files:**
- Create: `src/controllers/selection_context_policy.h`
- Create: `src/controllers/selection_context_policy.cpp`
- Create: `tests/controllers/selection_context_policy_tests.cpp`
- Create: `tests/controllers/selection_context_policy_tests.pro`

- [ ] **Step 1: Write RED policy tests**

Cover empty text, whitespace, min length, own PID, password/protected input, permission denial, secure desktop, blocked executable, identical snapshot deduplication, changed rectangle, and changed target window:

```cpp
void passwordAndOwnProcessAreNeverEligible()
{
    SelectionContextPolicyInput input;
    input.minimumTextLength = 2;
    input.currentProcessId = 42;
    input.snapshot.text = QStringLiteral("private");
    input.snapshot.targetProcessId = 42;
    QCOMPARE(selectionContextEligibility(input),
             SelectionContextEligibility::OwnProcess);
    input.snapshot.targetProcessId = 7;
    input.snapshot.sensitivity = SelectionSensitivity::Password;
    QCOMPARE(selectionContextEligibility(input),
             SelectionContextEligibility::Sensitive);
}

void duplicateRequiresSameWindowTextAndNearAnchor()
{
    SelectionSnapshot first = usableSnapshot(
        QStringLiteral("hello"), reinterpret_cast<void *>(1),
        QRect(100, 100, 80, 20));
    SelectionSnapshot near = first;
    near.anchorRect.translate(2, 2);
    QVERIFY(selectionSnapshotsEquivalent(first, near, 4));
    near.targetWindow = reinterpret_cast<void *>(2);
    QVERIFY(!selectionSnapshotsEquivalent(first, near, 4));
}
```

- [ ] **Step 2: Build and verify RED**

Target: `selection_context_policy_tests`; expected RED: policy files missing.

- [ ] **Step 3: Implement a closed eligibility enum**

```cpp
enum class SelectionContextEligibility
{
    Eligible,
    Empty,
    TooShort,
    Sensitive,
    PermissionDenied,
    SecureDesktop,
    OwnProcess,
    BlockedApplication
};

struct SelectionContextPolicyInput
{
    SelectionSnapshot snapshot;
    int minimumTextLength = 2;
    quint32 currentProcessId = 0;
    QStringList blockedApplications;
};

SelectionContextEligibility selectionContextEligibility(
    const SelectionContextPolicyInput &input
);
bool selectionSnapshotsEquivalent(
    const SelectionSnapshot &left,
    const SelectionSnapshot &right,
    int anchorTolerancePixels = 4
);
```

Executable comparison is case-insensitive and basename-only. Eligibility must never log or return the selected text in a diagnostic string.

- [ ] **Step 4: Run focused tests**

Expected: every enum path is asserted and GREEN.

- [ ] **Step 5: Commit Task 4**

```powershell
git add src/controllers/selection_context_policy.* `
  tests/controllers/selection_context_policy_tests.*
git commit -m "feat: gate selection context eligibility"
```

---

### Task 5: Place toolbars and result cards on the correct screen

**Files:**
- Create: `src/ui/selection_context_placement.h`
- Create: `src/ui/selection_context_placement.cpp`
- Create: `tests/ui/selection_context_toolbar_tests.cpp`
- Create: `tests/ui/selection_context_toolbar_tests.pro`

- [ ] **Step 1: Write RED data-driven placement tests**

Rows must cover below, above, cursor fallback, right edge, left edge, taskbar bottom, negative-coordinate monitor and 200% logical geometry:

```cpp
void placementStaysInsideAvailableGeometry_data()
{
    QTest::addColumn<QRect>("screen");
    QTest::addColumn<QRect>("anchor");
    QTest::addColumn<QPoint>("cursor");
    QTest::addColumn<QSize>("toolbar");
    QTest::newRow("negative-monitor")
        << QRect(-1920, 0, 1920, 1040)
        << QRect(-80, 980, 70, 20)
        << QPoint(-20, 990)
        << QSize(560, 48);
}

void placementStaysInsideAvailableGeometry()
{
    QFETCH(QRect, screen);
    QFETCH(QRect, anchor);
    QFETCH(QPoint, cursor);
    QFETCH(QSize, toolbar);
    const SelectionSurfacePlacement placed =
        placeSelectionSurfaces(
            anchor, cursor, toolbar, QSize(560, 320), screen, 8
        );
    QVERIFY(screen.contains(QRect(placed.toolbarTopLeft, toolbar)));
}
```

- [ ] **Step 2: Build and verify RED**

Target: `selection_context_toolbar_tests`; expected RED: placement API missing.

- [ ] **Step 3: Implement deterministic placement**

```cpp
struct SelectionSurfacePlacement
{
    QPoint toolbarTopLeft;
    QPoint cardTopLeft;
    bool toolbarAbove = false;
    bool cardAbove = false;
};

SelectionSurfacePlacement placeSelectionSurfaces(
    const QRect &anchorRect,
    const QPoint &cursorPosition,
    const QSize &toolbarSize,
    const QSize &cardSize,
    const QRect &availableGeometry,
    int gap
);
```

Algorithm order is fixed: anchor below, anchor above, cursor below, cursor above; every candidate is horizontally clamped and then vertically clamped. The card is centered under the toolbar unless that would overflow, then placed above both. Do not use the union rectangle of a multi-line selection.

- [ ] **Step 4: Run placement tests**

Expected: every returned toolbar rectangle is contained in the provided screen; card containment is asserted when `cardSize` fits the screen.

- [ ] **Step 5: Commit Task 5**

```powershell
git add src/ui/selection_context_placement.* `
  tests/ui/selection_context_toolbar_tests.*
git commit -m "feat: position selection context surfaces"
```

---

### Task 6: Build the non-activating selection toolbar

**Files:**
- Create: `src/ui/selection_context_toolbar.h`
- Create: `src/ui/selection_context_toolbar.cpp`
- Modify: `tests/ui/selection_context_toolbar_tests.cpp`
- Modify: `tests/ui/selection_context_toolbar_tests.pro`

- [ ] **Step 1: Add RED widget tests for ownership, order, and focus behavior**

Construct the real widget and assert these object names and behaviors:

```cpp
void toolbarUsesCatalogOrderWithoutTakingFocus()
{
    SelectionContextToolbar toolbar;
    toolbar.setActionOrder(
        QStringList() << selectionContextActionCopy()
                      << selectionContextActionTranslate()
                      << selectionContextActionAiSearch()
    );

    QCOMPARE(toolbar.objectName(), QStringLiteral("selectionContextToolbar"));
    QVERIFY(toolbar.testAttribute(Qt::WA_ShowWithoutActivating));
    QVERIFY(toolbar.windowFlags() & Qt::Tool);
    QVERIFY(toolbar.windowFlags() & Qt::FramelessWindowHint);
    QVERIFY(toolbar.windowFlags() & Qt::WindowDoesNotAcceptFocus);
    QVERIFY(toolbar.findChild<QWidget *>(
        QStringLiteral("selectionContextIdentity")));
    QVERIFY(toolbar.findChild<QToolButton *>(
        QStringLiteral("selectionContextMoreButton")));
    QCOMPARE(actionIds(toolbar), QStringList()
        << selectionContextActionCopy()
        << selectionContextActionTranslate()
        << selectionContextActionAiSearch());
}

void everyClickEmitsExactlyOneStableActionId();
void busyStateDisablesOtherActionsAndKeepsCloseAvailable();
void dragHandleMovesTheToolbarButDraggingAButtonDoesNot();
void fallbackShortcutModeCanReceiveKeyboardNavigationAndEscape();
void moreMenuEmitsCustomPauseAndSettingsIdsExactlyOnce();
void callbackMayDeleteToolbarSynchronously();
```

Also assert that the five default action buttons are each present exactly once, every button height is at least `max(40, fontMetrics().height() + 16)`, and no action button has a fixed height.

- [ ] **Step 2: Build `selection_context_toolbar_tests` and verify RED**

Use the focused build convention with `$target = 'selection_context_toolbar_tests'` and `$project = 'selection_context_toolbar_tests.pro'`. Expected RED: `SelectionContextToolbar` is missing.

- [ ] **Step 3: Implement the real toolbar**

Use this public contract:

```cpp
struct SelectionContextToolbarCallbacks
{
    std::function<void(const QString &actionId)> actionRequested;
    std::function<void()> closeRequested;
};

struct SelectionContextMenuItem
{
    QString actionId;
    QString title;
    bool enabled = true;
};

class SelectionContextToolbar : public QWidget
{
    Q_OBJECT
public:
    explicit SelectionContextToolbar(QWidget *parent = nullptr);
    void setCallbacks(const SelectionContextToolbarCallbacks &callbacks);
    void setActionOrder(const QStringList &actionIds);
    void setMoreActions(const QVector<SelectionContextMenuItem> &items);
    void setBusyAction(const QString &actionId);
    void setActionEnabled(const QString &actionId, bool enabled);
    void showForSnapshot(const SelectionSnapshot &snapshot,
                         const QRect &availableGeometry,
                         bool keyboardNavigationMode = false);
    void hideToolbar();
    bool ownsNativeWindow(SelectedTextNativeWindowHandle window) const;
};
```

Normal automatic mode uses `Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus` and `WA_ShowWithoutActivating`. Explicit fallback-shortcut mode may temporarily omit `WindowDoesNotAcceptFocus` and focus the first action for keyboard navigation; closing restores the non-activating default. Apply the drag event filter only to `selectionContextDragHandle`; never install it on an action button. Use `QPointer<SelectionContextToolbar>` immediately before invoking a callback and return if the callback destroys the widget.

Always render `selectionContextDragHandle`, `selectionContextIdentity`, the ordered five primary actions, and `selectionContextMoreButton`; More is not part of the reorderable primary list. Required button object names are `selectionActionAiSearchButton`, `selectionActionTranslateButton`, `selectionActionExplainButton`, `selectionActionSaveButton`, `selectionActionCopyButton`, and `selectionToolbarCloseButton`. The More menu receives dynamic `function:<id>` items plus pause-current-application and open-settings IDs. The first-round AI Search button remains enabled; its honest offline degradation is handled by Task 9.

- [ ] **Step 4: Run the complete toolbar project**

Expected: placement tests plus real widget tests are GREEN with no focus activation and no duplicate callback.

- [ ] **Step 5: Commit Task 6**

```powershell
git add src/ui/selection_context_toolbar.* tests/ui/selection_context_toolbar_tests.*
git commit -m "feat: add selection context toolbar"
```

---

### Task 7: Build the streaming result card

**Files:**
- Create: `src/ui/selection_result_card.h`
- Create: `src/ui/selection_result_card.cpp`
- Create: `tests/ui/selection_result_card_tests.cpp`
- Create: `tests/ui/selection_result_card_tests.pro`

- [ ] **Step 1: Write RED state and lifecycle tests**

```cpp
void rendersCommittedAndProvisionalTextInBoundedScrollArea();
void runningStateShowsCancelAndHidesReplace();
void completedStateEnablesCopyAndConditionallyEnablesReplace();
void completedStateRegeneratesOnlyAfterExplicitClick();
void pinKeepsCardVisibleAcrossSelectionChanges();
void followUpPinsAndActivatesTheInputOnlyAfterExplicitClick();
void outsideCloseDoesNotClosePinnedCard();
void terminalAndActionCallbacksAreExactlyOnce();
void callbackMayDeleteCardSynchronously();
void longTextConfirmationOffersProcessFullTextOrCancelWithoutTruncation();
void rapidDeltasCoalesceVisualLayoutWithoutDroppingCommittedText();
void longChineseTextIsNotClippedAtTwoHundredPercentFont();
```

The long-text test must instantiate the real card, set at least 400 Chinese characters, and assert that its text widget has a vertical scroll bar rather than expanding beyond the supplied available geometry.

- [ ] **Step 2: Build and verify RED**

Run `selection_result_card_tests.pro` as target `selection_result_card_tests`. Expected RED: result-card types are missing.

- [ ] **Step 3: Implement the card with one explicit state object**

```cpp
struct SelectionResultCardState
{
    QString actionId;
    QString title;
    QString committedText;
    QString provisionalText;
    QString statusText;
    bool running = false;
    bool pinned = false;
    bool replaceEnabled = false;
    bool requiresLongTextConfirmation = false;
};

struct SelectionResultCardCallbacks
{
    std::function<void()> cancelRequested;
    std::function<void()> copyRequested;
    std::function<void()> replaceRequested;
    std::function<void()> regenerateRequested;
    std::function<void(bool)> pinChanged;
    std::function<void()> closeRequested;
    std::function<void(const QString &question)> followUpRequested;
    std::function<void()> processFullTextRequested;
};
```

The card starts with the same non-activating window flags as the toolbar. Only an explicit click in the follow-up input may activate it. Put result text in a scrollable `QTextBrowser` or `QPlainTextEdit`; do not use a fixed result-label height. Required object names: `selectionResultCard`, `selectionResultCommittedText`, `selectionResultProvisionalText`, `selectionResultStatus`, `selectionResultCancelButton`, `selectionResultRegenerateButton`, `selectionResultCopyButton`, `selectionResultReplaceButton`, `selectionResultPinButton`, `selectionResultCloseButton`, `selectionResultFollowUpInput`, and `selectionResultFollowUpButton`.

Use a 33-50 ms single-shot render coalescer for streaming text so bursts do not relayout the window per token; the underlying committed/provisional strings must retain every accepted delta. For over-limit input, show an in-card confirmation with “处理全文” and “取消”; never silently truncate. Clear terminal callbacks before invoking them, and protect every synchronous callback boundary with `QPointer`.

- [ ] **Step 4: Run the result-card tests**

Expected: all state transitions, delete-in-callback cases, and 200% long-text checks are GREEN.

- [ ] **Step 5: Commit Task 7**

```powershell
git add src/ui/selection_result_card.* tests/ui/selection_result_card_tests.*
git commit -m "feat: add selection result card"
```

---

### Task 8: Coordinate observation, generation, pause, and close rules

**Files:**
- Create: `src/controllers/selection_context_coordinator.h`
- Create: `src/controllers/selection_context_coordinator.cpp`
- Create: `tests/controllers/selection_context_coordinator_tests.cpp`
- Create: `tests/controllers/selection_context_coordinator_tests.pro`

- [ ] **Step 1: Write RED tests against injected boundaries**

Cover these cases with fake probe/show/hide/cancel functions and no real hooks:

```cpp
void mouseReleaseDebouncesForOneHundredSixtyMilliseconds();
void keyboardObservationHonorsItsSeparateSetting();
void fallbackShortcutStillWorksWhileAutomaticObservationIsPaused();
void equivalentSnapshotDoesNotReopenTheToolbar();
void aNewSnapshotCancelsOnlyAnUnpinnedActiveRequest();
void pinnedResultDetachesFromLaterSelectionChanges();
void outsideClickClosesUnpinnedSurfacesButNotOwnedWindows();
void escapeAndForegroundChangeCloseOnlyUnpinnedSurfaces();
void destroyedTargetWindowClosesAndCancelsTheUnpinnedSession();
void sensitiveOwnProcessAndBlockedSnapshotsNeverShowUi();
void automaticPermissionFailureIsSilentButManualShortcutExplainsItOnce();
void secureDesktopNeverShowsUiOrUsesClipboardFallback();
void staleProbeAndStaleTerminalCallbacksAreIgnoredByGeneration();
void stoppingUninstallsObservationAndCancelsExactlyOnce();
void callbackMayDestroyCoordinatorSynchronously();
```

- [ ] **Step 2: Build and verify RED**

Run target `selection_context_coordinator_tests`. Expected RED: coordinator API missing.

- [ ] **Step 3: Implement a generation-based coordinator**

```cpp
struct SelectionContextCoordinatorAccess
{
    std::function<SelectionContextSettings()> settingsSnapshot;
    std::function<SelectionSnapshot(const QPoint &, bool)> probe;
    std::function<quint32()> currentProcessId;
    std::function<void(const SelectionSnapshot &,
                       bool keyboardNavigationMode)> showToolbar;
    std::function<void()> hideToolbar;
    std::function<void()> closeUnpinnedResult;
    std::function<void()> cancelActiveAction;
    std::function<bool(SelectedTextNativeWindowHandle)> ownsSurfaceWindow;
    std::function<void(SelectionContextEligibility)> showManualFailure;
    std::function<void(const QString &eventId, int textLength)> logMetadata;
};

class SelectionContextCoordinator : public QObject
{
    Q_OBJECT
public:
    explicit SelectionContextCoordinator(
        const SelectionContextCoordinatorAccess &access,
        QObject *parent = nullptr
    );
    void start();
    void stop();
    void refreshSettings();
    void triggerFallbackShortcut();
    void pauseForMinutes(int minutes);
    void resume();
    bool isPaused() const;
    void setResultPinned(bool pinned);
};
```

Use a single-shot 160 ms timer after a candidate mouse or keyboard event. Increment `m_generation` before every probe, new action cancellation, and stop. Automatic observation respects `enabled` and pause; manual shortcut bypasses pause but still enforces sensitivity, own-process and block-list policy, and calls `showToolbar(snapshot, true)` so keyboard navigation is available. Automatic probes call `showToolbar(snapshot, false)`. Automatic permission/secure-desktop failures are silent; a manual permission failure invokes `showManualFailure(PermissionDenied)` at most once per trigger and never reveals selected text. Esc, foreground change, or an invalid target window closes and cancels an unpinned session; pinned results remain. Logs contain only event ID and text length.

- [ ] **Step 4: Run coordinator tests three consecutive times**

```powershell
1..3 | ForEach-Object { .\release\selection_context_coordinator_tests.exe -maxwarnings 0 }
```

Expected: every run exits `0`; no timing flakes and no residual hook process.

- [ ] **Step 5: Commit Task 8**

```powershell
git add src/controllers/selection_context_coordinator.* `
  tests/controllers/selection_context_coordinator_tests.*
git commit -m "feat: coordinate selection context lifecycle"
```

---

### Task 9: Build honest model requests for translate, explain, custom actions, and offline AI Search

**Files:**
- Create: `src/tasks/selection_context_model_request.h`
- Create: `src/tasks/selection_context_model_request.cpp`
- Create: `tests/tasks/selection_context_model_request_tests.cpp`
- Create: `tests/tasks/selection_context_model_request_tests.pro`

- [ ] **Step 1: Write RED request-contract tests**

```cpp
void translateUsesTranslatePromptModelAndConfiguredTargetLanguage();
void explainUsesAskPromptAndPreservesTheOriginalSelection();
void customFunctionUsesTheNamedFunctionRuntimeWithoutChangingSettings();
void aiSearchExplicitlyDegradesToOrdinaryAiWithoutSources();
void followUpIncludesOriginalSelectionPreviousAnswerAndQuestion();
void unknownActionReturnsAStableLocalErrorWithoutCallingAProvider();
void selectedTextNeverAppearsInTheRequestDiagnosticSummary();
```

The AI Search assertion is exact:

```cpp
const SelectionContextModelRequest built = buildSelectionContextModelRequest(input);
QVERIFY(built.valid);
QVERIFY(built.degraded);
QCOMPARE(built.degradedMessage,
         QStringLiteral("未联网，已使用普通 AI 解答"));
const QString completePrompt = built.modelRequest.systemPrompt
    + QLatin1Char('\n') + built.modelRequest.userPrompt;
QVERIFY(!completePrompt.contains(QStringLiteral("来源：")));
QVERIFY(!completePrompt.contains(QStringLiteral("已联网")));
QVERIFY(!completePrompt.contains(QStringLiteral("搜索结果")));
```

- [ ] **Step 2: Build and verify RED**

Run target `selection_context_model_request_tests`. Expected RED: builder API missing.

- [ ] **Step 3: Implement the pure request builder**

```cpp
struct SelectionContextModelRequestInput
{
    QString actionId;
    QString selectedText;
    QString previousAnswer;
    QString followUpQuestion;
    AppSettingsData settings;
    PromptRuntimeSnapshot prompts;
};

struct SelectionContextModelRequest
{
    bool valid = false;
    bool degraded = false;
    QString degradedMessage;
    QString errorCode;
    QString errorMessage;
    ModelRequestTaskRequest modelRequest;
};

SelectionContextModelRequest buildSelectionContextModelRequest(
    const SelectionContextModelRequestInput &input
);
```

Map Translate to the existing translate function runtime, Explain and offline AI Search to the existing ask runtime, and custom IDs in the form `function:<stable-id>` to that function's runtime. Use `promptRuntimeForFunction`, the function-specific model selection, `targetLanguage`, `useSystemProxy`, streaming enabled, and the existing provider request boundary. Diagnostic strings expose action ID and text length only.

- [ ] **Step 4: Run request tests**

Expected: no network is used; every test is a pure builder test and GREEN.

- [ ] **Step 5: Commit Task 9**

```powershell
git add src/tasks/selection_context_model_request.* `
  tests/tasks/selection_context_model_request_tests.*
git commit -m "feat: build selection context model requests"
```

---

### Task 10: Run model work asynchronously with ordered deltas and bounded cancellation

**Files:**
- Create: `src/tasks/selection_context_model_runner.h`
- Create: `src/tasks/selection_context_model_runner.cpp`
- Create: `tests/tasks/selection_context_model_runner_tests.cpp`
- Create: `tests/tasks/selection_context_model_runner_tests.pro`

- [ ] **Step 1: Write RED runner tests with an injected provider function**

```cpp
void deltasArriveOnTheOwnerThreadInProviderOrder();
void terminalCallbackRunsAfterAllQueuedDeltas();
void cancellingDropsLateDeltaAndTerminalCallbacks();
void startingAgainCancelsThePreviousExecutionExactlyOnce();
void providerErrorPreservesTheExistingErrorMessageWithoutSelectedTextInLogs();
void callbackMayDeleteRunnerSynchronously();
void destructionCancelsAndDrainsWithoutAResidualWorker();
```

- [ ] **Step 2: Build and verify RED**

Run target `selection_context_model_runner_tests`. Expected RED: runner API missing.

- [ ] **Step 3: Implement a selection-specific async runner**

```cpp
struct SelectionContextModelRunnerCallbacks
{
    std::function<void(const ExecutionId &executionId,
                       const QString &delta)> delta;
    std::function<void(const ExecutionId &executionId,
                       const ModelRequestTaskResult &result)> finished;
};

struct SelectionContextModelRunnerAccess
{
    std::function<ModelRequestTaskResult(
        const ModelRequestTaskRequest &,
        const ModelDeltaCallback &)> runRequest;
};

class SelectionContextModelRunner : public QObject
{
    Q_OBJECT
public:
    ExecutionId start(
        const ModelRequestTaskRequest &request,
        const SelectionContextModelRunnerCallbacks &callbacks
    );
    void cancel();
    bool isRunning() const;
};
```

Follow the proven `FunctionFlowModelTaskRunner` ownership pattern without importing flow state. Use `QtConcurrent`, a unique execution ID, a shared cancellation state, queued owner-thread delivery, a pending-delta counter, and a generation check. Never deliver `finished` before the last accepted delta. Clear callbacks before invoking terminal callbacks and use `QPointer` afterward.

- [ ] **Step 4: Run runner tests three consecutive times**

Expected: all three executions are GREEN and finish without residual worker processes or threads that keep the test executable alive.

- [ ] **Step 5: Commit Task 10**

```powershell
git add src/tasks/selection_context_model_runner.* `
  tests/tasks/selection_context_model_runner_tests.*
git commit -m "feat: run selection context model tasks"
```

---

### Task 11: Execute local actions, AI actions, replacement, and follow-up safely

**Files:**
- Create: `src/controllers/selection_context_action_controller.h`
- Create: `src/controllers/selection_context_action_controller.cpp`
- Modify: `src/output/clipboard_writer.h`
- Modify: `src/output/clipboard_writer.cpp`
- Create: `tests/controllers/selection_context_action_controller_tests.cpp`
- Create: `tests/controllers/selection_context_action_controller_tests.pro`
- Modify: `tests/output/clipboard_writer_tests.cpp`
- Modify: `tests/output/clipboard_writer_tests.pro`

- [ ] **Step 1: Write RED action and clipboard tests**

Cover all action paths:

```cpp
void persistentCopyWritesTextWithoutPasteOrRestoreLease();
void copyClosesTheToolbarAndShowsNoModelResult();
void saveUsesTheGlobalVocabularyBridgeExactlyOnce();
void firstModelActionRequiresExplicitNetworkConsentBeforeProviderStart();
void declinedNetworkConsentSendsNothingAndKeepsLocalActionsAvailable();
void acknowledgedConsentDoesNotPromptAgain();
void modelActionShowsDegradedBannerBeforeOfflineAiSearchDeltas();
void aNewActionCancelsTheOldRunnerAndIgnoresLateCallbacks();
void replaceRequiresOriginalWindowAndLiveSelection();
void pinnedCardNeverOffersReplace();
void replaceUsesCheckedClipboardWriteAndKeepsResultOnFailure();
void followUpCarriesOriginalSelectionAndPreviousAnswer();
void regenerateStartsANewExecutionWithTheSameSnapshotAndAction();
void textOverTwelveThousandCharactersWaitsForExplicitFullTextConsent();
void longTextCancelMakesNoProviderCallAndNeverTruncates();
void closeAndOutsideClickCancelOnlyAnUnpinnedRunningRequest();
void logsContainActionLengthStateAndDurationButNeverText();
void everyInjectedCallbackMayDestroyTheControllerSynchronously();
```

- [ ] **Step 2: Build the two projects and verify RED**

Run `clipboard_writer_tests` and `selection_context_action_controller_tests`. Expected RED: persistent copy API and action controller are missing.

- [ ] **Step 3: Add the explicit persistent-copy boundary**

```cpp
// src/output/clipboard_writer.h
static bool copyText(const QString &text);
```

`copyText` must execute on the GUI thread, set only the clipboard text, and return whether the clipboard readback matches. It must not send Ctrl+V, change focus, create a restore lease, or overwrite other formats after returning.

- [ ] **Step 4: Implement the action controller**

```cpp
struct SelectionContextActionAccess
{
    std::function<bool(const QString &)> copyText;
    std::function<void(const QString &)> saveVocabulary;
    std::function<bool(SelectedTextNativeWindowHandle)> selectionStillExists;
    std::function<ClipboardWriteResult(
        const QString &,
        SelectedTextNativeWindowHandle
    )> replaceSelection;
    std::function<AppSettingsData()> settingsSnapshot;
    std::function<PromptRuntimeSnapshot()> promptSnapshot;
    std::function<bool(const QString &actionId,
                       const QString &modelId)> ensureNetworkConsent;
    std::function<void(const SelectionResultCardState &)> renderResult;
    std::function<void()> closeToolbar;
    std::function<void(const QString &eventId,
                       const QString &actionId,
                       int textLength,
                       qint64 elapsedMs)> logMetadata;
};

class SelectionContextActionController : public QObject
{
    Q_OBJECT
public:
    explicit SelectionContextActionController(
        SelectionContextModelRunner *runner,
        const SelectionContextActionAccess &access,
        QObject *parent = nullptr
    );
    void setSelection(const SelectionSnapshot &snapshot);
    void triggerAction(const QString &actionId);
    void processFullTextConfirmed();
    void regenerate();
    void submitFollowUp(const QString &question);
    void setPinned(bool pinned);
    void cancel();
    void close();
};
```

Copy calls `ClipboardWriter::copyText`. Save calls the existing vocabulary bridge with scope `"__global"`; neither local action may invoke network consent or a provider. Replace is available only while the card is unpinned, the original native window still exists, and `SelectedTextReader::hasSelectionInWindow` succeeds; then call `ClipboardWriter::pasteTextToWindowChecked(result, originalWindow, true, true)`. On failure retain the result card and show a local status. AI actions first build Task 9 requests, then call `ensureNetworkConsent(actionId, modelId)` before Task 10; declining sends no text and starts no provider. If selected text exceeds 12,000 UTF-16 code units, first render `requiresLongTextConfirmation=true`; only `processFullTextConfirmed()` may send the complete original text, and Cancel sends nothing. Follow-up includes original selection, previous committed answer, and explicit question. All callbacks carry the current generation and stale events are ignored.

- [ ] **Step 5: Run both complete test projects**

Expected: GREEN; no test sends real keys, reads real secrets, or calls a network provider.

- [ ] **Step 6: Commit Task 11**

```powershell
git add src/controllers/selection_context_action_controller.* `
  src/output/clipboard_writer.* `
  tests/controllers/selection_context_action_controller_tests.* `
  tests/output/clipboard_writer_tests.*
git commit -m "feat: execute selection context actions"
```

---

### Task 12: Add a real settings card with save rollback and scaled-font validation

**Files:**
- Create: `src/ui/selection_context_settings_card.h`
- Create: `src/ui/selection_context_settings_card.cpp`
- Create: `tests/ui/selection_context_settings_card_tests.cpp`
- Create: `tests/ui/selection_context_settings_card_tests.pro`
- Modify: `src/ui/basic_settings_section.h`
- Modify: `src/ui/basic_settings_section.cpp`
- Modify: `src/ui/settings_panel.cpp`
- Modify: `src/ui/hub_settings_state.h`
- Modify: `src/ui/hub_settings_state.cpp`
- Modify: `tests/ui/basic_settings_section_tests.cpp`
- Modify: `tests/ui/basic_settings_section_tests.pro`
- Modify: `tests/ui/settings_panel_access_factory_tests.cpp`
- Modify: `tests/ui/settings_panel_access_factory_tests.pro`
- Modify: `tests/ui/hub_settings_state_tests.cpp`
- Modify: `tests/ui/hub_settings_state_tests.pro` if the action catalog source is required.

- [ ] **Step 1: Write RED real-widget tests**

Instantiate the real card and the real settings section. Assert:

```cpp
void cardLoadsAndReturnsEveryTypedSetting();
void actionRowsFollowCatalogAndDragReorderPersistsStableIds();
void blockedApplicationsNormalizeOneExecutablePerLine();
void pauseDurationAndKeyboardObservationAreIndependent();
void acceptedNetworkConsentCanBeResetForTheNextModelAction();
void settingsPanelSavePassesTheWholeSelectionContextValue();
void failedSaveRestoresPersistedValuesAndVisibleWidgets();
void strongSelectionRemainsTheExistingGlobalCompatibilitySetting();
void buttonsAndChineseLabelsDoNotClipAt100_125_150_200Percent();
```

The save test must capture the real `AppSettingsData` submitted by the section, return `false`, and then verify every visible control rolled back. Source-text `contains` assertions do not count.

- [ ] **Step 2: Build and verify RED**

Run `selection_context_settings_card_tests` and the affected existing settings tests. Expected RED: card and snapshot plumbing are absent.

- [ ] **Step 3: Implement the card and section plumbing**

The card includes: master enable, keyboard-selection enable, minimum length, close-on-outside-click, pin enable, pause minutes, drag-reorderable action list, one-executable-per-line block list, and a “下次发送前再次提示” reset button when network consent has already been acknowledged. Use `QListWidget::InternalMove` and stable action IDs in `Qt::UserRole`. Keep strong selection in its existing setting and display a short link/explanation rather than duplicating its value.

Embed the card in the General/Basic settings section. Add `HubSettingsState::selectionContextSettings()` and `setSelectionContextSettings(const SelectionContextSettings &)` and pass the complete value through the real snapshot, save, refresh, and rollback paths. Every button uses minimum height `max(40, fontMetrics().height() + 16)` with zero vertical padding; labels use size policies or scroll containers instead of fixed heights.

- [ ] **Step 4: Run native Windows visual tests at four scales**

Set the test font to 100%, 125%, 150%, and 200%; save one PNG per scale under an ignored `build-selection-context-visual` directory. Assert each label/button height is at least its size hint, CJK glyphs have non-zero advance, and title foreground pixels exist. Inspect every image for clipping, overlap, and unreachable controls.

- [ ] **Step 5: Run all affected settings tests**

Expected: behavior and visual tests are GREEN; no PNG is written to the source or test directory.

- [ ] **Step 6: Commit Task 12**

```powershell
git add src/ui/selection_context_settings_card.* `
  src/ui/basic_settings_section.* src/ui/settings_panel.cpp `
  src/ui/hub_settings_state.* `
  tests/ui/selection_context_settings_card_tests.* `
  tests/ui/basic_settings_section_tests.cpp `
  tests/ui/basic_settings_section_tests.pro `
  tests/ui/settings_panel_access_factory_tests.cpp `
  tests/ui/settings_panel_access_factory_tests.pro `
  tests/ui/hub_settings_state_tests.*
git commit -m "feat: configure selection context toolbar"
```

Before committing, replace wildcard staging with the exact changed existing test paths shown by `git status --short`; do not stage unrelated generated files.

---

### Task 13: Assemble one runtime feature and expose hotkey and tray controls

**Files:**
- Create: `src/app/selection_context_feature.h`
- Create: `src/app/selection_context_feature.cpp`
- Create: `tests/app/selection_context_feature_tests.cpp`
- Create: `tests/app/selection_context_feature_tests.pro`
- Modify: `src/controllers/tray_controller.h`
- Modify: `src/controllers/tray_controller.cpp`
- Modify: `tests/controllers/tray_controller_tests.cpp`
- Modify: `tests/controllers/tray_controller_tests.pro` if new production dependencies require it.
- Modify: `src/app/vocekit_application_runtime.cpp`
- Modify: `vocekit.pro`

- [ ] **Step 1: Write RED feature, tray, and composition tests**

```cpp
void startAndStopOwnExactlyOneObserverAndRunner();
void refreshAppliesSettingsWithoutRecreatingVisiblePinnedResult();
void pinDetachesCurrentCardAndNextActionUsesANewCardUpToThreePinned();
void fallbackHotkeyRoutesToFeatureInsteadOfFunctionDispatcher();
void moreMenuListsEachCustomFunctionOnceAndRoutesStableIds();
void pauseCurrentApplicationPersistsItsExecutableAndClosesSurfaces();
void openSettingsMenuActionInvokesTheExistingSettingsEntry();
void trayShowsEnablePauseThirtyMinutesAndResumeActions();
void trayCheckStateRefreshesBeforeEveryShow();
void runtimeSaveVocabularyUsesVoiceControllerGlobalBridgeExactlyOnce();
void runtimeReplaceRevalidatesTheOriginalWindowSelection();
void runtimeConsentPromptPersistsAcceptanceAndDeclineSendsNothing();
void settingsChangeRefreshesFeatureAndHotkeyRegistration();
void teardownStopsHooksBeforeDestroyingUiAndModelRunner();
```

- [ ] **Step 2: Build and verify RED**

Run `selection_context_feature_tests` and `tray_controller_tests`. Expected RED: feature and tray callbacks are absent; the main Release build in Step 5 is the production composition/link boundary.

- [ ] **Step 3: Implement the assembly boundary**

```cpp
struct SelectionContextFeatureAccess
{
    std::function<AppSettingsData()> settingsSnapshot;
    std::function<PromptRuntimeSnapshot()> promptSnapshot;
    std::function<void(const QString &)> saveVocabulary;
    std::function<void(const QString &executable)> blockApplication;
    std::function<void()> openSettings;
    std::function<bool(const QString &actionId,
                       const QString &modelId)> ensureNetworkConsent;
    std::function<void(const QString &eventId,
                       const QString &actionId,
                       int textLength,
                       qint64 elapsedMs)> logMetadata;
};

class SelectionContextFeature : public QObject
{
    Q_OBJECT
public:
    explicit SelectionContextFeature(
        const SelectionContextFeatureAccess &access,
        QObject *parent = nullptr
    );
    void start();
    void stop();
    void refresh();
    void triggerFallbackShortcut();
    void pauseForMinutes(int minutes);
    void resume();
    bool isPaused() const;
};
```

`SelectionContextFeature` owns observer, coordinator, toolbar, one current unpinned session, and up to three detached pinned sessions in destruction-safe order. Each session owns its own result card, model runner, action controller, immutable snapshot, and generation; pinning therefore does not cancel an in-flight request. The next model action creates a fresh current session. When three are pinned, disable Pin on the current card instead of silently deleting an older result. Closing a pinned session first cancels its runner, clears callbacks, then removes its widgets/objects with `deleteLater` and `QPointer` guards.

The runtime constructs the feature after `VoiceController`, supplies `voiceController.addVocabularyForFlow(text, "__global", "")`, and supplies an `ensureNetworkConsent` callback. That callback returns immediately when `settings.toData().selectionContext.networkConsentAcknowledged` is true; otherwise it shows an explicit modal saying the selected text will be sent to the configured model service, and only on acceptance sets the typed setting true, calls `settings.save()`, and refreshes settings. It must not include selected text in the dialog or logs. Build the More menu from every current `FunctionSettings` as `function:<id>` exactly once, then append pause-current-application and open-settings actions. Feature routing intercepts only those two menu IDs; every `function:<id>` is forwarded unchanged to the action controller. Pausing the current application normalizes its executable into `blockedApplications`, saves, refreshes, and closes unpinned surfaces. The runtime intercepts only hotkey ID `selection_toolbar` before the function dispatcher, refreshes the feature after settings changes, and calls `stop()` before application teardown.

Add tray actions for Enable, Pause 30 minutes, and Resume. They must have stable `data()` IDs, live in one exclusive state group where appropriate, connect exactly once, and refresh from feature state on `aboutToShow`.

Register every Task 1-13 production source and header exactly once in `vocekit.pro`. Do not register focused test fakes in the application project.

- [ ] **Step 4: Run focused integration tests**

Expected: feature, tray, hotkey, runtime, settings, action-controller, runner, and widget projects are GREEN; tests use injected provider functions and never invoke the real network.

- [ ] **Step 5: Build the main Release target**

```powershell
$qt = 'D:\QQQQQT0001\5.9\mingw53_32\bin'
$mingw = 'D:\QQQQQT0001\Tools\mingw530_32\bin'
$openssl = 'D:\QQQQQT0001\Tools\mingw530_32\opt\bin'
$env:PATH = "$qt;$mingw;$env:PATH"
& "$qt\qmake.exe" vocekit.pro -spec win32-g++ CONFIG+=release
& "$mingw\mingw32-make.exe" -j2 release
```

Expected: application compilation and final link exit `0` with all new symbols resolved.

- [ ] **Step 6: Commit Task 13**

```powershell
git add src/app/selection_context_feature.* `
  src/app/vocekit_application_runtime.cpp `
  src/controllers/tray_controller.* `
  tests/app/selection_context_feature_tests.* `
  tests/controllers/tray_controller_tests.* vocekit.pro
git commit -m "feat: integrate selection context toolbar"
```

Add the exact runtime assembly test files if changed; verify `git diff --cached --name-only` contains no generated build output before committing.

---

### Task 14: Run the complete security, visual, compatibility, and packaging gate

**Files:**
- Modify: `docs/AI_PROJECT_GUIDE.md`
- Modify: `docs/TESTING.md`
- Modify: focused tests only if this gate exposes a real regression; each fix must begin with a reproducing RED test.

- [ ] **Step 1: Document the delivered behavior and explicit search boundary**

Document automatic selection, fallback hotkey, supported local/AI actions, pause, block list, pin/follow-up, strong-selection opt-in, replace safety, and the exact first-round message `未联网，已使用普通 AI 解答`. State that real web search and source citations are not delivered by this phase.

- [ ] **Step 2: Run all focused projects from fresh object/MOC directories**

Run Tasks 1-13 projects plus affected existing selected-text, clipboard, hotkey, model-request, settings, tray, runtime and packaging tests. Delete only their dedicated ignored build directories before rebuilding; do not delete user or unrelated untracked files.

- [ ] **Step 3: Run the complete repository suite**

```powershell
powershell.exe -ExecutionPolicy Bypass -File .\scripts\run-all-tests.ps1 -Configuration release
```

Expected summary: every project verified, `Failed=0`, `Skipped=0`, and `InfrastructureFailures=0`. If an ignored external OCR asset is absent, supply it through the existing temporary verified fixture wrapper and remove the fixture in `finally`; do not weaken or skip the OCR test.

- [ ] **Step 4: Perform the native Windows interaction matrix**

Use a packaged or isolated settings root. Verify automatic mouse selection, keyboard Shift selection, fallback hotkey, copy, save, translate, explain, offline AI Search degradation, cancel, pin, follow-up, replace success/failure, outside close, pause/resume and stale selection cancellation in:

- Notepad
- Edge and Chrome editable/non-editable text
- Word and WPS
- one PDF viewer
- one Electron application

Also verify password fields never show the toolbar, the app's own windows never trigger it, elevated targets fail safely, keyboard-only selection honors its setting, and clipboard strong fallback restores text plus non-text formats.

- [ ] **Step 5: Perform the native multi-monitor and scaled-font visual gate**

Capture toolbar and card screenshots at 100%, 125%, 150%, and 200% with Chinese text. Include left/right screen edges, taskbar bottom, negative-coordinate monitor, long streaming text, completed text, degraded AI Search banner, error state, pinned follow-up, and replacement-disabled state. Inspect every PNG with `view_image`; no label/button text may clip, no surface may escape the available screen, and long text must scroll inside a bounded card.

- [ ] **Step 6: Audit privacy, focus, and process cleanup**

Search logs and packages for the selected test text and assert zero matches. Verify no selected text appears in normal logs, no UIA probe or model worker remains after application exit, no toolbar click steals focus, no `selection_context_*` test process remains, and no real secret is included in test output or package contents.

- [ ] **Step 7: Rebuild, deploy, package, and verify runtime**

```powershell
$qt = 'D:\QQQQQT0001\5.9\mingw53_32\bin'
$mingw = 'D:\QQQQQT0001\Tools\mingw530_32\bin'
$openssl = 'D:\QQQQQT0001\Tools\mingw530_32\opt\bin'
$env:PATH = "$qt;$mingw;$openssl;$env:PATH"
& "$qt\qmake.exe" vocekit.pro -spec win32-g++ CONFIG+=release
& "$mingw\mingw32-make.exe" -j2 release
powershell.exe -ExecutionPolicy Bypass -File .\scripts\deploy.ps1 `
  -Configuration release -QtBin $qt -MingwBin $mingw -OpenSslBin $openssl
powershell.exe -ExecutionPolicy Bypass -File .\scripts\package-test.ps1 `
  -PackageName vocekit-selection-context-test
powershell.exe -ExecutionPolicy Bypass -File .\scripts\verify-runtime.ps1 `
  -Configuration release `
  -RuntimeDir .\dist\vocekit-selection-context-test
```

Expected: Release build, runtime verification, privacy scan, directory package, and ZIP archive validation all exit `0`.

- [ ] **Step 8: Request final code review and fix only reproduced findings**

Review against the design spec and this plan. Treat focus theft, sensitive-field display, selected-text logging, stale callback delivery, duplicate action execution, clipped Chinese text, false web-search claims, and lingering hooks/processes as release blockers. Add a RED regression test before each correction and rerun the smallest affected set plus the relevant integration set.

- [ ] **Step 9: Run final repository checks and commit documentation/fixes**

```powershell
git diff --check
git status --short
git add docs/AI_PROJECT_GUIDE.md docs/TESTING.md
git commit -m "docs: verify selection context toolbar"
```

Stage any regression-test or production fixes as explicit paths in the same final commit only when they are directly caused by Task 14 findings. Preserve every unrelated pre-existing untracked artifact.

---

## Completion evidence required in the handoff

- Commit SHAs for Tasks 1-14.
- Focused-test pass counts and the full-suite summary.
- Main executable, packaged directory and ZIP SHA-256 hashes.
- The four-scale visual evidence directory and the inspected state matrix.
- Interaction results for every target application in Task 14.
- Privacy scan, clipboard-format restoration, focus-retention, and residual-process results.
- A clear statement that AI Search is an offline ordinary-AI degradation in this release and that real search/provider/citation work remains outside this implementation plan.
