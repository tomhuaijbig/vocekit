# 普通模式与画布模式隔离 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为每个功能增加唯一的 `executionMode`，让普通配置与画布发布配置独立保存，并保证所有快捷键、截图入口和运行时只执行当前模式、绝不跨模式回退。

**Architecture:** `FunctionSettings::executionMode` 是唯一运行事实来源，`FunctionFlowState::enabled` 只保留为旧版本兼容镜像。`FunctionFlowPublicationService` 负责验证并原子切换模式；计划缓存、命令路由、快捷键和截图悬浮入口只读取 `executionMode`。功能页提供同一组模式选择按钮，进入画布与切换执行模式保持为两个独立动作。

**Tech Stack:** Qt 5.9 Widgets、C++11、QtTest、qmake、MinGW 5.3 32 位、现有 `AppSettingsStore`、`ApplicationEvents`、`FunctionFlowPublicationService`、`FunctionFlowPlanCache`、`FunctionCommandController`。

---

## 实施前安全约束

当前工作区包含尚未提交的画布实现，其中部分 `function_flow_*` 和
`function_canvas_*` 文件仍是未跟踪文件。本功能依赖这些当前文件，不能从
`HEAD` 新建空白 worktree 后直接实施。

实施会话必须先从仓库根目录记录基线：

```powershell
Set-Location 'C:\Users\13736\Desktop\tts'
$env:PATH="D:\QQQQQT0001\Tools\mingw530_32\bin;D:\QQQQQT0001\5.9\mingw53_32\bin;$env:PATH"
qmake -v
mingw32-make --version
git status --short | Set-Content -LiteralPath "$env:TEMP\vocekit-mode-isolation-baseline.txt" -Encoding UTF8
```

预期：

- qmake 报告 Qt 5.9 的 `win32-g++`。
- `mingw32-make` 可运行。
- 基线文件保留现有修改，后续不得使用 `git reset --hard`、`git checkout --`
  或清理未跟踪文件。

提交步骤必须遵守：

1. 已跟踪且实施前就有修改的文件使用
   `git add -p -- vocekit/src/domain/function_settings.cpp` 这种带明确路径的命令，
   只暂存本计划新增的模式隔离 hunks。
2. 未跟踪的画布文件只有在该任务实际修改且缓存差异已完整审阅后才能按完整文件
   暂存。
3. 每次提交前运行 `git diff --cached --name-status` 和
   `git diff --cached --check`；出现任务外路径立即取消暂存并核对。
4. 不暂存本计划未列出的用户文件。

实施会话定义统一测试函数：

```powershell
function Invoke-CodexQtTest {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Project,
        [ValidateSet("debug", "release")]
        [string]$Configuration = "debug"
    )

    $projectPath = (Resolve-Path -LiteralPath $Project).Path
    $directory = Split-Path -Parent $projectPath
    $projectName = Split-Path -Leaf $projectPath
    $targetMatch = Select-String -LiteralPath $projectPath `
        -Pattern '^\s*TARGET\s*=\s*(.+?)\s*$' |
        Select-Object -First 1
    if (-not $targetMatch) {
        throw "Missing TARGET in $projectPath"
    }
    $target = $targetMatch.Matches[0].Groups[1].Value.Trim()
    $makefile = "Makefile.codex.$target"
    $previousQpaPlatform = $env:QT_QPA_PLATFORM

    Push-Location $directory
    try {
        qmake -o $makefile $projectName -spec win32-g++ `
            "CONFIG+=$Configuration"
        if ($LASTEXITCODE -ne 0) {
            throw "qmake failed: $target"
        }
        mingw32-make -f $makefile -j2
        if ($LASTEXITCODE -ne 0) {
            throw "build failed: $target"
        }
        $exe = Join-Path $Configuration "$target.exe"
        if (-not (Test-Path -LiteralPath $exe)) {
            $exe = ".\$target.exe"
        }
        if (-not (Test-Path -LiteralPath $exe)) {
            throw "test executable missing: $target"
        }
        $env:QT_QPA_PLATFORM = "offscreen"
        & $exe -maxwarnings 0
        if ($LASTEXITCODE -ne 0) {
            throw "test failed: $target"
        }
    } finally {
        if ($null -eq $previousQpaPlatform) {
            Remove-Item Env:QT_QPA_PLATFORM -ErrorAction SilentlyContinue
        } else {
            $env:QT_QPA_PLATFORM = $previousQpaPlatform
        }
        Pop-Location
    }
}
```

每次绿色测试必须看到 QtTest 的 `0 failed` 且进程退出码为 0；只完成 qmake 或
编译不算测试通过。

## 文件职责映射

### 配置与领域模型

- Modify: `vocekit/src/domain/function_settings.h`
  - 定义 `FunctionExecutionMode`、稳定 ID 转换函数和唯一模式字段。
- Modify: `vocekit/src/domain/function_settings.cpp`
  - 规范化模式并同步 `flow.enabled` 兼容镜像。
- Modify: `vocekit/src/config/app_settings_json.cpp`
  - 读取新字段、按旧 `enabled` 迁移、保留未知值并写回兼容镜像。
- Modify: `vocekit/src/config/app_settings_store.cpp`
  - 普通设置保存时保留最新模式和画布状态。

### 模式服务与运行分流

- Modify: `vocekit/src/app/application_events.h`
- Modify: `vocekit/src/app/application_events.cpp`
  - 用 `functionExecutionModeSettingsKey()` 取代旧启用事件。
- Modify: `vocekit/src/controllers/function_flow_publication_service.h`
- Modify: `vocekit/src/controllers/function_flow_publication_service.cpp`
  - 发布与启用解耦；集中验证并原子切换执行模式。
- Modify: `vocekit/src/controllers/function_flow_plan_cache.cpp`
  - 只为画布模式建立运行计划。
- Modify: `vocekit/src/controllers/function_flow_execution_controller.cpp`
  - 缺少计划或当前入口时返回配置错误，不再返回可回退结果。
- Modify: `vocekit/src/domain/function_flow_errors.cpp`
  - 注册 `flow_trigger_not_configured` 的稳定用户提示。
- Modify: `vocekit/src/controllers/function_command_controller.h`
- Modify: `vocekit/src/controllers/function_command_controller.cpp`
  - 普通模式只走普通流程，画布模式只走画布流程。
- Modify: `vocekit/src/input/hotkey_settings_snapshot.h`
- Modify: `vocekit/src/input/hotkey_settings_snapshot.cpp`
  - 根据当前模式构建互斥快捷键和截图入口画像。
- Modify: `vocekit/src/app/vocekit_application_runtime.cpp`
  - 装配模式服务、严格启动错误和互斥截图悬浮入口。

### 界面与刷新

- Modify: `vocekit/src/ui/function_flow_settings_access.h`
  - 暴露类型化 `setExecutionMode`，移除 `setEnabled`。
- Modify: `vocekit/src/ui/function_command_page.h`
- Modify: `vocekit/src/ui/function_command_page.cpp`
  - 增加“当前执行”双选控件并把导航按钮改为“编辑画布”。
- Modify: `vocekit/src/ui/function_canvas_editor.h`
- Modify: `vocekit/src/ui/function_canvas_editor.cpp`
  - “应用流程”改为“发布流程”，移除“停用流程”。
- Modify: `vocekit/src/controllers/function_flow_editor_controller.h`
- Modify: `vocekit/src/controllers/function_flow_editor_controller.cpp`
  - 发布成功不再改变执行模式。
- Modify: `vocekit/src/ui/hub_settings_state.cpp`
  - 窄刷新同时读取最新 `executionMode` 和 `flow`。
- Modify: `vocekit/src/ui/hub_refresh_coordinator_bundle.h`
- Modify: `vocekit/src/ui/hub_refresh_coordinator_bundle.cpp`
- Modify: `vocekit/src/ui/hub_refresh_coordinator_action_factory.cpp`
  - 模式事件刷新当前功能页、运行计划和快捷键。

### 测试

- Modify: `vocekit/tests/config/function_flow_json_tests.cpp`
- Modify: `vocekit/tests/config/app_settings_json_tests.cpp`
- Modify: `vocekit/tests/app/application_events_tests.cpp`
- Modify: `vocekit/tests/app/function_flow_settings_event_tests.cpp`
- Modify: `vocekit/tests/controllers/function_flow_publication_service_tests.cpp`
- Modify: `vocekit/tests/controllers/function_flow_plan_cache_tests.cpp`
- Modify: `vocekit/tests/controllers/function_flow_execution_controller_tests.cpp`
- Modify: `vocekit/tests/controllers/function_command_controller_tests.cpp`
- Modify: `vocekit/tests/controllers/function_flow_fallback_tests.cpp`
- Modify: `vocekit/tests/domain/function_flow_errors_tests.cpp`
- Modify: `vocekit/tests/input/hotkey_settings_snapshot_tests.cpp`
- Modify: `vocekit/tests/ui/function_canvas_editor_tests.cpp`
- Modify: `vocekit/tests/ui/function_command_page_tests.cpp`
- Modify: `vocekit/tests/ui/hub_settings_state_tests.cpp`
- Modify: `vocekit/tests/ui/hub_refresh_coordinator_bundle_tests.cpp`
- Modify: `vocekit/tests/ui/hub_refresh_coordinator_action_factory_tests.cpp`

---

### Task 1: 建立唯一执行模式并完成 JSON 迁移

**Files:**

- Modify: `vocekit/src/domain/function_settings.h:65`
- Modify: `vocekit/src/domain/function_settings.cpp:136`
- Modify: `vocekit/src/config/app_settings_json.cpp:429-579,607-613`
- Test: `vocekit/tests/config/function_flow_json_tests.cpp`

- [ ] **Step 1: 先写新字段、旧字段迁移和未知值保留测试**

在 `function_flow_json_tests.cpp` 增加三个测试槽，并使用以下断言主体：

```cpp
void FunctionFlowJsonTests::executionModeRoundTripsAndMirrorsEnabled()
{
    QJsonObject root;
    QJsonObject hotkeys;
    hotkeys.insert(QStringLiteral("dictate"), QStringLiteral("Alt+D"));
    root.insert(QStringLiteral("hotkeys"), hotkeys);

    QJsonObject flow = validFunctionFlowJson();
    flow.insert(QStringLiteral("executionMode"), QStringLiteral("canvas"));
    flow.insert(QStringLiteral("enabled"), false);
    QJsonObject flows;
    flows.insert(QStringLiteral("dictate"), flow);
    root.insert(QStringLiteral("functionFlows"), flows);

    const AppSettingsData data = appSettingsDataFromJson(root);
    const FunctionSettings dictate = data.function(QStringLiteral("dictate"));
    QCOMPARE(dictate.executionMode, FunctionExecutionMode::Canvas);
    QVERIFY(dictate.flow.enabled);

    const QJsonObject writtenFlow =
        appSettingsDataToJson(data)
            .value(QStringLiteral("functionFlows")).toObject()
            .value(QStringLiteral("dictate")).toObject();
    QCOMPARE(
        writtenFlow.value(QStringLiteral("executionMode")).toString(),
        QStringLiteral("canvas")
    );
    QVERIFY(writtenFlow.value(QStringLiteral("enabled")).toBool());
}

void FunctionFlowJsonTests::legacyEnabledMigratesExecutionMode()
{
    QJsonObject flow = validFunctionFlowJson();
    flow.remove(QStringLiteral("executionMode"));
    flow.insert(QStringLiteral("enabled"), true);
    QJsonObject flows;
    flows.insert(QStringLiteral("dictate"), flow);
    QJsonObject root;
    root.insert(QStringLiteral("functionFlows"), flows);

    const AppSettingsData data = appSettingsDataFromJson(root);
    QCOMPARE(
        data.function(QStringLiteral("dictate")).executionMode,
        FunctionExecutionMode::Canvas
    );
}

void FunctionFlowJsonTests::unknownExecutionModeIsRetainedButRunsClassic()
{
    QJsonObject flow = validFunctionFlowJson();
    flow.insert(
        QStringLiteral("executionMode"),
        QStringLiteral("future-mode")
    );
    flow.insert(QStringLiteral("enabled"), true);
    QJsonObject flows;
    flows.insert(QStringLiteral("dictate"), flow);
    QJsonObject root;
    root.insert(QStringLiteral("functionFlows"), flows);

    const AppSettingsData data = appSettingsDataFromJson(root);
    const FunctionSettings dictate = data.function(QStringLiteral("dictate"));
    QCOMPARE(dictate.executionMode, FunctionExecutionMode::Classic);
    QVERIFY(!dictate.flow.enabled);

    const QJsonObject writtenFlow =
        appSettingsDataToJson(data)
            .value(QStringLiteral("functionFlows")).toObject()
            .value(QStringLiteral("dictate")).toObject();
    QCOMPARE(
        writtenFlow.value(QStringLiteral("executionMode")).toString(),
        QStringLiteral("future-mode")
    );
    QVERIFY(!writtenFlow.value(QStringLiteral("enabled")).toBool());
}
```

- [ ] **Step 2: 运行配置测试确认红灯**

Run:

```powershell
Invoke-CodexQtTest 'vocekit/tests/config/function_flow_json_tests.pro'
```

Expected: 编译失败，指出 `FunctionExecutionMode` 或
`FunctionSettings::executionMode` 尚不存在。

- [ ] **Step 3: 增加模式类型、稳定 ID 和兼容镜像**

在 `function_settings.h` 中、`FunctionSettings` 之前增加：

```cpp
enum class FunctionExecutionMode
{
    Classic,
    Canvas
};

QString functionExecutionModeId(FunctionExecutionMode mode);
FunctionExecutionMode functionExecutionModeFromId(
    const QString &id,
    bool *known = nullptr
);
```

并在 `FunctionSettings` 中、`flow` 之前增加：

```cpp
FunctionExecutionMode executionMode = FunctionExecutionMode::Classic;
```

在 `function_settings.cpp` 增加：

```cpp
QString functionExecutionModeId(FunctionExecutionMode mode)
{
    return mode == FunctionExecutionMode::Canvas
        ? QStringLiteral("canvas")
        : QStringLiteral("classic");
}

FunctionExecutionMode functionExecutionModeFromId(
    const QString &id,
    bool *known)
{
    const QString normalized = id.trimmed();
    const bool isCanvas = normalized == QStringLiteral("canvas");
    const bool isClassic = normalized == QStringLiteral("classic");
    if (known) {
        *known = isCanvas || isClassic;
    }
    return isCanvas
        ? FunctionExecutionMode::Canvas
        : FunctionExecutionMode::Classic;
}
```

在 `normalizeFunctionSettings()` 返回前同步只读兼容镜像：

```cpp
normalized.flow.enabled =
    normalized.executionMode == FunctionExecutionMode::Canvas;
```

- [ ] **Step 4: 在 `functionFlows[id]` 中读取和写出新模式**

在 `app_settings_json.cpp` 的匿名命名空间增加：

```cpp
FunctionExecutionMode executionModeFromFlowJson(
    const QJsonObject &flowObject)
{
    const QJsonValue value =
        flowObject.value(QStringLiteral("executionMode"));
    if (value.isString()) {
        bool known = false;
        const FunctionExecutionMode parsed =
            functionExecutionModeFromId(value.toString(), &known);
        return known ? parsed : FunctionExecutionMode::Classic;
    }
    return flowObject.value(QStringLiteral("enabled")).toBool(false)
        ? FunctionExecutionMode::Canvas
        : FunctionExecutionMode::Classic;
}

QJsonObject functionFlowJson(const FunctionSettings &settings)
{
    QJsonObject object = functionFlowStateToJson(settings.flow);
    const QJsonValue retained =
        settings.flow.retainedValues.value(
            QStringLiteral("executionMode")
        );
    bool retainedKnown = false;
    if (retained.isString()) {
        functionExecutionModeFromId(
            retained.toString(),
            &retainedKnown
        );
    }
    if (!retained.isString() || retainedKnown) {
        object.insert(
            QStringLiteral("executionMode"),
            functionExecutionModeId(settings.executionMode)
        );
    }
    object.insert(
        QStringLiteral("enabled"),
        settings.executionMode == FunctionExecutionMode::Canvas
    );
    return object;
}
```

把读取 `functionFlows` 的分支改为先让新字段胜出，再把兼容镜像交给现有流程
JSON 读取器：

```cpp
const QJsonObject rawFlow = it.value().toObject();
data.functions[index].executionMode =
    executionModeFromFlowJson(rawFlow);
QJsonObject normalizedFlow = rawFlow;
normalizedFlow.insert(
    QStringLiteral("enabled"),
    data.functions[index].executionMode
        == FunctionExecutionMode::Canvas
);
data.functions[index].flow =
    functionFlowStateFromJson(normalizedFlow, warnings);
data.functions[index] =
    normalizeFunctionSettings(data.functions[index]);
```

把写出调用改为：

```cpp
functionFlows.insert(settings.id, functionFlowJson(settings));
```

- [ ] **Step 5: 运行配置测试确认绿灯**

Run:

```powershell
Invoke-CodexQtTest 'vocekit/tests/config/function_flow_json_tests.pro'
```

Expected: `0 failed`。同时原有草稿、发布图、孤儿流程和未知节点保留测试继续通过。

- [ ] **Step 6: 提交配置模型变更**

```powershell
git add -p -- `
  vocekit/src/domain/function_settings.h `
  vocekit/src/domain/function_settings.cpp `
  vocekit/src/config/app_settings_json.cpp `
  vocekit/tests/config/function_flow_json_tests.cpp
git diff --cached --check
git commit -m "feat: add function execution mode migration"
```

Expected: 缓存中只有本任务四个路径的模式字段、迁移逻辑和测试 hunks。

---

### Task 2: 普通设置保存不得覆盖模式和画布状态

**Files:**

- Modify: `vocekit/src/config/app_settings_store.cpp:215-259`
- Test: `vocekit/tests/config/app_settings_json_tests.cpp:374-425`

- [ ] **Step 1: 扩展旧快照合并测试**

在 `mergesOnlyNonFlowSettingsFromAnOlderSnapshot()` 中明确制造模式冲突：

```cpp
current.functions[customIndex].executionMode =
    FunctionExecutionMode::Canvas;
current.functions[customIndex] =
    normalizeFunctionSettings(current.functions[customIndex]);

stale.functions[customIndex].executionMode =
    FunctionExecutionMode::Classic;
stale.functions[customIndex] =
    normalizeFunctionSettings(stale.functions[customIndex]);
```

保存后增加：

```cpp
const FunctionSettings saved =
    store.function(QStringLiteral("custom_1"));
QCOMPARE(saved.executionMode, FunctionExecutionMode::Canvas);
QVERIFY(saved.flow.enabled);
```

- [ ] **Step 2: 运行测试确认旧实现会被错误快照覆盖**

Run:

```powershell
Invoke-CodexQtTest 'vocekit/tests/config/app_settings_json_tests.pro'
```

Expected: `saved.executionMode` 实际为 `Classic`，测试失败。

- [ ] **Step 3: 合并普通设置时保留最新执行状态**

在 `replaceNonFlowSettingsAndSave()` 的功能合并循环中使用：

```cpp
editedFunction.builtIn = current.builtIn;
editedFunction.executionMode = current.executionMode;
editedFunction.flow = current.flow;
editedFunction = normalizeFunctionSettings(editedFunction);
```

这保证普通设置页的旧快照只能更新普通字段，不能关闭或启用画布。

- [ ] **Step 4: 运行配置存储测试**

Run:

```powershell
Invoke-CodexQtTest 'vocekit/tests/config/app_settings_json_tests.pro'
```

Expected: `0 failed`，并且保存失败回滚、功能集合冲突和孤儿流程保留测试不回退。

- [ ] **Step 5: 提交合并隔离**

```powershell
git add -p -- `
  vocekit/src/config/app_settings_store.cpp `
  vocekit/tests/config/app_settings_json_tests.cpp
git diff --cached --check
git commit -m "fix: preserve execution mode during classic saves"
```

---

### Task 3: 发布与模式切换彻底解耦

**Files:**

- Modify: `vocekit/src/app/application_events.h:31-36`
- Modify: `vocekit/src/app/application_events.cpp:18-26`
- Modify: `vocekit/src/controllers/function_flow_publication_service.h:55-64`
- Modify: `vocekit/src/controllers/function_flow_publication_service.cpp:483-718`
- Modify: `vocekit/src/ui/function_flow_settings_access.h:41-49`
- Test: `vocekit/tests/app/application_events_tests.cpp`
- Test: `vocekit/tests/controllers/function_flow_publication_service_tests.cpp`

- [ ] **Step 1: 写发布不切模式和模式切换验证测试**

在 publication service 测试中加入：

```cpp
void FunctionFlowPublicationServiceTests::
publishDoesNotChangeClassicExecutionMode()
{
    Harness harness;
    harness.settings.functions << functionSettings();
    const FunctionFlowPublishResult result =
        harness.service.publish(QStringLiteral("custom_1"), 3);

    QVERIFY(result.ok);
    const FunctionSettings saved =
        harness.function(QStringLiteral("custom_1"));
    QCOMPARE(saved.executionMode, FunctionExecutionMode::Classic);
    QVERIFY(!saved.flow.enabled);
    QCOMPARE(
        harness.events.last().key,
        functionFlowPublishedSettingsKey()
    );
}

void FunctionFlowPublicationServiceTests::
canvasModeRequiresValidPublishedGraph()
{
    Harness harness;
    harness.settings.functions << functionSettings();
    OperationError error;

    QVERIFY(!harness.service.setExecutionMode(
        QStringLiteral("custom_1"),
        FunctionExecutionMode::Canvas,
        &error
    ));
    QCOMPARE(error.code, QStringLiteral("flow_published_unavailable"));
    QCOMPARE(
        harness.function(QStringLiteral("custom_1")).executionMode,
        FunctionExecutionMode::Classic
    );
}

void FunctionFlowPublicationServiceTests::
classicModeRetainsPublishedGraph()
{
    Harness harness;
    FunctionSettings function = functionSettings();
    function.flow.published =
        publishedVersion(validGraph(), 2, 3);
    function.executionMode = FunctionExecutionMode::Canvas;
    function = normalizeFunctionSettings(function);
    harness.settings.functions << function;

    OperationError error;
    QVERIFY(harness.service.setExecutionMode(
        function.id,
        FunctionExecutionMode::Classic,
        &error
    ));
    const FunctionSettings saved = harness.function(function.id);
    QCOMPARE(saved.executionMode, FunctionExecutionMode::Classic);
    QCOMPARE(saved.flow.published.revision, 2);
    QCOMPARE(
        harness.events.last().key,
        functionExecutionModeSettingsKey()
    );
}
```

- [ ] **Step 2: 运行服务测试确认红灯**

Run:

```powershell
Invoke-CodexQtTest 'vocekit/tests/controllers/function_flow_publication_service_tests.pro'
```

Expected: `setExecutionMode` 和新事件键尚不存在，编译失败。

- [ ] **Step 3: 用独立模式事件取代启用事件**

在 `application_events.h/.cpp` 定义：

```cpp
QString functionExecutionModeSettingsKey();
```

```cpp
QString functionExecutionModeSettingsKey()
{
    return QStringLiteral("function.executionMode");
}
```

删除 `functionFlowEnabledSettingsKey()` 的声明、实现和后续调用。同步
`application_events_tests.cpp`，要求稳定键集合包含
`function.executionMode`，不再包含 `functionFlowEnabled`。

- [ ] **Step 4: 把 `setEnabled` 改为类型化模式服务**

在 publication service 头文件声明：

```cpp
bool setExecutionMode(
    const QString &functionId,
    FunctionExecutionMode mode,
    OperationError *error
);
```

实现必须从最新快照取功能；切到画布时复用现有发布图完整性、领域验证和编译
检查，切回普通模式不校验发布图：

```cpp
bool FunctionFlowPublicationService::setExecutionMode(
    const QString &functionId,
    FunctionExecutionMode mode,
    OperationError *error)
{
    clearError(error);
    AppSettingsData settings =
        m_access.settingsSnapshotProvider
            ? m_access.settingsSnapshotProvider()
            : AppSettingsData();
    const int index = settings.functionIndex(functionId);
    if (index < 0) {
        setError(
            error,
            QStringLiteral("flow_function_not_found"),
            QStringLiteral("功能不存在。")
        );
        return false;
    }

    FunctionSettings &function = settings.functions[index];
    if (function.executionMode == mode) {
        return true;
    }

    if (mode == FunctionExecutionMode::Canvas) {
        const VersionedFunctionFlowGraph &published =
            function.flow.published;
        if (!published.supported || published.revision <= 0) {
            setError(
                error,
                QStringLiteral("flow_published_unavailable"),
                QStringLiteral("没有可切换到画布模式的发布流程。"),
                published.unavailableCode
            );
            return false;
        }
        const QString integrityProblem =
            publishedIntegrityError(published, true);
        if (!integrityProblem.isEmpty()) {
            setError(
                error,
                integrityProblem,
                QStringLiteral("发布流程的完整性校验失败。")
            );
            return false;
        }
        const FunctionFlowValidationResult validation =
            FunctionFlowValidator::validateForPublish(
                published.graph,
                validationContext(m_access, settings, functionId)
            );
        if (!validation.ok) {
            if (error) {
                *error = validationError(validation);
            }
            return false;
        }
        const FunctionFlowCompileResult compiled =
            FunctionFlowCompiler::compile(
                published.graph,
                published.revision,
                published.graphHash
            );
        if (!compiled.ok) {
            if (error) {
                *error = compiled.error;
            }
            return false;
        }
    }

    function.executionMode = mode;
    function.flow.retainedValues.remove(
        QStringLiteral("executionMode")
    );
    function = normalizeFunctionSettings(function);
    if (!saveSnapshot(m_access, settings, error)) {
        return false;
    }
    publishChange(
        m_access,
        functionExecutionModeSettingsKey(),
        functionId
    );
    return true;
}
```

- [ ] **Step 5: 发布路径只更新 `published`**

从 `publish()` 删除全部 `flow.enabled = true` 分支和
`functionFlowEnabledSettingsKey()` 事件。相同哈希直接返回成功；新哈希只保存
`flow.published` 并发布 `functionFlowPublishedSettingsKey()`：

```cpp
if (sameHash) {
    result.ok = true;
    result.publishedRevision = flow.published.revision;
    return result;
}

VersionedFunctionFlowGraph published;
published.revision = nextRevision;
published.sourceDraftRevision = flow.draft.revision;
published.graphHash = analysis.graphHash;
published.graph = graph;
flow.published = published;
if (!saveSnapshot(m_access, settings, &result.error)) {
    return result;
}
result.ok = true;
result.publishedRevision = nextRevision;
publishChange(
    m_access,
    functionFlowPublishedSettingsKey(),
    functionId
);
return result;
```

`publishedIntegrityError()` 在发布检查中传入
`function.executionMode == FunctionExecutionMode::Canvas`，不再读取镜像决定运行
状态。

- [ ] **Step 6: 更新窄访问接口**

在 `FunctionFlowSettingsAccess` 删除 `setEnabled` 并加入：

```cpp
std::function<bool(
    const QString &,
    FunctionExecutionMode,
    OperationError *
)> setExecutionMode;
```

所有测试 fake 同步使用类型化模式。

- [ ] **Step 7: 运行服务和事件测试**

Run:

```powershell
Invoke-CodexQtTest 'vocekit/tests/app/application_events_tests.pro'
Invoke-CodexQtTest 'vocekit/tests/controllers/function_flow_publication_service_tests.pro'
```

Expected: 两个工程均 `0 failed`。发布事件只报告 published；模式事件只在保存
成功后报告 execution mode。

- [ ] **Step 8: 提交服务边界变更**

```powershell
git add -p -- `
  vocekit/src/app/application_events.h `
  vocekit/src/app/application_events.cpp `
  vocekit/src/controllers/function_flow_publication_service.h `
  vocekit/src/controllers/function_flow_publication_service.cpp `
  vocekit/src/ui/function_flow_settings_access.h `
  vocekit/tests/app/application_events_tests.cpp `
  vocekit/tests/controllers/function_flow_publication_service_tests.cpp
git diff --cached --check
git commit -m "feat: separate flow publishing from execution mode"
```

---

### Task 4: 计划缓存和启动错误只认画布模式

**Files:**

- Modify: `vocekit/src/controllers/function_flow_plan_cache.cpp:23-72`
- Modify: `vocekit/src/controllers/function_flow_execution_controller.cpp:165-299`
- Modify: `vocekit/src/domain/function_flow_errors.cpp`
- Test: `vocekit/tests/controllers/function_flow_plan_cache_tests.cpp`
- Test: `vocekit/tests/controllers/function_flow_execution_controller_tests.cpp`
- Test: `vocekit/tests/domain/function_flow_errors_tests.cpp`

- [ ] **Step 1: 写缓存和入口错误测试**

在 plan cache 测试中把“启用/停用”用例改为：

```cpp
settings.functions[0].executionMode = FunctionExecutionMode::Classic;
settings.functions[0] =
    normalizeFunctionSettings(settings.functions[0]);
cache.rebuildFunction(settings, settings.functions[0].id);
QVERIFY(cache.plan(settings.functions[0].id).isNull());

settings.functions[0].executionMode = FunctionExecutionMode::Canvas;
settings.functions[0] =
    normalizeFunctionSettings(settings.functions[0]);
cache.rebuildFunction(settings, settings.functions[0].id);
QVERIFY(!cache.plan(settings.functions[0].id).isNull());
```

在 execution controller 测试增加：

```cpp
void FunctionFlowExecutionControllerTests::
missingTriggerIsAConfigurationError()
{
    FunctionFlowExecutionController controller(
        FunctionFlowRuntimeAccess()
    );
    FunctionFlowExecutionPlan plan;
    plan.functionId = QStringLiteral("custom_1");

    QCOMPARE(
        controller.start(
            QStringLiteral("custom_1"),
            plan,
            FunctionFlowTrigger::ScreenshotLauncher,
            nullptr
        ),
        FunctionFlowStartOutcome::ConfigurationError
    );
    QCOMPARE(
        controller.lastStartError().code,
        QStringLiteral("flow_trigger_not_configured")
    );
}
```

并在错误码测试的 `requiredStableCodes()` 中加入：

```cpp
QStringLiteral("flow_trigger_not_configured")
```

- [ ] **Step 2: 运行三个工程确认红灯**

```powershell
Invoke-CodexQtTest 'vocekit/tests/controllers/function_flow_plan_cache_tests.pro'
Invoke-CodexQtTest 'vocekit/tests/controllers/function_flow_execution_controller_tests.pro'
Invoke-CodexQtTest 'vocekit/tests/domain/function_flow_errors_tests.pro'
```

Expected: 缓存仍读取 `flow.enabled`，缺少入口仍返回 `NotAvailable`，新错误码尚未
注册。

- [ ] **Step 3: 缓存只读取 `executionMode`**

把 `FunctionFlowPlanCache::rebuildFunction()` 的模式门卫改为：

```cpp
const FunctionSettings &function = settings.functions.at(index);
if (function.executionMode != FunctionExecutionMode::Canvas) {
    return;
}
const FunctionFlowState &flow = function.flow;
```

本文件不再出现 `flow.enabled`。

- [ ] **Step 4: 缺少计划或入口都返回配置错误**

在 `FunctionFlowExecutionController::start(functionId, plan, ...)` 的入口不可用分支
写入：

```cpp
if (!triggerPlan.available) {
    m_lastStartError = executionError(
        QStringLiteral("flow_trigger_not_configured"),
        QString::fromUtf8("当前画布未配置此入口。")
    );
    return FunctionFlowStartOutcome::ConfigurationError;
}
```

在 `start(request, sharedPlan)` 的空计划分支写入：

```cpp
if (plan.isNull()) {
    m_lastStartError = executionError(
        QStringLiteral("flow_published_unavailable"),
        QString::fromUtf8("当前画布没有可运行的发布流程。")
    );
    return FunctionFlowStartOutcome::ConfigurationError;
}
```

- [ ] **Step 5: 注册精确用户提示**

在 `function_flow_errors.cpp` 的 runtime 定义中加入
`flow_trigger_not_configured`，并覆盖为精确消息：

```cpp
addDefinition(
    &result,
    QStringLiteral("flow_trigger_not_configured"),
    runtimeFaq,
    flowErrorText("当前画布未配置此入口。")
);
```

- [ ] **Step 6: 运行缓存、控制器和错误测试**

```powershell
Invoke-CodexQtTest 'vocekit/tests/controllers/function_flow_plan_cache_tests.pro'
Invoke-CodexQtTest 'vocekit/tests/controllers/function_flow_execution_controller_tests.pro'
Invoke-CodexQtTest 'vocekit/tests/domain/function_flow_errors_tests.pro'
```

Expected: 三个工程均 `0 failed`；入口缺失不再产生 `NotAvailable`。

- [ ] **Step 7: 提交严格缓存和错误语义**

```powershell
git add -- `
  vocekit/src/controllers/function_flow_plan_cache.cpp `
  vocekit/src/controllers/function_flow_execution_controller.cpp
git add -p -- `
  vocekit/src/domain/function_flow_errors.cpp `
  vocekit/tests/controllers/function_flow_plan_cache_tests.cpp `
  vocekit/tests/controllers/function_flow_execution_controller_tests.cpp `
  vocekit/tests/domain/function_flow_errors_tests.cpp
git diff --cached --check
git commit -m "fix: make canvas plan failures terminal"
```

---

### Task 5: 命令控制器取消所有跨模式回退

**Files:**

- Modify: `vocekit/src/controllers/function_command_controller.h:119-137`
- Modify: `vocekit/src/controllers/function_command_controller.cpp:43-288,449-478`
- Test: `vocekit/tests/controllers/function_command_controller_tests.cpp`
- Test: `vocekit/tests/controllers/function_flow_fallback_tests.cpp`

- [ ] **Step 1: 把旧 fallback 测试改成模式隔离契约**

在 `function_flow_fallback_tests.cpp` 将用例替换为以下三个槽：

```cpp
void FunctionFlowFallbackTests::classicModeNeverCallsFlow()
{
    int flowCalls = 0;
    int classicCalls = 0;
    FunctionCommandAccess access;
    access.captureTargetWindow = []() {
        return reinterpret_cast<void *>(quintptr(91));
    };
    access.startPublishedFlow =
        [&flowCalls](const FunctionFlowTriggerRequest &) {
            ++flowCalls;
            return FunctionFlowStartOutcome::Started;
        };
    access.readSelectedText =
        [](const SelectedTextWorkflowRequest &) {
            SelectedTextWorkflowResult result;
            result.text = QStringLiteral("classic");
            return result;
        };
    access.processText = [&classicCalls](const QString &, const QString &) {
        ++classicCalls;
    };

    AppSettingsData settings;
    FunctionSettings function;
    function.id = QStringLiteral("custom_1");
    function.name = QStringLiteral("Custom");
    function.input.useSelection = true;
    settings.functions << normalizeFunctionSettings(function);

    FunctionCommandController controller(access);
    controller.updateConfiguration(settings);
    QCOMPARE(
        controller.handleHotkey(function.id),
        FunctionCommandOutcome::TextSubmitted
    );
    QCOMPARE(flowCalls, 0);
    QCOMPARE(classicCalls, 1);
}

void FunctionFlowFallbackTests::canvasModeNeverCallsClassic()
{
    int flowCalls = 0;
    int classicCalls = 0;
    FunctionCommandAccess access;
    access.captureTargetWindow = []() {
        return reinterpret_cast<void *>(quintptr(92));
    };
    access.startPublishedFlow =
        [&flowCalls](const FunctionFlowTriggerRequest &) {
            ++flowCalls;
            return FunctionFlowStartOutcome::NotAvailable;
        };
    access.processText = [&classicCalls](const QString &, const QString &) {
        ++classicCalls;
    };

    AppSettingsData settings;
    FunctionSettings function;
    function.id = QStringLiteral("custom_1");
    function.name = QStringLiteral("Custom");
    function.executionMode = FunctionExecutionMode::Canvas;
    settings.functions << normalizeFunctionSettings(function);

    FunctionCommandController controller(access);
    controller.updateConfiguration(settings);
    QCOMPARE(
        controller.handleHotkey(function.id),
        FunctionCommandOutcome::FlowConfigurationFailed
    );
    QCOMPARE(flowCalls, 1);
    QCOMPARE(classicCalls, 0);
}

void FunctionFlowFallbackTests::canvasLauncherNeverUsesClassicScreenshot()
{
    int screenshots = 0;
    FunctionCommandAccess access;
    access.startPublishedFlow =
        [](const FunctionFlowTriggerRequest &) {
            return FunctionFlowStartOutcome::NotAvailable;
        };
    access.beginScreenshot =
        [&screenshots](const QString &, bool, bool) {
            ++screenshots;
            return true;
        };

    AppSettingsData settings;
    FunctionSettings function;
    function.id = QStringLiteral("custom_1");
    function.name = QStringLiteral("Custom");
    function.executionMode = FunctionExecutionMode::Canvas;
    settings.functions << normalizeFunctionSettings(function);

    FunctionCommandController controller(access);
    controller.updateConfiguration(settings);
    QCOMPARE(
        controller.handleScreenshotLauncherTrigger(
            function.id,
            reinterpret_cast<void *>(quintptr(100))
        ),
        FunctionCommandOutcome::FlowConfigurationFailed
    );
    QCOMPARE(screenshots, 0);
}
```

- [ ] **Step 2: 运行路由测试确认红灯**

```powershell
Invoke-CodexQtTest 'vocekit/tests/controllers/function_flow_fallback_tests.pro'
```

Expected: 普通模式仍先调用 flow，画布 `NotAvailable` 仍执行普通流程，至少两个
断言失败。

- [ ] **Step 3: 让 `tryStartPublishedFlow()` 先检查模式**

把方法实现改为：

```cpp
FunctionCommandOutcome
FunctionCommandController::tryStartPublishedFlow(
    const QString &functionId,
    FunctionFlowTrigger trigger) const
{
    const QString id = functionId.trimmed();
    const FunctionSettings function = m_settings.function(id);
    if (function.executionMode != FunctionExecutionMode::Canvas) {
        return FunctionCommandOutcome::NoAction;
    }
    if (!m_access.startPublishedFlow) {
        if (m_access.showError) {
            m_access.showError(commandText(
                "画布运行服务尚未初始化。"
            ));
        }
        return FunctionCommandOutcome::FlowConfigurationFailed;
    }

    FunctionFlowTriggerRequest request;
    request.functionId = id;
    request.trigger = trigger;
    request.targetWindow = m_targetWindow;
    request.classicWorkflowBusy = classicWorkflowBusy();
    const FunctionFlowStartOutcome outcome =
        m_access.startPublishedFlow(request);
    if (outcome == FunctionFlowStartOutcome::NotAvailable) {
        if (m_access.showError) {
            m_access.showError(commandText(
                "当前画布未配置此入口。"
            ));
        }
        return FunctionCommandOutcome::FlowConfigurationFailed;
    }
    return commandOutcomeForFlow(outcome);
}
```

现有 `handleHotkey()` 和 `handleScreenshotLauncherTrigger()` 可以保留
`NoAction` 后进入普通路径，因为只有 `Classic` 才能得到 `NoAction`。

- [ ] **Step 4: 按键释放也按模式分流**

把 `handleHotkeyReleased()` 开头改成：

```cpp
const FunctionSettings function = m_settings.function(id.trimmed());
if (function.executionMode == FunctionExecutionMode::Canvas
    && m_access.releasePublishedFlowHold
    && m_access.releasePublishedFlowHold(id)) {
    return FunctionCommandOutcome::RecordingHandled;
}
```

普通模式不调用画布释放回调；画布模式不调用普通录音释放回调。为此后续普通回调
条件写为：

```cpp
if (function.executionMode == FunctionExecutionMode::Classic
    && m_access.recordingConsumesRelease
    && m_access.recordingConsumesRelease(id)) {
    return FunctionCommandOutcome::RecordingHandled;
}
```

- [ ] **Step 5: 更新原命令控制器用例**

在 `function_command_controller_tests.cpp`：

- 原 `flowStartRunsBeforeClassicBusyGuard` 的功能设置显式设为 `Canvas`。
- 原 `onlyNotAvailableFallsBackToClassic` 改为断言
  `FlowConfigurationFailed` 且普通截图次数为 0。
- 为普通模式补充 `startPublishedFlow` 调用次数为 0。

使用一致设置：

```cpp
FunctionSettings function = functionSettings(
    QStringLiteral("translate"),
    true,
    false
);
function.executionMode = FunctionExecutionMode::Canvas;
settings.functions << normalizeFunctionSettings(function);
controller.updateConfiguration(settings);
```

- [ ] **Step 6: 运行两个命令路由工程**

```powershell
Invoke-CodexQtTest 'vocekit/tests/controllers/function_command_controller_tests.pro'
Invoke-CodexQtTest 'vocekit/tests/controllers/function_flow_fallback_tests.pro'
```

Expected: 两个工程均 `0 failed`，所有画布失败路径的普通调用次数为 0。

- [ ] **Step 7: 提交命令隔离**

```powershell
git add -p -- `
  vocekit/src/controllers/function_command_controller.h `
  vocekit/src/controllers/function_command_controller.cpp `
  vocekit/tests/controllers/function_command_controller_tests.cpp
git add -- vocekit/tests/controllers/function_flow_fallback_tests.cpp
git diff --cached --check
git commit -m "fix: remove cross-mode command fallback"
```

---

### Task 6: 快捷键、截图悬浮入口和应用装配遵守当前模式

**Files:**

- Modify: `vocekit/src/input/hotkey_settings_snapshot.h`
- Modify: `vocekit/src/input/hotkey_settings_snapshot.cpp:15-177`
- Modify: `vocekit/src/app/vocekit_application_runtime.cpp:311-422,534-594,988-1021`
- Test: `vocekit/tests/input/hotkey_settings_snapshot_tests.cpp`

- [ ] **Step 1: 写互斥快捷键和悬浮入口测试**

把 `missingPublishedTriggerKeepsClassicEntrance()` 改为：

```cpp
void missingPublishedTriggerDoesNotKeepClassicEntrance()
{
    AppSettingsData settings;
    FunctionSettings custom;
    custom.id = QStringLiteral("custom-2");
    custom.name = QStringLiteral("Canvas only");
    custom.shortcut = QStringLiteral("Alt+2");
    custom.input.useScreenshot = true;
    custom.input.screenshotTriggerMode =
        QStringLiteral("separateAndLauncher");
    custom.input.screenshotShortcut =
        QStringLiteral("Alt+Shift+2");
    custom.executionMode = FunctionExecutionMode::Canvas;
    settings.functions.append(normalizeFunctionSettings(custom));

    QSharedPointer<FunctionFlowExecutionPlan> plan(
        new FunctionFlowExecutionPlan
    );
    FunctionFlowTriggerPlan main;
    main.available = true;
    plan->triggers.insert(FunctionFlowTrigger::MainHotkey, main);

    const GlobalHotkeySettingsSnapshot snapshot =
        globalHotkeySnapshotFromData(
            settings,
            [plan](const QString &id) {
                return id == QStringLiteral("custom-2")
                    ? QSharedPointer<
                        const FunctionFlowExecutionPlan
                      >(plan)
                    : QSharedPointer<
                        const FunctionFlowExecutionPlan
                      >();
            }
        );
    const GlobalHotkeyFunction function =
        functionById(snapshot, QStringLiteral("custom-2"));
    QVERIFY(!function.registerScreenshotHotkey);
    QVERIFY(!function.useScreenshot);
    QVERIFY(!functionUsesScreenshotLauncher(
        settings.functions.first(),
        QSharedPointer<const FunctionFlowExecutionPlan>(plan)
    ));
}
```

再增加普通模式忽略画布计划：

```cpp
void classicModeIgnoresPublishedProfiles()
{
    AppSettingsData settings;
    FunctionSettings custom;
    custom.id = QStringLiteral("custom-3");
    custom.name = QStringLiteral("Classic");
    custom.shortcut = QStringLiteral("Alt+3");
    custom.input.useVoice = true;
    custom.recording.triggerMode = QStringLiteral("toggle");
    settings.functions.append(normalizeFunctionSettings(custom));

    QSharedPointer<FunctionFlowExecutionPlan> plan(
        new FunctionFlowExecutionPlan
    );
    FunctionFlowTriggerPlan main;
    main.available = true;
    main.usesHoldToTalk = true;
    plan->triggers.insert(FunctionFlowTrigger::MainHotkey, main);

    const GlobalHotkeySettingsSnapshot snapshot =
        globalHotkeySnapshotFromData(
            settings,
            [plan](const QString &) {
                return QSharedPointer<
                    const FunctionFlowExecutionPlan
                >(plan);
            }
        );
    const GlobalHotkeyFunction function =
        functionById(snapshot, custom.id);
    QVERIFY(!function.useHoldToTalk);
    QCOMPARE(
        function.recordingTriggerMode,
        QStringLiteral("toggle")
    );
}
```

- [ ] **Step 2: 运行快捷键测试确认红灯**

```powershell
Invoke-CodexQtTest 'vocekit/tests/input/hotkey_settings_snapshot_tests.pro'
```

Expected: 画布缺失入口仍保留普通截图快捷键，测试失败。

- [ ] **Step 3: 增加共享截图悬浮入口判断**

在 `hotkey_settings_snapshot.h` 声明：

```cpp
bool functionUsesScreenshotLauncher(
    const FunctionSettings &function,
    const QSharedPointer<const FunctionFlowExecutionPlan> &plan
);
```

在 `.cpp` 实现：

```cpp
bool functionUsesScreenshotLauncher(
    const FunctionSettings &function,
    const QSharedPointer<const FunctionFlowExecutionPlan> &plan)
{
    if (function.executionMode == FunctionExecutionMode::Classic) {
        return function.input.useScreenshot
            && screenshotTriggerUsesLauncher(
                function.input.screenshotTriggerMode
            );
    }
    return !plan.isNull()
        && plan->triggers.value(
            FunctionFlowTrigger::ScreenshotLauncher
        ).available;
}
```

- [ ] **Step 4: 构建画布画像前清空普通执行字段**

增加：

```cpp
void clearClassicExecutionProfile(GlobalHotkeyFunction *function)
{
    if (!function) {
        return;
    }
    function->recordingTriggerMode = QStringLiteral("toggle");
    function->useVoice = false;
    function->useScreenshot = false;
    function->useHoldToTalk = false;
    function->registerScreenshotHotkey = false;
    function->screenshotShortcut.clear();
}
```

两个生成循环只在当前模式为 `Canvas` 时读取 plan：

```cpp
if (functionSettings.executionMode
        == FunctionExecutionMode::Canvas) {
    clearClassicExecutionProfile(&function);
    const QSharedPointer<const FunctionFlowExecutionPlan> plan =
        flowPlanProvider
            ? flowPlanProvider(functionSettings.id)
            : QSharedPointer<const FunctionFlowExecutionPlan>();
    if (!plan.isNull()) {
        applyPublishedFlowProfiles(*plan, &function);
    }
}
```

普通模式完全保留 `globalHotkeyFunctionFromData()` 结果。

- [ ] **Step 5: 更新应用装配和错误展示**

将 `FunctionFlowSettingsAccess` 装配改为：

```cpp
functionFlows.setExecutionMode = [&publicationService](
    const QString &functionId,
    FunctionExecutionMode mode,
    OperationError *error
) {
    return publicationService.setExecutionMode(
        functionId,
        mode,
        error
    );
};
```

截图悬浮入口循环改为：

```cpp
const QSharedPointer<const FunctionFlowExecutionPlan> plan =
    functionFlowPlanCache.plan(function.id);
if (!functionUsesScreenshotLauncher(function, plan)) {
    continue;
}
```

启动画布前若缓存没有计划，优先显示缓存错误，不让执行控制器返回可回退结果：

```cpp
const QSharedPointer<const FunctionFlowExecutionPlan> plan =
    functionFlowPlanCache.plan(request.functionId);
if (plan.isNull()) {
    OperationError error =
        functionFlowPlanCache.error(request.functionId);
    if (error.code.trimmed().isEmpty()) {
        error.code = QStringLiteral("flow_published_unavailable");
    }
    showAttentionWarning(
        hub.data(),
        tr8("功能流程配置错误"),
        functionFlowUserMessage(error)
    );
    return FunctionFlowStartOutcome::ConfigurationError;
}
const FunctionFlowStartOutcome outcome =
    flowExecutionController->start(request, plan);
```

- [ ] **Step 6: 运行快捷键测试**

```powershell
Invoke-CodexQtTest 'vocekit/tests/input/hotkey_settings_snapshot_tests.pro'
```

Expected: `0 failed`。Canvas 缺少截图入口时，普通独立快捷键和普通悬浮入口均为
关闭。

- [ ] **Step 7: 编译主工程验证装配签名**

```powershell
Push-Location 'vocekit'
qmake -o Makefile.codex.mode-isolation vocekit.pro -spec win32-g++ "CONFIG+=debug"
mingw32-make -f Makefile.codex.mode-isolation -j2
$buildExit = $LASTEXITCODE
Pop-Location
if ($buildExit -ne 0) { throw 'main build failed' }
```

Expected: 主工程编译成功，不存在 `setEnabled` 或
`functionFlowEnabledSettingsKey` 的残留引用。

- [ ] **Step 8: 提交入口装配**

```powershell
git add -p -- `
  vocekit/src/input/hotkey_settings_snapshot.h `
  vocekit/src/input/hotkey_settings_snapshot.cpp `
  vocekit/src/app/vocekit_application_runtime.cpp `
  vocekit/tests/input/hotkey_settings_snapshot_tests.cpp
git diff --cached --check
git commit -m "fix: isolate hotkeys and launcher by execution mode"
```

---

### Task 7: 在设置页和画布页增加一致的模式选择

**Files:**

- Modify: `vocekit/src/ui/function_command_page.h:47-77`
- Modify: `vocekit/src/ui/function_command_page.cpp:466-635`
- Modify: `vocekit/src/ui/function_canvas_editor.h:21-124`
- Modify: `vocekit/src/ui/function_canvas_editor.cpp:176-243,451-470,886-968`
- Modify: `vocekit/src/controllers/function_flow_editor_controller.h`
- Modify: `vocekit/src/controllers/function_flow_editor_controller.cpp:853-948`
- Test: `vocekit/tests/ui/function_command_page_tests.cpp`
- Test: `vocekit/tests/ui/function_canvas_editor_tests.cpp`

- [ ] **Step 1: 写功能页模式选择测试**

扩展 `FakePageFlows`，记录模式调用并同步 fake 持久化快照：

```cpp
AppSettingsData *settingsData = nullptr;
QHash<QString, FunctionExecutionMode> modes;
int modeChanges = 0;
bool failModeChange = false;
```

fake access：

```cpp
result.setExecutionMode = [this](
    const QString &id,
    FunctionExecutionMode mode,
    OperationError *error
) {
    ++modeChanges;
    if (failModeChange) {
        if (error) {
            error->code = QStringLiteral("flow_mode_save_failed");
            error->message = QStringLiteral("mode save failed");
        }
        return false;
    }
    modes[id] = mode;
    states[id].enabled = mode == FunctionExecutionMode::Canvas;
    if (settingsData) {
        const int index = settingsData->functionIndex(id);
        if (index >= 0) {
            settingsData->functions[index].executionMode = mode;
            settingsData->functions[index] =
                normalizeFunctionSettings(
                    settingsData->functions[index]
                );
        }
    }
    return true;
};
```

在 `PageEnvironment` 完成功能列表初始化后绑定 fake 的最新设置来源：

```cpp
flows.settingsData = &data;
```

增加页面测试：

```cpp
void FunctionCommandPageTests::
modeSelectorIsVisibleOnSettingsAndCanvasWithoutNavigating()
{
    PageEnvironment environment;
    environment.data.functions[0].flow.published.revision = 1;
    environment.data.functions[0].flow.published.graphHash =
        functionFlowGraphHash(
            environment.data.functions[0].flow.published.graph
        );
    environment.flows.modes.insert(
        QStringLiteral("custom_1"),
        FunctionExecutionMode::Classic
    );
    FunctionCommandPage page(environment.pageAccess);
    QVERIFY(page.setFunctionId(QStringLiteral("custom_1")));
    showPage(&page);

    QPushButton *classic = page.findChild<QPushButton *>(
        QStringLiteral("functionClassicModeButton")
    );
    QPushButton *canvasMode = page.findChild<QPushButton *>(
        QStringLiteral("functionCanvasModeButton")
    );
    QPushButton *navigation = canvasToggle(&page);
    QVERIFY(classic);
    QVERIFY(canvasMode);
    QVERIFY(navigation);
    QVERIFY(classic->isChecked());
    QCOMPARE(navigation->text(), QString::fromUtf8("编辑画布"));

    navigation->click();
    QCoreApplication::processEvents();
    QVERIFY(page.canvasEditor());
    QCOMPARE(environment.flows.modeChanges, 0);
    QVERIFY(page.findChild<QPushButton *>(
        QStringLiteral("functionClassicModeButton")
    ));
}
```

增加切换失败恢复测试：

```cpp
void FunctionCommandPageTests::failedModeSwitchRestoresSelection()
{
    PageEnvironment environment;
    environment.flows.failModeChange = true;
    FunctionCommandPage page(environment.pageAccess);
    QVERIFY(page.setFunctionId(QStringLiteral("custom_1")));
    showPage(&page);

    QPushButton *canvasMode = page.findChild<QPushButton *>(
        QStringLiteral("functionCanvasModeButton")
    );
    canvasMode->click();
    QCoreApplication::processEvents();

    QVERIFY(page.findChild<QPushButton *>(
        QStringLiteral("functionClassicModeButton")
    )->isChecked());
    QCOMPARE(environment.reportedErrors, 1);
}
```

- [ ] **Step 2: 写画布发布按钮测试**

把 footer 用例的对象名期望改为只包含：

```cpp
QStringList() << QStringLiteral("flowPublishButton")
```

并断言：

```cpp
QCOMPARE(publish->text(), QString::fromUtf8("发布流程"));
QVERIFY(!editor.findChild<QPushButton *>(
    QStringLiteral("flowDisableButton")
));
```

发布成功后 fake 的执行模式仍为 `Classic`，并验证信息提示为：

```cpp
QCOMPARE(
    shownMessage,
    QString::fromUtf8("流程已发布；切换到画布模式后生效。")
);
```

- [ ] **Step 3: 运行两个 UI 工程确认红灯**

```powershell
Invoke-CodexQtTest 'vocekit/tests/ui/function_command_page_tests.pro'
Invoke-CodexQtTest 'vocekit/tests/ui/function_canvas_editor_tests.pro'
```

Expected: 模式按钮不存在，导航仍显示“画布”，footer 仍有停用按钮。

- [ ] **Step 4: 在共用页头增加分段模式控件**

在 `FunctionCommandPage` 增加私有方法：

```cpp
QWidget *executionModeSelector(
    const FunctionSettings &function
);
bool setExecutionMode(FunctionExecutionMode mode);
```

控件实现使用稳定对象名：

```cpp
QWidget *FunctionCommandPage::executionModeSelector(
    const FunctionSettings &function)
{
    QWidget *container = new QWidget;
    container->setObjectName(
        QStringLiteral("functionExecutionModeSelector")
    );
    QHBoxLayout *layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    QLabel *label = new QLabel(text8("当前执行"), container);
    QPushButton *classic =
        new QPushButton(text8("普通模式"), container);
    QPushButton *canvas =
        new QPushButton(text8("画布模式"), container);
    classic->setObjectName(
        QStringLiteral("functionClassicModeButton")
    );
    canvas->setObjectName(
        QStringLiteral("functionCanvasModeButton")
    );
    classic->setCheckable(true);
    canvas->setCheckable(true);
    classic->setMinimumHeight(34);
    canvas->setMinimumHeight(34);

    QButtonGroup *group = new QButtonGroup(container);
    group->setExclusive(true);
    group->addButton(classic);
    group->addButton(canvas);
    classic->setChecked(
        function.executionMode == FunctionExecutionMode::Classic
    );
    canvas->setChecked(
        function.executionMode == FunctionExecutionMode::Canvas
    );

    connect(classic, &QPushButton::clicked, container, [this]() {
        setExecutionMode(FunctionExecutionMode::Classic);
    });
    connect(canvas, &QPushButton::clicked, container, [this]() {
        setExecutionMode(FunctionExecutionMode::Canvas);
    });

    layout->addWidget(label);
    layout->addSpacing(8);
    layout->addWidget(classic);
    layout->addWidget(canvas);
    return container;
}
```

切换方法只调用模式服务，不改变 `m_canvasMode`：

```cpp
bool FunctionCommandPage::setExecutionMode(
    FunctionExecutionMode mode)
{
    if (!m_access.settings) {
        return false;
    }
    const FunctionSettings current =
        m_access.settings->toData().function(m_functionId);
    if (current.executionMode == mode) {
        return true;
    }
    OperationError error;
    if (!m_access.flows.setExecutionMode
        || !m_access.flows.setExecutionMode(
            m_functionId,
            mode,
            &error
        )) {
        reportFlowFailure(error);
        refresh();
        return false;
    }
    m_access.settings->reloadFunctionFlowState(m_functionId);
    refresh();
    return true;
}
```

在页头获取完整功能设置后加入：

```cpp
const FunctionSettings function =
    m_access.settings->toData().function(id);
headerLayout->addWidget(executionModeSelector(function));
```

导航文字改为：

```cpp
m_canvasMode ? text8("返回设置") : text8("编辑画布")
```

导航按钮的 toggled 回调仍只调用 `setCanvasMode()`。

- [ ] **Step 5: 发布 footer 移除启用概念**

在 `FunctionCanvasEditor`：

- `m_publishButton` 文案改为“发布流程”。
- 删除 `m_disableButton` 成员、创建、样式、布局和连接。
- `refreshStatus()` 不再检查 `flowState().enabled`。
- 错误标题改为“发布流程失败”。

删除 `FunctionFlowEditorController::setFlowEnabled()`。发布成功分支删除：

```cpp
m_state.enabled = true;
```

保留发布修复确认和发布版本更新。

- [ ] **Step 6: 普通模式发布成功给出明确信息**

在 `FunctionCanvasEditorAccess` 增加：

```cpp
std::function<FunctionExecutionMode(const QString &)>
    executionModeProvider;
std::function<void(const QString &, const QString &)> showInformation;
```

`FunctionCommandPage::ensureCanvasEditor()` 从最新设置提供模式；发布成功后执行：

```cpp
if (result.outcome
        == FunctionFlowEditorPublishOutcome::Succeeded
    && m_access.executionModeProvider
    && m_access.executionModeProvider(m_controller->functionId())
        == FunctionExecutionMode::Classic) {
    const QString title = QString::fromUtf8("流程已发布");
    const QString message = QString::fromUtf8(
        "流程已发布；切换到画布模式后生效。"
    );
    if (m_access.showInformation) {
        m_access.showInformation(title, message);
    } else {
        QMessageBox::information(this, title, message);
    }
}
```

- [ ] **Step 7: 运行 UI 测试**

```powershell
Invoke-CodexQtTest 'vocekit/tests/ui/function_command_page_tests.pro'
Invoke-CodexQtTest 'vocekit/tests/ui/function_canvas_editor_tests.pro'
```

Expected: 两个工程均 `0 failed`；进入画布不切执行模式，发布不切执行模式，失败
切换恢复原按钮。

- [ ] **Step 8: 提交 UI**

```powershell
git add -p -- `
  vocekit/src/ui/function_command_page.h `
  vocekit/src/ui/function_command_page.cpp `
  vocekit/tests/ui/function_command_page_tests.cpp
git add -- `
  vocekit/src/ui/function_canvas_editor.h `
  vocekit/src/ui/function_canvas_editor.cpp `
  vocekit/src/controllers/function_flow_editor_controller.h `
  vocekit/src/controllers/function_flow_editor_controller.cpp `
  vocekit/tests/ui/function_canvas_editor_tests.cpp
git diff --cached --check
git commit -m "feat: add explicit function execution mode controls"
```

---

### Task 8: 模式事件刷新页面、计划和快捷键

**Files:**

- Modify: `vocekit/src/ui/hub_settings_state.cpp:156-184`
- Modify: `vocekit/src/ui/hub_refresh_coordinator_bundle.h:11-19`
- Modify: `vocekit/src/ui/hub_refresh_coordinator_bundle.cpp:30-76`
- Modify: `vocekit/src/ui/hub_refresh_coordinator_action_factory.cpp:5-24`
- Test: `vocekit/tests/ui/hub_settings_state_tests.cpp`
- Test: `vocekit/tests/ui/hub_refresh_coordinator_bundle_tests.cpp`
- Test: `vocekit/tests/ui/hub_refresh_coordinator_action_factory_tests.cpp`
- Test: `vocekit/tests/app/function_flow_settings_event_tests.cpp`

- [ ] **Step 1: 写窄刷新和事件路由测试**

在 `hub_settings_state_tests.cpp` 增加：

```cpp
void HubSettingsStateTests::
reloadFunctionFlowStateAlsoReloadsExecutionMode()
{
    AppSettingsData latest;
    FunctionSettings function;
    function.id = QStringLiteral("custom_1");
    function.name = QStringLiteral("Custom");
    latest.functions << normalizeFunctionSettings(function);

    HubWindowAccess access;
    access.settingsSnapshotProvider = [&latest]() {
        return latest;
    };
    HubSettingsState state(access);

    latest.functions[0].executionMode =
        FunctionExecutionMode::Canvas;
    latest.functions[0] =
        normalizeFunctionSettings(latest.functions[0]);
    QVERIFY(state.reloadFunctionFlowState(function.id));
    QCOMPARE(
        state.toData().function(function.id).executionMode,
        FunctionExecutionMode::Canvas
    );
}
```

在 bundle 测试中对模式事件断言：

```cpp
SettingsChangeSet mode;
mode.keys << functionExecutionModeSettingsKey();
mode.functionIds << QStringLiteral("custom_1");
bundle.apply(mode);

QCOMPARE(reloadedIds, mode.functionIds);
QCOMPARE(activeFunctionRefreshes, 1);
QCOMPARE(runtimeRefreshes, 1);
QCOMPARE(hotkeyRefreshes, 1);
```

在 `function_flow_settings_event_tests.cpp` 用新键替换 enabled 用例，并要求 draft
和 editor 事件仍不刷新运行计划。

- [ ] **Step 2: 运行刷新测试确认红灯**

```powershell
Invoke-CodexQtTest 'vocekit/tests/ui/hub_settings_state_tests.pro'
Invoke-CodexQtTest 'vocekit/tests/ui/hub_refresh_coordinator_bundle_tests.pro'
Invoke-CodexQtTest 'vocekit/tests/ui/hub_refresh_coordinator_action_factory_tests.pro'
Invoke-CodexQtTest 'vocekit/tests/app/function_flow_settings_event_tests.pro'
```

Expected: `reloadFunctionFlowState()` 只复制 flow，新模式键没有刷新当前功能页。

- [ ] **Step 3: 窄刷新复制模式和兼容镜像**

把 `reloadFunctionFlowState()` 改为分别按功能 ID 查找当前索引和最新索引，不能
假设两个快照的功能顺序相同：

```cpp
const int latestIndex = latest.functionIndex(functionId);
const int currentIndex = m_data.functionIndex(functionId);
if (latestIndex < 0 || currentIndex < 0) {
    return false;
}
FunctionSettings &current = m_data.functions[currentIndex];
const FunctionSettings &incoming =
    latest.functions.at(latestIndex);
current.executionMode = incoming.executionMode;
current.flow = incoming.flow;
current = normalizeFunctionSettings(current);
refreshCustomFunctions();
return true;
```

这里不复制普通配置字段，因此远端模式事件不会覆盖当前页的普通设置编辑状态。

- [ ] **Step 4: 模式事件刷新当前功能页**

在 `HubRefreshCoordinatorBundleActions` 增加：

```cpp
std::function<void()> refreshActiveFunction;
```

factory 装配：

```cpp
actions.refreshActiveFunction = ui.refreshActiveFunction;
```

bundle 识别键改为包含：

```cpp
functionExecutionModeSettingsKey()
```

并计算：

```cpp
const bool modeChanged =
    change.keys.contains(functionExecutionModeSettingsKey());
const bool runtimeChanged =
    definitionsChanged
    || change.keys.contains(functionFlowPublishedSettingsKey())
    || modeChanged;
```

窄 reload 和 canvas refresh 之后增加：

```cpp
if (modeChanged && m_actions.refreshActiveFunction) {
    m_actions.refreshActiveFunction();
}
```

运行计划和快捷键仍在 `runtimeChanged` 分支刷新。

- [ ] **Step 5: 运行刷新测试**

```powershell
Invoke-CodexQtTest 'vocekit/tests/ui/hub_settings_state_tests.pro'
Invoke-CodexQtTest 'vocekit/tests/ui/hub_refresh_coordinator_bundle_tests.pro'
Invoke-CodexQtTest 'vocekit/tests/ui/hub_refresh_coordinator_action_factory_tests.pro'
Invoke-CodexQtTest 'vocekit/tests/app/function_flow_settings_event_tests.pro'
```

Expected: 四个工程均 `0 failed`；模式事件刷新页面、runtime 和 hotkeys 各一次，
草稿/视口事件不触发 runtime。

- [ ] **Step 6: 全局扫描旧启用真相源**

Run:

```powershell
$oldApiRefs = rg -n `
  'functionFlowEnabledSettingsKey|setEnabled\s*=|setFlowEnabled' `
  vocekit/src vocekit/tests
if ($LASTEXITCODE -eq 0) {
    $oldApiRefs
    throw 'legacy flow enabled APIs remain'
}
if ($LASTEXITCODE -ne 1) {
    throw 'rg failed'
}

$runtimeMirrorRefs = rg -n 'flow\.enabled\s*=' `
  vocekit/src/app `
  vocekit/src/controllers `
  vocekit/src/input `
  vocekit/src/ui
if ($LASTEXITCODE -eq 0) {
    $runtimeMirrorRefs
    throw 'runtime flow enabled writers remain'
}
if ($LASTEXITCODE -ne 1) {
    throw 'rg failed'
}
```

Expected: 无匹配。允许的兼容镜像读取/断言必须集中在
`function_settings.cpp`、JSON 和迁移测试中；若扫描模式需要精确放宽，逐项列出
路径后再放宽，不能忽略运行时代码命中。

- [ ] **Step 7: 提交刷新链路**

```powershell
git add -p -- `
  vocekit/src/ui/hub_settings_state.cpp `
  vocekit/src/ui/hub_refresh_coordinator_bundle.h `
  vocekit/src/ui/hub_refresh_coordinator_bundle.cpp `
  vocekit/src/ui/hub_refresh_coordinator_action_factory.cpp `
  vocekit/tests/ui/hub_settings_state_tests.cpp `
  vocekit/tests/ui/hub_refresh_coordinator_bundle_tests.cpp `
  vocekit/tests/ui/hub_refresh_coordinator_action_factory_tests.cpp
git add -- vocekit/tests/app/function_flow_settings_event_tests.cpp
git diff --cached --check
git commit -m "feat: refresh execution mode across the application"
```

---

### Task 9: 全量回归、主程序构建和可用性验收

**Files:**

- Verify: `vocekit/src/**`
- Verify: `vocekit/tests/**`
- Verify: `vocekit/vocekit.pro`
- Verify: `vocekit/scripts/run-all-tests.ps1`

- [ ] **Step 1: 运行模式隔离专项测试组**

```powershell
$projects = @(
  'vocekit/tests/config/function_flow_json_tests.pro',
  'vocekit/tests/config/app_settings_json_tests.pro',
  'vocekit/tests/app/application_events_tests.pro',
  'vocekit/tests/app/function_flow_settings_event_tests.pro',
  'vocekit/tests/controllers/function_flow_publication_service_tests.pro',
  'vocekit/tests/controllers/function_flow_plan_cache_tests.pro',
  'vocekit/tests/controllers/function_flow_execution_controller_tests.pro',
  'vocekit/tests/controllers/function_command_controller_tests.pro',
  'vocekit/tests/controllers/function_flow_fallback_tests.pro',
  'vocekit/tests/domain/function_flow_errors_tests.pro',
  'vocekit/tests/input/hotkey_settings_snapshot_tests.pro',
  'vocekit/tests/ui/function_command_page_tests.pro',
  'vocekit/tests/ui/function_canvas_editor_tests.pro',
  'vocekit/tests/ui/hub_settings_state_tests.pro',
  'vocekit/tests/ui/hub_refresh_coordinator_bundle_tests.pro',
  'vocekit/tests/ui/hub_refresh_coordinator_action_factory_tests.pro'
)
foreach ($project in $projects) {
    Invoke-CodexQtTest $project
}
```

Expected: 每个工程均 `0 failed`。

- [ ] **Step 2: 运行完整测试发现器**

```powershell
& 'vocekit/scripts/run-all-tests.ps1'
if ($LASTEXITCODE -ne 0) {
    throw 'full test suite failed'
}
```

Expected: 脚本发现全部唯一 TARGET，最终退出码 0。任何与本改动无关的既有失败也
必须记录具体 TARGET 和失败断言，不能写成“基本通过”。

- [ ] **Step 3: 构建 release 主程序**

```powershell
Push-Location 'vocekit'
qmake -o Makefile.codex.mode-isolation-release `
  vocekit.pro -spec win32-g++ "CONFIG+=release"
mingw32-make -f Makefile.codex.mode-isolation-release -j2
$buildExit = $LASTEXITCODE
Pop-Location
if ($buildExit -ne 0) {
    throw 'release build failed'
}
```

Expected: release 构建退出码 0，无未解析符号和旧 `setEnabled` 引用。

- [ ] **Step 4: 验证四条真实用户路径**

启动本次 release 可执行文件后逐条验证：

1. 普通模式：即使已有发布流程，主快捷键也只运行普通配置。
2. 普通模式进入“编辑画布”并发布：页面仍显示普通模式，提示
   “流程已发布；切换到画布模式后生效”。
3. 切换画布模式：仅在合法发布版本存在时成功；删除当前触发入口后再次触发，
   显示“当前画布未配置此入口”，普通处理、普通截图和第二份历史记录均不出现。
4. 切回普通模式：画布节点、草稿、发布版本和视口仍存在，下一次快捷键恢复普通
   流程。

每条路径记录可见结果和日志中的一次启动记录；出现跨模式第二次启动即验收失败。

- [ ] **Step 5: 验证中文文字和窗口状态**

在 100% 与 150% Windows 缩放下分别检查普通窗口和最大化窗口：

- “当前执行”“普通模式”“画布模式”完整显示。
- “编辑画布”“返回设置”“发布流程”完整显示。
- 页头在窄窗口中不遮挡功能名和快捷键。
- footer 在节点数、连线数和较长发布版本文字下不覆盖按钮。

如果任何文字裁切，使用 `QFontMetrics::horizontalAdvance()` 与布局最小宽度修正，
然后重新运行 `function_command_page_tests` 和
`function_canvas_editor_tests`。不能仅凭 release 编译成功通过此门。

- [ ] **Step 6: 检查任务范围和最终差异**

```powershell
git diff --check
git status --short
rg -n `
  'functionFlowEnabledSettingsKey|setFlowEnabled|只.*回退|fallsBackToClassic' `
  vocekit/src vocekit/tests
```

Expected:

- `git diff --check` 无错误。
- 没有新增任务外路径。
- 源码与测试中不存在旧启用事件、停用按钮或经典回退契约。
- 设计规格中的十条验收标准均能指向本计划的测试或真实路径验证。

- [ ] **Step 7: 提交必要的验收修正**

如果 Step 1-6 发现并修正了测试清单、布局最小宽度或错误文字，使用明确路径暂存：

```powershell
git add -p -- `
  vocekit/src/ui/function_command_page.h `
  vocekit/src/ui/function_command_page.cpp `
  vocekit/src/ui/function_canvas_editor.h `
  vocekit/src/ui/function_canvas_editor.cpp `
  vocekit/tests/ui/function_command_page_tests.cpp `
  vocekit/tests/ui/function_canvas_editor_tests.cpp
git diff --cached --check
git commit -m "test: verify isolated function execution modes"
```

如果没有产生修正，不创建空提交。

---

## 规格覆盖自检

- 唯一模式字段、旧配置迁移、未知值保留：Task 1。
- 普通设置与画布设置互不覆盖：Task 2。
- 发布不自动切模式、切换失败不留半状态：Task 3。
- 画布发布损坏、缓存缺失和入口缺失不回退：Task 4、Task 5。
- 主快捷键、按住说话释放、独立截图快捷键和截图悬浮入口互斥：Task 5、Task 6。
- 设置页和画布页显示同一模式，导航与模式切换独立：Task 7。
- 模式事件刷新计划、快捷键和当前页面，草稿事件保持窄刷新：Task 8。
- 冻结中的运行不受模式切换影响：现有 execution controller 冻结计划测试在
  Task 9 全量回归中保留；本功能不修改运行中 `m_plan`。
- 中文文字裁切与真实四路径验收：Task 9。

计划中没有把 `flow.enabled` 作为运行判断条件；它只由
`normalizeFunctionSettings()` 和 JSON 写出同步为兼容镜像。
