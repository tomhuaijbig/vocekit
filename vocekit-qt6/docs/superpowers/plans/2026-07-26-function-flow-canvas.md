# 功能流程画布 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

> **Revision V4（2026-07-27）：** 本版在 V3 基础上再次按当前源码压力测试，补齐三种触发入口的显式分流、按住说话释放路由、运行事件观察契约、草稿只读分析、语音历史元数据、历史目录冻结、历史刷新事件和两类可编辑结果窗的回写生命周期；修正唯一 Output 表述、`OcrResult::elapsedMs` 接口名、语义数值不得被规范化静默钳制，以及既有测试工程漏接问题。第一版只支持 `waitAll`，不支持 `eachInput` 或节点并发。

> **Revision V5（2026-07-28）：** 设置页滚动区与画布固定工作区彻底分离。画布不显示内部滚动条，普通滚轮不移动页面或画布，空白处按住鼠标左键拖动平移，`Ctrl + 滚轮` 缩放；节点、端口和连线继续使用左键编辑。

> **Revision V6（2026-07-28）：** 返回设置前同步保存草稿和视口，失败则留在画布；功能 ID 仅在编辑器打开成功后提交；草稿和视口事件使用窄刷新，不能重建设置表单。补充真实页面所有权/反复刷新测试、缩放标签同步和拖放对象生命周期检查。

> **Revision V7（2026-07-28）：** 内置功能导航和旧历史记录的内置功能名仅显示中文；节点卡片的端口、输入内容角色、截图识别语言、超时单位和结果按钮动作全部使用中文显示。新放置的结果小框只继承流程支持的动作。持久化和执行仍使用 `text_in`、`source` 等稳定 ID，旧草稿和旧历史文件无需迁移。

**Goal:** 把当前空白 `FunctionCanvasView` 落地为可放置、连接、配置、保存、发布并真实执行的节点流程画布，同时让未发布、停用或结构损坏的画布继续安全使用现有经典功能。

**Architecture:** 画布分为“持久化图模型、端口与校验、发布服务、编译与纯调度、一次运行控制器、真实能力适配器、编辑器 UI”七个边界。编辑器只修改草稿；发布服务在一个事务中校验并复制语义图；一次运行开始后冻结已发布版本、触发入口、目标窗口和取消令牌。第一版虽然允许 DAG 分支和汇合，但节点始终按稳定顺序串行执行，以兼容当前单录音、单截图、单模型生命周期和外部写入约束。

**Tech Stack:** Qt 5.9 Widgets、C++11、`QGraphicsView/QGraphicsScene`、`QUndoStack`、qmake、MinGW 5.3 32 位、QtTest、现有 `AppSettingsStore`、`ApplicationEvents`、`CancellationToken/ExecutionId`、语音/OCR/模型/结果输出和历史服务。

---

## 零、实施前固定约束

1. 当前工作树可能包含用户尚未提交的修改。实施时只修改任务列出的文件，不使用通配符暂存，不清理、不重置、不覆盖无关改动。
2. 开始前保存 `git status --short`基线；停靠点只对本任务 Files小节列出的路径
   运行 scoped `git diff --check`，并对新增未跟踪文件执行显式尾随空白/UTF-8检查。
   结束时与基线比较，出现任务外新路径立即停止核对。除非用户明确要求，否则不
   执行 `git add`、`git commit`、推送或创建分支。
3. 当前源码和 `vocekit.pro` 优先于旧文档。新增文件必须同时进入主工程和对应测试工程。
4. 图模型、调度器和编译器不得依赖 QWidget、网络、文件或具体 Provider。
5. UI 只发送编辑意图；UI、`HubWindow` 和 `VoiceController` 不得直接实现图调度、JSON、OCR、HTTP、WebSocket或历史持久化。
6. 草稿变化不能重新注册快捷键、重载运行时控制器或改变正在执行的流程。
7. 一次运行只读取启动瞬间冻结的 `published`、模型/提示词解析结果和目标窗口。运行中编辑草稿或再次发布不改变当前运行。

实施会话从仓库根目录先运行：

```powershell
$env:PATH="D:\QQQQQT0001\Tools\mingw530_32\bin;D:\QQQQQT0001\5.9\mingw53_32\bin;$env:PATH"
qmake -v
mingw32-make --version
git status --short
```

两个工具路径任一不存在就停止，不使用其它 Qt/MinGW版本混编。把开始时的
`git status --short`输出保留为本次改动基线。

本计划中的“运行专项测试”不是只编译。除明确要求验证编译失败的红灯步骤外，
每个测试工程都必须在构建后执行生成的 exe，并检查退出码。实施会话可先定义：

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

不得把 qmake/make 成功写成“测试通过”；必须看到 QtTest 的 `0 failed`和进程
退出码 0。新增 `.pro`还必须有唯一 `TARGET`，以便
`scripts/run-all-tests.ps1`严格发现并执行。

## 一、第一版产品边界

### 1.1 第一版必须实现

1. 画布首次打开为空白，不自动放置任何节点。
2. 点击“放置”打开节点库，可以拖入或点击放置节点。
3. 节点可拖动；一次拖动手势只产生一个撤销命令和一次防抖保存。
4. 从输出端口拖到输入端口建立有方向的连线。
5. 点击节点后在画布右侧显示对应设置，不弹出独立编辑窗口。
6. 支持以下节点：
   - 内容来源：语音采集、选中文字、截图识别。
   - 数据整理：输入节点、输出节点。
   - 内容处理：调用大模型。
   - 结果动作：结果小框、截图对照窗、自动写入。
7. 翻译、润色和总结由“大模型节点 + 提示词”完成，不提供重复专用节点。
8. 支持多个 Input、Model和结果动作节点；编辑草稿可暂存多个 Output，但每个
   可发布版本必须恰好启用一个 Output。
9. 支持一对多分发和多对一汇合。
10. 语音、选中文字和截图来源每种最多放置一个；同一来源可连接多个 Input节点。
11. 第一版模型统一使用 `waitAll`：所有前驱输入终结后，检查必需输入，再按角色和顺序组装一次请求。
12. 图可以分支和汇合，但运行时一次只执行一个节点；不并发录音、截图、模型或结果动作。
13. `Ctrl + 鼠标滚轮`缩放；普通滚轮不移动页面或画布；空白处按住鼠标左键拖动平移；支持适应窗口、撤销和重做。
14. 修改后 500 毫秒尾沿防抖保存草稿；关闭页面和退出程序前冲刷待保存草稿。
15. 只有结构、触发入口和节点配置全部合法时才能发布。
16. 主快捷键、独立截图快捷键和截图悬浮入口分别运行编译出的已发布触发画像。
17. 没有可用于当前触发入口的已发布图时，运行经典逻辑。
18. 已发布图开始执行后，任何失败、取消或依赖缺失都不再回退经典逻辑。
19. 运行支持取消、节点状态、错误编号、逐节点耗时和一次运行历史。
20. 最多允许一个 `AutoWrite` 节点，所有结果动作按连线顺序串行执行。
21. 一次流程只写一条历史记录，不因多个结果动作重复保存。

### 1.2 第一版明确不实现

- 循环、条件分支、脚本节点。
- `eachInput`、节点重复发射和流式集合。
- 节点并发、并行模型、并行图片或文件处理。
- 用户编写 C++、JavaScript 或 PowerShell 节点。
- 第三方插件动态加载。
- 云端同步画布。
- 流程结果小框中的“重新生成、换模型、继续追问”。第一版流程结果小框只保留复制、写入、替换、加入词库、展开和关闭等不重新执行图的动作。
- 任意麦克风设备选择；第一版继续使用系统默认麦克风。
- 选中文字的富文本格式传递；第一版只传文字。
- 识别语言自由编辑；第一版内部仍使用稳定语言集合 `zh-Hans` 和 `en`，界面只显示“简体中文 / 英文”。

## 二、用户看到的界面

### 2.1 画布主体

`FunctionCommandPage` 的“画布”按钮继续原地切换，不创建新窗口。画布模式包含：

- 左上角浮动工具栏：放置、撤销、重做、缩小、比例、放大、适应窗口。
- 中间：无可见滚动条、可用空白左键拖动平移并用 `Ctrl + 滚轮` 缩放的网格画布。
- 右侧：仅在选中节点时滑出的节点设置栏。
- 底部：节点数量、连线数量、草稿状态、发布版本、当前校验状态。
- 发布区：“应用流程”“停用流程”，以及当前主快捷键/截图快捷键/截图悬浮入口
  在配置上是否具备合法执行画像。Windows实际注册失败属于运行环境状态，必须
  另行显示/记录，不能把“发布校验通过”误写成“系统热键已注册”。

点击空白区域取消选中并关闭右侧设置栏。

### 2.2 节点视觉

每个节点显示：

- 节点类型图标。
- 节点名称。
- 一行配置摘要。
- 固定定义的输入和输出端口；界面显示“文字输入、文字输出、动作输入、动作输出”，内部端口 ID 不变。
- 禁用、等待、运行中、取消中、完成、跳过、失败、阻塞、已取消状态。

节点类型使用稳定颜色：

- 内容来源：蓝色。
- 输入和数据传送：绿色。
- 大模型处理：橙色。
- 输出：紫色。
- 结果动作：灰色。
- 错误：红色。

节点本体不展开长表单，避免画布重新变成设置页。

### 2.3 第一版真实可用的节点设置

每种节点的 Inspector顶部都有“节点名称”和“启用此节点”两个通用字段；修改均
进入撤销栈。名称只影响编辑器显示，enabled影响发布编译。

| 节点 | 第一版设置 |
|---|---|
| 语音采集 | 系统默认麦克风（只读）、语音服务、录音方式、采集顺序、倒计时、提示音、长录音、网络策略 |
| 选中文字 | 普通读取/继承全局强力选中、采集顺序 |
| 截图识别 | 文字识别引擎、超时、截图入口、独立截图快捷键、采集顺序、网络策略；语言只读显示“简体中文 / 英文” |
| 输入节点 | 内容角色（常用角色显示中文、内部 ID 不变）、模型消息顺序、是否必需 |
| 调用大模型 | 模型、提示词、流式输出、网络策略；触发方式固定显示“等待全部输入” |
| 输出节点 | 空结果策略、已连接结果动作的执行顺序；排序实际修改出边 `order` |
| 结果小框 | 模板、显示时间、透明度、允许的按钮顺序；动作名称显示中文、内部 ID 不变 |
| 截图对照窗 | 显示时间、透明度 |
| 自动写入 | 插入/替换、失败时是否转结果小框 |

“目标节点”和“目标结果动作”不属于设置项。上下游关系只由画布连线决定。

流程 ResultPopup 的 `resultActions`只接受
`expand`、`vocabulary`、`copy`、`write`和`replace`。
出现 `regenerate`、`retryModel`或`followUp`时发布失败
`flow_popup_action_unsupported`。
新放置节点会过滤经典设置中不受流程支持的动作并保留受支持动作的原有顺序；
旧草稿中的已知动作 ID 仍以中文显示，不能把内部英文直接暴露给用户。

### 2.4 流式输出限制

只有同时满足以下条件时，模型节点才能启用流式输出：

1. 当前触发画像内只有一个模型节点。
2. 模型只进入一个输出节点。
3. 输出节点只连接一个结果小框。
4. 不连接自动写入或截图对照窗。

不满足条件时发布校验返回 `flow_stream_topology_unsupported`，而不是运行时静默改变行为。

### 2.5 唯一最终输出

第一版每张可发布图必须恰好有一个启用的 Output，且它至少连接一个结果动作。
中间节点仍可分支和汇合；多个结果目的地通过这个 Output 的多条 `action_out`
边表达。零个或多个启用 Output返回 `flow_output_count`。这样结果展示、自动
写入和历史记录始终共享一个明确的最终值。

## 三、领域模型与执行契约

### 3.1 节点类型和固定端口

```cpp
enum class FunctionFlowNodeType
{
    VoiceSource,
    SelectionSource,
    ScreenshotSource,
    Input,
    Model,
    Output,
    ResultPopup,
    ScreenshotPanel,
    AutoWrite
};

enum class FunctionFlowPortDirection
{
    Input,
    Output
};

enum class FunctionFlowPortCardinality
{
    One,
    Many
};

struct FunctionFlowPortSpec
{
    QString id;
    FunctionFlowPortDirection direction =
        FunctionFlowPortDirection::Input;
    FunctionFlowPortCardinality cardinality =
        FunctionFlowPortCardinality::One;
    bool connectionRequired = false;
};
```

端口不作为任意数组持久化，由节点类型注册表固定返回：

```cpp
QVector<FunctionFlowPortSpec> functionFlowPortSpecs(
    FunctionFlowNodeType type
);
bool hasFunctionFlowPort(
    FunctionFlowNodeType type,
    const QString &portId,
    FunctionFlowPortDirection direction
);
bool isFunctionFlowConnectionAllowed(
    FunctionFlowNodeType fromType,
    const QString &fromPortId,
    FunctionFlowNodeType toType,
    const QString &toPortId
);
```

固定端口 ID：

| 节点 | 输入端口 | 输出端口 |
|---|---|---|
| VoiceSource | 无 | `text_out` |
| SelectionSource | 无 | `text_out` |
| ScreenshotSource | 无 | `text_out` |
| Input | `text_in`，允许多条来源入边 | `text_out`，允许多条出边 |
| Model | `text_in`，允许多条入边 | `text_out`，允许多条出边 |
| Output | `text_in`，只允许一条入边 | `action_out`，允许多条出边 |
| ResultPopup | `action_in` | 无 |
| ScreenshotPanel | `action_in` | 无 |
| AutoWrite | `action_in` | 无 |

Input、Model、Output和三个结果动作的输入端口都设置
`connectionRequired=true`；“optional Input”只表示上游本次没有运行值时可跳过，
不表示可以不连线。Output 的 `action_out`另外要求至少一条出边。

连线必须从输出端口连接到输入端口。不能通过持久化自定义端口绕过注册表。
第一版只允许以下节点类型组合：

| 上游 | 下游 | 用途 |
|---|---|---|
| VoiceSource / SelectionSource / ScreenshotSource | Input | 给来源文字标注 role、sequence 和 required |
| Input | Model | 组装一次模型请求 |
| Input | Output | 无模型直通 |
| Model | Input | 给上一模型结果重新标注角色后进入下一模型 |
| Model | Output | 输出最终模型结果 |
| Output | ResultPopup / ScreenshotPanel / AutoWrite | 按顺序执行结果动作 |

其它组合返回 `flow_edge_type_unsupported`。因此两个模型串联必须显式写成
`Model A -> Input -> Model B`，不能直接 `Model -> Model`；每个 Model 的所有
直接前驱都必须是 Input，编译器才能稳定生成 role/sequence 输入绑定。

### 3.2 显式节点配置

```cpp
struct FunctionFlowRecordingConfig
{
    QString triggerMode = QStringLiteral("toggle");
    bool longRecordingEnabled = false;
    int segmentSeconds = 55;
    int maximumMinutes = 30;
    int countdownSeconds = 0;
    bool beepEnabled = false;
    QString beepPath;
};

struct FunctionFlowVoiceSourceConfig
{
    QString speechProviderId; // 空值表示继承全局
    FunctionFlowRecordingConfig recording;
    int acquisitionSequence = 0;
    QString networkPolicy = QStringLiteral("inherit");
};

struct FunctionFlowSelectionSourceConfig
{
    bool inheritStrongSelection = true;
    int acquisitionSequence = 0;
};

struct FunctionFlowScreenshotSourceConfig
{
    QString ocrEngineId = QStringLiteral("automatic");
    int timeoutMs = 45000;
    QString triggerMode = QStringLiteral("primary");
    QString separateShortcut;
    int acquisitionSequence = 0;
    QString networkPolicy = QStringLiteral("inherit");
};

struct FunctionFlowInputConfig
{
    QString role = QStringLiteral("source");
    int sequence = 0;
    bool required = true;
};

struct FunctionFlowModelConfig
{
    QString modelId;
    QString promptId;
    bool stream = false;
    QString networkPolicy = QStringLiteral("inherit");
};

struct FunctionFlowOutputConfig
{
    QString emptyResultPolicy = QStringLiteral("fail");
};

struct FunctionFlowResultPopupConfig
{
    QString resultTemplate = QStringLiteral("simple");
    QStringList resultActions = defaultFunctionFlowPopupActionIds();
    int displaySeconds = 0;
    int opacity = -1; // -1 表示继承全局
};

struct FunctionFlowScreenshotPanelConfig
{
    int displaySeconds = 0;
    int opacity = -1;
};

struct FunctionFlowAutoWriteConfig
{
    QString writeMode = QStringLiteral("insert");
    bool fallbackToPopup = true;
};

struct FunctionFlowNodeConfig
{
    FunctionFlowVoiceSourceConfig voice;
    FunctionFlowSelectionSourceConfig selection;
    FunctionFlowScreenshotSourceConfig screenshot;
    FunctionFlowInputConfig input;
    FunctionFlowModelConfig model;
    FunctionFlowOutputConfig output;
    FunctionFlowResultPopupConfig popup;
    FunctionFlowScreenshotPanelConfig screenshotPanel;
    FunctionFlowAutoWriteConfig autoWrite;
};
```

`FunctionFlowNodeConfig`虽然包含多个类型化成员，但校验、规范化和 JSON 只读取当前节点类型对应的成员。不得用无约束字符串字典替代这些运行字段。

流程小框使用独立动作注册表：

```cpp
QStringList supportedFunctionFlowPopupActionIds();
QStringList defaultFunctionFlowPopupActionIds();
bool isFunctionFlowPopupActionSupported(const QString &id);
```

三者只包含 `expand/vocabulary/copy/write/replace`。不得调用经典
`normalizeResultActionIds()`，因为它会为空列表补入
`regenerate/retryModel/followUp`。JSON读取保留不支持的原值交给发布校验报错，
编辑器只允许写入注册表中的值。

发布校验还固定以下值域，非法枚举返回 `flow_node_config_invalid`：

| 字段 | 值域 |
|---|---|
| recording.triggerMode | `toggle` / `hold` |
| segmentSeconds / maximumMinutes / countdownSeconds | 20–55 / 1–30 / 0–60 |
| acquisitionSequence / Input.sequence | 0–10,000 |
| networkPolicy | `inherit` / `direct` / `systemProxy` |
| screenshot.timeoutMs | 1,000–120,000 |
| screenshot.triggerMode | `primary` / `separate` / `launcher` / `separateAndLauncher` |
| output.emptyResultPolicy | `fail` / `skipActions` |
| popup.resultTemplate | `simple` / `detail` / `compare` / `outputOnly` |
| popup/screenshotPanel.displaySeconds | 0–600 |
| popup/screenshotPanel.opacity | `-1`或 20–100 |
| autoWrite.writeMode | `insert` / `replace` |

Model的 modelId/promptId、Voice显式 provider ID和所有需要的快捷键必须非空且通过
3.6/3.7 的引用与冲突校验。
`FunctionFlowEdge::order`同样必须在 0–10,000；越界返回
`flow_edge_order_invalid`，不能在规范化时钳制。新建边的“当前最大值 + 1”必须
先检查上限，避免外部 JSON中的 `INT_MAX`造成整数溢出。

### 3.3 图、编辑器状态和版本状态

```cpp
struct FunctionFlowNode
{
    QString id;
    FunctionFlowNodeType type = FunctionFlowNodeType::Input;
    QString title;
    QPointF position;
    bool enabled = true;
    FunctionFlowNodeConfig config;
    QJsonObject retainedValues;
};

struct FunctionFlowEdge
{
    QString id;
    QString fromNodeId;
    QString fromPortId;
    QString toNodeId;
    QString toPortId;
    int order = 0;
    QJsonObject retainedValues;
};

struct FunctionFlowEndpoint
{
    QString nodeId;
    QString portId;
};

struct FunctionFlowGraph
{
    int schemaVersion = 1;
    QVector<FunctionFlowNode> nodes;
    QVector<FunctionFlowEdge> edges;
    QJsonObject retainedValues;
};

struct FunctionFlowEditorState
{
    QPointF viewportCenter;
    qreal zoom = 1.0;
};

struct VersionedFunctionFlowGraph
{
    int revision = 0;
    int sourceDraftRevision = 0;
    QString graphHash;
    bool supported = true;
    QString unavailableCode;
    FunctionFlowGraph graph;
    QJsonObject retainedRaw;
};

struct FunctionFlowState
{
    bool enabled = false;
    VersionedFunctionFlowGraph draft;
    VersionedFunctionFlowGraph published;
    FunctionFlowEditorState editor;
};
```

约束：

- 视口和缩放属于 `editor`，不进入发布图哈希。
- 草稿 revision 每次成功保存增加 1。
- 发布 revision 只在成功发布时增加 1。
- `sourceDraftRevision`记录发布来源。
- `graphHash`使用规范化节点/连线稳定排序后的运行语义 JSON 计算完整 64 位十六
  进制 SHA-256；UI和日志可以只显示前 12 位，但缓存、比较和历史保存完整值。
- 规范 JSON只写已知运行字段，递归按 UTF-8键名排序对象，节点/边按稳定 ID排序，
  使用 `QJsonDocument::Compact`的 UTF-8字节；禁止依赖 QHash迭代顺序、区域设置
  或格式化空白。测试固定一个 golden hash，避免“同图同进程看似稳定、换构建后
  hash漂移”。
- 全新且从未保存/发布的 revision 0状态允许 graphHash为空；第一次成功保存草稿
  或发布后必须写入 64位完整 hash。不得把 SHA-256空字节串的固定值冒充“空图
  hash”，空图也必须对其规范化语义 JSON真实计算。
- graphHash是派生校验值，不能盲信磁盘字符串。加载 supported draft时始终重算
  并以内存中的实际 hash为准，外部修改后的草稿仍须重新发布；加载 revision>0
  或 enabled的 published时，缺失、格式错误或与重算值不一致都把 published标为
  `supported=false/flow_published_hash_mismatch`并保留 retainedRaw，绝不执行
  未经发布事务确认的图。revision 0且从未发布的空 published仍允许空 hash。
- 节点 title/position、editor viewport/zoom和 retainedValues/retainedRaw 不属于
  运行语义，不进入 graphHash；节点 type/enabled/config、端口、连线和 edge.order
  必须进入。
- `Input.text_in`从单来源放宽为多来源时保留 schema v1规范 JSON中的
  `cardinality=one`兼容标记，避免仅因编辑规则放宽而使既有发布图失效；新增的
  每条来源边仍完整进入 graphHash，因此单来源图与多来源图不会发生语义哈希碰撞。
- `retainedValues/retainedRaw`只用于兼容保存，不参与运行配置。
- 不支持的未来 schema 设置 `supported=false`，保留原始 JSON，不能执行、不能覆盖。
- `unavailableCode`在 supported=true时为空；未来 schema使用
  `flow_schema_newer`，当前 schema中出现无法表示的未来节点类型使用
  `flow_node_type_unsupported`，损坏 JSON使用 `flow_json_invalid`，published
  hash错误使用 `flow_published_hash_mismatch`，UI和服务原样报告。
- published.supported=false时始终允许停用并拒绝 setEnabled(true)，保留
  retainedRaw并走经典入口。未来 schema/未知节点拒绝当前版本 publish；只有
  3.6规定的当前版本损坏/hash不匹配可经明确修复确认覆盖。
- 节点 Running/Succeeded/Failed等状态是 scene 的瞬态 overlay，不进入
  FunctionFlowGraph、JSON或撤销栈。运行事件携带 publishedHash；只有当前草稿
  运行语义哈希与它相同且 nodeId/type匹配时才给节点着色，否则只在底部显示
  带具体 revision 号的“发布版运行中”状态。

### 3.4 ID、规范化和规模限制

- 新节点和连线 ID 使用 `QUuid::createUuid().toString(QUuid::WithoutBraces)`。
- 节点 ID、连线 ID不能为空或重复。
- `normalizeFunctionFlowGraph()`只允许：
  - 修剪 ID、标题和配置字符串。
  - 把非有限的编辑器坐标/缩放替换为安全默认值，并把 zoom约束到
    0.35–3.0。
  - 按稳定规则排序节点和连线。
  - 补充不改变拓扑的默认值。
- 规范化不得删除节点、删除连线、合并重复 ID或替换未知节点。
- 运行语义数值（录音时长、超时、顺序、显示时间和透明度等）不得在读取、
  保存或发布前被静默钳制；越界值必须原样到达发布校验并返回
  `flow_node_config_invalid`。Inspector可通过控件范围阻止新建非法值，但不能
  借此掩盖外部 JSON中的非法值。
- 第一版最多 128 个节点、256 条连线。
- 节点标题最多 80 个字符，角色最多 40 个字符。
- Input角色不得包含换行、控制字符或 `[`/`]`，否则返回
  `flow_input_role_invalid`，避免破坏固定消息分段格式。
- 坐标和缩放必须是有限数；zoom 范围为 0.35–3.0。

### 3.5 持久化所有权

内存中 `FunctionSettings`包含：

```cpp
FunctionFlowState flow;
```

`AppSettingsData`另外增加：

```cpp
QJsonObject retainedOrphanFunctionFlows;
```

它只保存顶层 `functionFlows`中无法匹配当前功能 ID 的原始对象。已知功能仍以
`FunctionSettings::flow`为唯一内存所有者；写回时把已知流程和孤儿流程合并，
不得让普通设置保存删除未知功能的数据。

JSON 中统一写到顶层 `functionFlows`，以功能 ID 为键。两者是同一份数据的内存和存储表示，不再建立第二个运行时流程 map。

```json
{
  "functionFlows": {
    "custom_1": {
      "enabled": false,
      "editor": {
        "viewport": { "x": 900, "y": 520 },
        "zoom": 1.0
      },
      "draft": {
        "revision": 0,
        "sourceDraftRevision": 0,
        "graphHash": "",
        "graph": {
          "schemaVersion": 1,
          "nodes": [],
          "edges": []
        }
      },
      "published": {
        "revision": 0,
        "sourceDraftRevision": 0,
        "graphHash": "",
        "graph": {
          "schemaVersion": 1,
          "nodes": [],
          "edges": []
        }
      }
    }
  }
}
```

存储规则：

1. 旧 JSON 没有 `functionFlows`时生成停用空流程，不改经典设置。
2. 草稿损坏时只标记草稿不可用，已发布图保持可执行。
3. 已发布图损坏或未来 schema 不可解析时保留原 JSON，并让对应触发入口回退经典模式。
   published hash缺失/格式错误/与语义图不一致时同样保留原 JSON并以
   `flow_published_hash_mismatch`回退；运行缓存还要在编译前独立复核一次。
4. 已知 schema 中的未知扩展字段往返保存。
   当前 schema遇到未知节点 type时不能强制转换成 Input或删除节点；把该版本图
   标为 `supported=false/flow_node_type_unsupported`并保留完整 retainedRaw。
5. 加载时发现孤儿功能 ID，原样保留但不展示、不执行。
6. 自定义功能创建时在同一事务中创建停用空流程；删除时在同一事务中删除功能、
   functionOrder项和对应流程。
7. 图中只保存模型 ID、提示词 ID和服务 ID；不保存 API Key、录音、截图、选中文字、模型输入或输出。
8. `functionFlows`由专用解析器消费，不能同时残留在
   `AppSettingsData::retainedRootValues`；序列化时只生成一个顶层
   `functionFlows`对象。

### 3.6 发布事务

发布不能接受调用方构造的任意 `published` 图。使用：

```cpp
struct FunctionFlowValidationIssue
{
    QString code;
    QString nodeId;
    QString edgeId;
    QString message;
};

struct FunctionFlowValidationResult
{
    bool ok = false;
    QStringList issueCodes;
    QVector<FunctionFlowValidationIssue> issues;
};

struct FunctionFlowReferenceCatalog
{
    QStringList modelIds;
    QStringList promptIds;
    QStringList speechProviderIds;
    QStringList ocrEngineIds;
    QString defaultSpeechProviderId;
    QString defaultOcrEngineId;
};

struct FunctionFlowValidationContext
{
    FunctionFlowReferenceCatalog references;
    QString functionId;
    QString mainShortcut;
    QMap<QString, QString> occupiedShortcutOwners;
};

class FunctionFlowValidator
{
public:
    static FunctionFlowValidationResult validateForPublish(
        const FunctionFlowGraph &graph,
        const FunctionFlowValidationContext &context
    );
};

struct FunctionFlowPublishResult
{
    bool ok = false;
    int publishedRevision = 0;
    FunctionFlowValidationResult validation;
    OperationError error;
};

enum class FunctionFlowTrigger;

struct FunctionFlowDraftAnalysis
{
    QString graphHash;
    FunctionFlowValidationResult validation;
    QMap<FunctionFlowTrigger, bool> triggerAvailability;
};

class FunctionFlowPublicationService
{
public:
    bool readState(
        const QString &functionId,
        FunctionFlowState *state,
        OperationError *error
    ) const;

    FunctionFlowDraftAnalysis analyzeDraft(
        const QString &functionId,
        const FunctionFlowGraph &draft
    ) const;

    bool addCustomFunction(
        const FunctionSettings &functionWithoutFlow,
        OperationError *error
    );

    bool updateDraft(
        const QString &functionId,
        int expectedRevision,
        const FunctionFlowGraph &draft,
        int *savedRevision,
        OperationError *error
    );

    bool updateEditorState(
        const QString &functionId,
        const FunctionFlowEditorState &editor,
        OperationError *error
    );

    FunctionFlowPublishResult publish(
        const QString &functionId,
        int expectedDraftRevision,
        bool replaceCorruptPublished = false
    );

    bool setEnabled(
        const QString &functionId,
        bool enabled,
        OperationError *error
    );

    bool removeCustomFunction(
        const QString &functionId,
        OperationError *error
    );
};

struct FunctionFlowSettingsAccess
{
    std::function<bool(
        const QString &,
        FunctionFlowState *,
        OperationError *
    )> readState;
    std::function<FunctionFlowDraftAnalysis(
        const QString &,
        const FunctionFlowGraph &
    )> analyzeDraft;
    std::function<bool(
        const FunctionSettings &,
        OperationError *
    )> addCustomFunction;
    std::function<bool(
        const QString &,
        int,
        const FunctionFlowGraph &,
        int *,
        OperationError *
    )> updateDraft;
    std::function<bool(
        const QString &,
        const FunctionFlowEditorState &,
        OperationError *
    )> updateEditorState;
    std::function<FunctionFlowPublishResult(
        const QString &,
        int,
        bool
    )> publish;
    std::function<bool(
        const QString &,
        bool,
        OperationError *
    )> setEnabled;
    std::function<bool(
        const QString &,
        OperationError *
    )> removeCustomFunction;
};
```

`FunctionFlowPublishResult/FunctionFlowDraftAnalysis`放在共享的
`src/domain/function_flow_publication_types.h`；
`FunctionFlowSettingsAccess`是 UI 侧的头文件级访问契约，放在
`src/ui/function_flow_settings_access.h`。它不放进 publication service头文件，
也不包含该具体控制器头，避免 UI 为了几个回调依赖具体服务。应用组装层把
service方法适配成该结构。
`analyzeDraft()`只用调用瞬间的引用/快捷键目录执行校验和编译分析，不保存、
不增加 revision、不发设置事件；编辑器的“可发布”和三个触发入口状态必须来自
它，不能用 UI 自己拼一套缩水校验。

发布服务必须在一个事务中：

1. 读取当前草稿。
2. 检查 `expectedDraftRevision`，不一致返回 `flow_draft_stale`。
3. 当前 draft不支持时返回其 unavailableCode。
4. 当前 published为 `flow_schema_newer`或
   `flow_node_type_unsupported`时始终拒绝覆盖 retainedRaw。当前版本可判定为
   损坏的 `flow_json_invalid/flow_published_hash_mismatch`默认返回
   `flow_published_repair_confirmation_required`；只有 UI向用户展示将被替换的
   原因并得到明确确认后，才以 `replaceCorruptPublished=true`继续。确认不绕过
   draft校验、乐观 revision或原子保存，任何失败仍保留 retainedRaw。
5. 用当前 PromptRuntimeSnapshot、稳定 provider/OCR ID目录、主快捷键和全局
   快捷键占用快照校验引用
   与所有触发画像。
6. 编译执行计划并计算稳定哈希；`compileResult.ok=false`时不保存。
7. 若哈希与现有 supported published相同且已启用，返回现有 revision，不写文件、
   不发事件；相同但停用时只执行重新启用事务。
8. 哈希变化时把当前草稿语义图复制到 published。
9. 增加 published revision，记录 sourceDraftRevision。
10. 在同一快照中设置 `enabled=true`；“应用流程”不执行发布、启用两个独立保存。
11. 原子保存；保存失败恢复内存快照。
12. 新 published成功后才发布 `functionFlowPublishedSettingsKey()`；相同 hash仅
    重新启用时发布 `functionFlowEnabledSettingsKey()`。

编辑器用 draft.graphHash与published.graphHash判断“发布内容是否过期”，不用
sourceDraftRevision直接比较；仅移动节点、改标题或视口不会把流程标成运行语义
过期。

`setEnabled(false)`始终允许并原子停用；`setEnabled(true)`只用于重新启用已有
published，必须再次校验 schema、引用和编译结果，没有合法发布版时返回
`flow_published_unavailable`。目标 enabled值与当前相同时不重复保存或发事件；
任何失败都保持原 enabled值。

`FunctionFlowPublicationService`通过注入的设置快照读取和
`AppSettingsStore::replaceAndSave()`回调及 validation context provider完成上述方法，
不持有 UI 指针。
validation context provider必须按当前真实解析入口构建：

- modelIds取 `modelOptions()`中稳定的 `ModelOption.id`，不能用展示标题。
- promptIds取 `promptRuntimeTargets(PromptRuntimeSnapshot)`的所有可解析目标，
  包括内置功能、功能自带提示词和 PromptLibrary条目，不能只枚举
  `PromptLibraryStore::items()`。
- speechProviderIds取新增的纯目录
  `supportedSpeechProviderIds()`（当前由
  `speechProviderBaidu()/speechProviderXfyun()/speechProviderCustom()`组成）；
  OCR取 `supportedOcrEngineIds()`中的
  `automatic/rapid/windows/customCloud/vision`。当前应用运行模块没有长期持有
  ProviderRegistry，不为发布校验虚构第二套全局 Provider生命周期。
- 内置功能主快捷键取 `applicationHotkeys`覆盖值并回退 `hotkeyDefs()`默认值；
  自定义功能取 `FunctionSettings.shortcut`。所有值使用
  `hotkey_parser`的同一规范化结果比较，不能比较原始显示字符串。

空的 `speechProviderId`先解析为目录中的 defaultSpeechProviderId；
`ocrEngineId=automatic`解析为当前默认/自动选择能力后再验证，不能因为“继承”
或“自动”而跳过引用检查。
`vocekit_application_runtime.cpp`用只捕获服务生命期内对象的 lambda 组装
`FunctionFlowSettingsAccess`，再沿 `HubWindowAccess`传入各页面；UI 不保存
AppSettingsStore 或服务裸指针。

为防止 HubSettingsState 的旧快照覆盖新草稿，普通设置保存必须使用：

```cpp
bool replaceNonFlowSettingsAndSave(
    const AppSettingsData &editedSettings,
    OperationError *error
);
```

`HubWindowAccess`增加：

```cpp
std::function<bool(
    const AppSettingsData &,
    OperationError *
)> applyNonFlowAndSave;
```

`HubSettingsState::save()`和`replaceAndSave()`增加默认值为 nullptr 的
`OperationError *`参数。旧的单参数 applyAndSave只作为测试/迁移
fallback，生产 runtime全部接到非流程保存接口，因此 UI可以识别
`settings_function_set_stale`而不是只得到模糊的 false。发布服务是生产代码中
唯一可提交包含流程结构变更的完整 `replaceAndSave()`调用方。

该接口按功能 ID保留 AppSettingsStore 当前最新的 `FunctionFlowState`，
只合并非流程设置。它要求 editedSettings 与当前快照具有完全相同的功能 ID
集合；集合不同返回 `settings_function_set_stale`且不保存，防止旧快照重新加入
已删除功能或漏掉刚创建的功能。草稿、发布、停用以及自定义功能创建/删除只能走
`FunctionFlowPublicationService`。创建事务强制写入全新的停用空流程，不接受
调用方携带的 published 图；删除事务同时删除功能、functionOrder 和对应流程。

流程服务成功保存并发出设置事件后，
`HubRefreshCoordinatorBundle`只从 AppSettingsStore 重读该 functionId 的最新
`FunctionFlowState`，并通过以下窄方法同步 HubSettingsState：

```cpp
void replaceFunctionFlowState(
    const QString &functionId,
    const FunctionFlowState &state
);
```

编辑器分别保存 `baseDraftRevision`和 `observedRemoteRevision`。本地不 dirty
时，draft事件可装载远端图并同时更新两者；本地 dirty且远端 revision更高时，
不得替换图、清空撤销栈或抬高 baseDraftRevision，而是进入 conflict状态、暂停
自动保存和发布，只更新 observedRemoteRevision及状态栏。用户明确确认“重新
加载远端草稿”后才丢弃本地工作副本。这样下一次保存仍会用旧 base revision
触发乐观锁冲突，不可能静默覆盖远端。

设置事件 key 由 `application_events.h/.cpp`集中定义，生产代码和测试共用：

```cpp
QString functionDefinitionsSettingsKey();
QString functionFlowDraftSettingsKey();
QString functionFlowEditorStateSettingsKey();
QString functionFlowPublishedSettingsKey();
QString functionFlowEnabledSettingsKey();
```

`addCustomFunction/removeCustomFunction`发布 `functionDefinitionsSettingsKey()`；
`updateDraft/updateEditorState/setEnabled`分别发布 draft/editor/enabled key。
`publish`写入新 published时发布 published key（该事件已包含同事务启用），语义
hash相同但仅从停用恢复时只发布 enabled key。所有事件都只能在
`QSaveFile::commit()`成功后发布，保存失败不得刷新 UI、运行时或快捷键。

`ApplicationEvents`当前在同线程同步发信号，因此编辑器调用 `updateDraft()`前
必须进入 local-save guard。该调用尚未返回时收到同 functionId的 draft事件，
只能暂存为 deferred local event，不能立刻按“远端 revision更高”进入冲突。
调用成功后先用返回的 savedRevision更新 base、清除对应 dirty代次，再合并暂存
事件；只有事件 revision不是本次 `expectedRevision + 1`，或保存完成后又观察到
更高 revision，才走真正的远端冲突规则。调用失败不应存在成功事件；若错误实现
仍发出事件，保留 dirty并按异常冲突处理。不得通过全局屏蔽事件来掩盖其它
functionId的合法更新。

`updateEditorState()`始终从 store 当前快照开始，只替换 viewport/zoom，不接收
调用方图，不增加 draft或published revision；不同窗口的纯视口更新采用
last-write-wins。它不能用于绕过草稿乐观锁。
对称地，`updateDraft()`始终从 store当前快照开始，只替换 draft并保留当前最新
editor/published；它不接收调用方 editor。图保存和视口保存使用独立防抖与错误
状态，任一调用都不能携带另一边可能过期的快照。

### 3.7 触发画像

```cpp
enum class FunctionFlowTrigger
{
    MainHotkey,
    ScreenshotHotkey,
    ScreenshotLauncher
};

using FunctionFlowTargetWindowHandle = void *;

struct FunctionFlowTriggerPlan
{
    FunctionFlowTrigger trigger = FunctionFlowTrigger::MainHotkey;
    bool available = false;
    QStringList activeSourceNodeIds;
    QStringList acquisitionNodeIds;
    bool usesHoldToTalk = false;
};

enum class FunctionFlowStartOutcome;

struct FunctionFlowTriggerRequest
{
    QString functionId;
    FunctionFlowTrigger trigger = FunctionFlowTrigger::MainHotkey;
    FunctionFlowTargetWindowHandle targetWindow = nullptr;
    bool classicWorkflowBusy = false;
};
```

编译规则：

- `MainHotkey`激活 VoiceSource、SelectionSource，以及 `triggerMode=primary`的 ScreenshotSource。
- `ScreenshotHotkey`激活 `triggerMode=separate`或`separateAndLauncher`的 ScreenshotSource。
- `ScreenshotLauncher`激活 `triggerMode=launcher`或`separateAndLauncher`的 ScreenshotSource。
- 每个可用触发画像独立检查从激活来源到结果动作的可达性和必需输入。
- 不可由当前触发入口产生的输入必须标为 optional，否则该画像不能发布。
- 编译器还要按触发画像做静态 provenance传播：Source产生自己的来源类型，
  Input/Model/Output取所有前驱来源类型的并集。只要唯一 Output连接
  ScreenshotPanel，该图的每个可用触发画像都必须有一个激活的
  ScreenshotSource能沿数据边到达 Output；否则返回
  `flow_screenshot_context_missing`。不能只证明“整张图某处有截图节点”，因为
  MainHotkey和独立截图入口可能激活不同来源。
- 同理，Output连接 `writeMode=replace`的 AutoWrite时，每个可用触发画像都必须
  有 SelectionSource provenance到达 Output，否则返回
  `flow_replace_selection_context_missing`。静态 provenance只证明该入口可能
  取得选区；运行时选区为空仍必须安全失败，不能退化成插入。
- 有 MainHotkey画像时 function主快捷键必须存在；有 ScreenshotHotkey画像时
  ScreenshotSource.separateShortcut必须存在。两者先用现有快捷键规范化器解析，
  再检查彼此及 `occupiedShortcutOwners`，缺失或冲突分别返回
  `flow_trigger_shortcut_missing`或`flow_trigger_shortcut_conflict`。
- validation context构建时从 occupiedShortcutOwners排除当前 functionId 的主逻辑
  ID和 `screenshot:<functionId>`，再在图内单独比较 mainShortcut与
  separateShortcut，避免把合法的“替换自身旧注册”误报为冲突。
- `hold`语音节点必须唯一、必须是 MainHotkey 的第一个采集节点；第一版同一 MainHotkey画像不能再包含 primary截图来源。
- 草稿变化不更新热键画像；发布或停用后才更新。
- 热键运行快照按 MainHotkey、ScreenshotHotkey和ScreenshotLauncher三个入口
  分别选择：已启用 published有该入口画像时使用流程配置，否则保留该入口的
  经典 FunctionSettings配置。不能因为某一个流程画像可用就删除其它入口的经典
  注册。

命令入口必须显式区分三种 trigger，不能继续把 Launcher伪装成截图快捷键：

```cpp
struct FunctionCommandAccess
{
    // 保留现有回调，并新增：
    std::function<FunctionFlowStartOutcome(
        const FunctionFlowTriggerRequest &
    )> startPublishedFlow;
    std::function<bool(const QString &functionId)>
        releasePublishedFlowHold;
};

FunctionCommandOutcome handleScreenshotLauncherTrigger(
    const QString &functionId,
    FunctionFlowTargetWindowHandle rememberedTargetWindow
);
```

`handleHotkey()`在处理 hub/vocabulary等非功能命令后，先解析主功能或
`screenshot:<functionId>`逻辑 ID、立即取得目标窗口，再调用
`startPublishedFlow()`；该调用必须发生在通用 `processing()`/
`screenshotActive()`早退之前，否则再次按同一流程快捷键永远无法取消正在运行的
流程。start协调器先处理“同一非 hold trigger取消当前 flow”，再检查其它 flow或
经典录音/截图/模型是否忙。只有返回 `NotAvailable`才继续执行现有经典忙碌检查
和经典输入流程。

Launcher在显示菜单前持续保存最后一个有效的非 Vocekit前台窗口；用户选择功能
时把该句柄传给 `handleScreenshotLauncherTrigger()`，不得调用现有含糊的
`handleScreenshotTrigger()`。新 handler先把 rememberedTargetWindow写入本次
命令上下文；若流程返回 NotAvailable而走经典截图，必须调用
`startScreenshot(functionId, true)`沿用同一句柄，不能在 QMenu关闭后重新抓取
前台窗口。`handleHotkeyReleased()`先询问
`releasePublishedFlowHold(functionId)`，已消费时不再交给经典录音控制器；未
消费才保持现有释放路径。

### 3.8 运行时瞬态值

截图图片、OCR 块和录音元数据不进入图 JSON，只在一次运行中传递：

```cpp
struct FunctionFlowVoicePayload
{
    QString sourceAudioPath;
    QVector<RecordingSegment> segments;
    qint64 speechElapsedMs = -1;
    QString recordingTriggerMode;
    bool longRecording = false;
};

struct FunctionFlowScreenshotPayload
{
    QImage image;
    QVector<OcrTextBlock> blocks;
    QString recognizedText;
    OcrEngine engine = OcrEngine::Automatic;
    qint64 elapsedMs = -1;
    bool usedFallback = false;
    QRect rect;
};

struct FunctionFlowValue
{
    QString text;
    QString sourceNodeId;
    QString role;
    int sequence = 0;
    QSharedPointer<const FunctionFlowVoicePayload> voice;
    QSharedPointer<const FunctionFlowScreenshotPayload> screenshot;
};
```

Input、Model 和 Output节点必须把输入中的 voice/screenshot payload作为
provenance继续传递，模型只把非空 `text`组装进请求。语音 payload中的音频路径、
分段和识别耗时只供统一历史收尾使用；运行日志不得记录路径、音频、图片或完整
中间文字，截图 QImage/blocks永不持久化。

### 3.9 一次运行上下文与回调契约

```cpp
struct FunctionFlowResolvedNodeSettings
{
    QString modelId;
    QString systemPrompt;
    QString promptVersion;
    QString speechProviderId;
    QString ocrEngineId;
    QString effectiveNetworkPolicy;
    bool strongSelectionEnabled = false;
};

struct FunctionFlowResolvedDependencies
{
    QMap<QString, FunctionFlowResolvedNodeSettings> byNodeId;
    QString functionTitle;
    QString recordDirectory; // 启动时解析后的受管绝对目录
    int inheritedResultPopupOpacity = 100;
    int inheritedScreenshotPanelOpacity = 92;
    bool hasResultPopupGeometry = false;
    QRect resultPopupGeometry;
    bool hasScreenshotPanelGeometry = false;
    QRect screenshotPanelGeometry;
};

struct FunctionFlowRunContext
{
    ExecutionId runId;
    QString functionId;
    int publishedRevision = 0;
    QString publishedHash;
    FunctionFlowTrigger trigger = FunctionFlowTrigger::MainHotkey;
    FunctionFlowTargetWindowHandle targetWindow = nullptr;
    CancellationToken cancellation;
    QSharedPointer<const FunctionFlowResolvedDependencies> dependencies;
};

enum class FunctionFlowNodeState
{
    Pending,
    Ready,
    Running,
    Cancelling,
    Succeeded,
    Skipped,
    Failed,
    Blocked,
    Cancelled
};

enum class FunctionFlowStartOutcome
{
    Started,
    CancelledExisting,
    NotAvailable,
    Busy,
    TargetUnavailable,
    ConfigurationError
};

struct FunctionFlowNodeResult
{
    FunctionFlowNodeState state = FunctionFlowNodeState::Failed;
    QList<FunctionFlowValue> values;
    QList<FunctionFlowValue> historyObservations;
    OperationError error;
};

struct FunctionFlowResultActionRequest
{
    FunctionFlowValue output;
    QString canonicalInput;
    bool collectedSelection = false;
};

using FunctionFlowNodeCompletion =
    std::function<void(const FunctionFlowNodeResult &)>;

struct FunctionFlowNodeTrace
{
    QString nodeId;
    QString nodeType;
    QString state;
    qint64 elapsedMs = -1;
    QString errorCode;
    QString modelId;
    QString promptVersion;
};

struct FunctionFlowNodeExecutionEvent
{
    ExecutionId runId;
    QString functionId;
    int publishedRevision = 0;
    QString publishedHash;
    QString nodeId;
    FunctionFlowNodeType nodeType = FunctionFlowNodeType::Input;
    FunctionFlowNodeState state = FunctionFlowNodeState::Pending;
    qint64 elapsedMs = -1;
    QString errorCode;
};

Q_DECLARE_METATYPE(FunctionFlowNodeExecutionEvent)

struct FunctionFlowRunExecutionEvent
{
    ExecutionId runId;
    QString functionId;
    int publishedRevision = 0;
    QString publishedHash;
    bool running = false;
    bool cancelled = false;
    OperationError terminalError;
};

Q_DECLARE_METATYPE(FunctionFlowRunExecutionEvent)

struct FunctionFlowHistoryRequest
{
    ExecutionId runId;
    QString functionId;
    QString functionTitle;
    QString recordDirectory;
    int publishedRevision = 0;
    QString publishedHash;
    QString trigger;
    QString canonicalInput;
    QString finalOutput;
    QString pendingEditedText;
    OperationError terminalError;
    QVector<FunctionFlowNodeTrace> traces;
    QString failedNodeId;
    QString failedNodeType;
    QString sourceAudioPath;
    QVector<RecordingSegment> recordingSegments;
    qint64 speechElapsedMs = -1;
    QString recordingTriggerMode;
    bool longRecording = false;
    QString ocrEngineId;
    qint64 ocrElapsedMs = -1;
    bool ocrUsedFallback = false;
    QRect screenshotRect;
    bool cancelled = false;
};

struct FunctionFlowHistorySaveResult
{
    bool ok = false;
    QString detailPath;
    bool alreadyExists = false;
    OperationError error;
};

struct FunctionFlowHistoryEditRequest
{
    ExecutionId runId;
    QString recordDirectory;
    QString detailPath;
    QString editedText;
};

struct FunctionFlowHistoryEditResult
{
    bool ok = false;
    OperationError error;
};

struct FunctionFlowExecutionOptions
{
    int cancellationGraceMs = 3000;
};
```

所有真实适配器必须遵守：

- 执行控制器先创建 CancellationSource，并令
  `runContext.runId == cancellationSource.executionId()`；所有子任务沿用同一
  token，不能再生成第二个流程 executionId。
- 适配器外层的一次性完成门最多向执行控制器转发一次 completion。
- `values`只允许 Succeeded节点进入 scheduler；Failed/Cancelled节点即使意外
  返回 values也不得向下游传播。来源在已经取得录音文件/分段或截图上下文后才
  失败/取消时，把 provenance-only值放入 `historyObservations`，只供统一
  finalizer判断是否保存及提取安全元数据，绝不送入 Input/Model/Output。
- historyObservations中的 text/role保持为空，不得借该字段持久化中间识别文字、
  部分模型输出或 Provider响应正文。
- completion 可以同步或异步调用，控制器都必须安全。
- completion 到达时先核对 `runId`和取消令牌。
- 旧运行、已取消运行和已经完成节点的迟到回调全部丢弃。
- 后台线程不得直接操作 QWidget、QGraphicsItem 或 `QUndoStack`。
- 一次运行只捕获一次目标窗口，而且必须在 Vocekit主窗口、结果窗、Launcher或
  截图层取得焦点前完成；自动写入只能写回该窗口。ScreenshotLauncher维护最后
  一个非 Vocekit前台窗口，点击入口时使用该值，不能把 Launcher自身当作目标。
- 启动前依赖解析同时取得 targetWindow。当前触发画像包含 SelectionSource或
  AutoWrite时，必须用共享平台验证器确认句柄非空、仍存在且进程 ID不是当前
  Vocekit；失败返回 `flow_target_window_unavailable`且不得向该窗口发送
  Ctrl+C/粘贴。只有 ResultPopup/ScreenshotPanel的流程允许没有外部目标继续
  显示，write/replace按钮在点击时再验证并提示。动作执行前仍要二次验证，覆盖
  运行期间窗口被关闭的情况。
- 应用组装层只创建一个
  `std::function<bool(FunctionFlowTargetWindowHandle)>`平台验证回调，并同时注入
  依赖解析、Selection防御检查和 `FunctionFlowResultControllerAccess`；三处不能
  各自实现不同的“外部窗口”判断。Windows生产实现检查非空、`IsWindow()`和
  `GetWindowThreadProcessId()!=GetCurrentProcessId()`，控制器测试统一注入假值。
- 调度开始前一次性解析所有活跃节点的 prompt/model/provider、继承后的网络策略和
  强力选中开关，以及 functionTitle、recordDirectory和继承后的结果窗透明度，
  位置，写入只读 dependencies；运行中普通设置、历史目录或提示词变化不影响
  当前运行的保存目的地和首次展示配置。窗口显示后的用户拖动仍可通过非流程
  设置接口保存为下一次运行的位置。API Key只由 Provider在请求时从现有安全
  存储读取，不进入该快照。
- completion返回后通过排队调用继续调度，不在同步 completion 的原调用栈中递归执行下一节点。
- 用户取消时 Running节点先进入 `Cancelling`并向同一 CancellationToken 发出取消。
  最多等待固定 3000ms；底层仍未完成时由执行控制器的一次性完成门把该节点终结
  为 `Cancelled`。之后到达的底层回调只记录为迟到回调并丢弃。
- 执行控制器在自己的线程发出
  `FunctionFlowNodeExecutionEvent/FunctionFlowRunExecutionEvent`；事件必须包含
  完整 publishedHash。编辑器只
  把 functionId/hash/nodeId/nodeType都匹配的事件画到节点上，旧发布版事件只更新
  底部运行提示。事件是观察通道，不能反向修改调度器、图或撤销栈。

### 3.10 编译计划

```cpp
struct FunctionFlowCompiledInput
{
    QString edgeId;
    QString predecessorNodeId;
    QString predecessorPortId;
    int edgeOrder = 0;
    QString role;
    int sequence = 0;
    bool required = true;
};

struct FunctionFlowCompiledNode
{
    QString nodeId;
    FunctionFlowNodeType type = FunctionFlowNodeType::Input;
    FunctionFlowNodeConfig config;
    QVector<FunctionFlowCompiledInput> inputs;
    QStringList successors;
    QString streamingResultPopupNodeId;
};

struct FunctionFlowExecutionPlan
{
    int publishedRevision = 0;
    QString publishedHash;
    QStringList topologicalNodeIds;
    QMap<QString, FunctionFlowCompiledNode> nodes;
    QMap<FunctionFlowTrigger, FunctionFlowTriggerPlan> triggers;
    QStringList terminalActionNodeIds;
};

struct FunctionFlowCompileResult
{
    bool ok = false;
    FunctionFlowExecutionPlan plan;
    OperationError error;
};

class FunctionFlowCompiler
{
public:
    static FunctionFlowCompileResult compile(
        const FunctionFlowGraph &graph,
        int publishedRevision,
        const QString &publishedHash
    );
};

class FunctionFlowPlanCache
{
public:
    void rebuildAll(const AppSettingsData &settings);
    void rebuildFunction(
        const AppSettingsData &settings,
        const QString &functionId
    );
    QSharedPointer<const FunctionFlowExecutionPlan> plan(
        const QString &functionId
    ) const;
    OperationError error(const QString &functionId) const;
};
```

执行计划不写入 settings.json。应用启动、成功发布或启用流程时从
published图重新编译，并按
`functionId + publishedRevision + publishedHash`缓存在内存中。
缓存编译失败时把对应触发画像视为不可用并记录结构错误。
cache只保存 enabled且supported的 published；停用、删除或损坏时移除对应项。
加入缓存前重新计算规范语义 hash并与 publishedHash逐字符精确比较；不一致返回
`flow_published_hash_mismatch`并移除缓存，不能仅依赖 JSON解析阶段曾检查过。
返回不可变 QSharedPointer，正在运行的控制器持有旧计划时，事件刷新可替换 cache
条目但不能改变该运行。draft/editorState事件不得调用任何 rebuild方法。

编译器先剪除禁用节点及相关连线，再对剩余图校验。不能通过禁用节点连接上下游；剪除后失去可达性的启用节点导致发布失败。

稳定顺序：

1. 活跃采集节点按 `acquisitionSequence`，再按节点 ID。
2. 每个来源完成后，先执行它直接连接且已 ready 的 Input节点，按 Input.sequence/
   节点 ID；再开始下一个采集节点。
3. 当前触发画像的所有活跃来源终结前，不启动任何 Model。
4. Model 输入按 Input节点 `sequence`，再按 Input节点 ID。
5. 其余同层普通节点按拓扑层、入边 `order`、节点 ID。
6. 结果动作按唯一 Output出边 `order`，再按动作节点 ID。

新建出边时使用同一来源端口当前最大 `order + 1`。Inspector中的结果动作排序只重排这些出边的 `order`，不额外保存目标节点列表。

### 3.11 第一版纯调度语义

1. 调度器一次最多返回一个 ready 节点。
2. 来源节点由触发画像激活；不属于当前画像的来源标记 `Skipped`。
3. Input节点等待唯一上游终结：
   - 有非空文字时输出带 role/sequence 的值。
   - required 且上游无值时失败 `flow_required_input_empty`。
   - optional 且上游无值时标记 `Skipped`。
   - “无值”表示文字为空且没有 voice/screenshot provenance。optional上游文字
     为空但带录音或截图 payload时，Input仍以 `Succeeded`转发 provenance-only
     value；模型组装忽略其空文字，但历史和 ScreenshotPanel仍能取得元数据。
   - optional只容忍“来源正常完成但无数据”或“该来源不属于当前触发画像”；
     上游服务错误仍失败，用户/全局取消仍取消整条流程。
4. Model节点等待所有前驱 Input终结：
   - 任一 required 输入失败或缺失时失败。
   - optional 输入缺失时忽略。
   - 忽略 optional后没有任何非空值时失败 `flow_model_input_empty`，不发送空请求。
   - 按 sequence/节点 ID排序，一次组装、一次请求、一次输出。
   - 模型输出继承所有输入 value的 voice/screenshot provenance并集；第一版每种
     Source最多一个，因此同类 payload冲突视为编译器/调度器内部错误。
5. Output节点只有一个上游。空结果按配置 `fail`或 `skipActions`处理。
6. 结果动作按编译顺序串行执行。
7. 非 optional 节点失败后采用 fail-fast：尚未运行的后继标记 `Blocked`，其余未运行节点取消。
8. 用户取消后，Running节点进入 `Cancelling`；未运行节点标记 `Cancelled`且不再
   返回 ready 节点。底层完成或 3000ms宽限到期后 Running节点变为 `Cancelled`。
9. 同一节点不能第二次进入 Running，也不能第二次发射值。
10. 如果不存在 Ready/Running/Cancelling节点但仍有未终结节点，调度器立即设置
    `flow_scheduler_deadlock`并终结，执行控制器不得无限等待。

### 3.12 模型请求语义

模型节点不把多输入硬塞回 `VoiceRunContext`。使用独立组装器：

```text
[用户要求]
请翻译成中文

[待处理原文]
Hello

[参考资料]
...
```

规则：

- system prompt来自 `promptId`在运行开始时解析的快照。
- model来自 `modelId`在运行开始时解析的快照。
- role为空时使用“输入”。
- role和文本之间使用固定分隔，保持顺序。
- 调用通用 `ModelRequestTask`，不调用带听写/翻译/问答固定模板的 `VoiceRunExecutor`。
- 每个模型节点使用同一个流程取消令牌，但保留自己的节点 ID、耗时、模型 ID和提示词版本。
- 第一版模型节点串行执行。
- 发布后提示词内容可以继续按 ID更新；运行历史记录实际使用的提示词版本哈希，确保可追踪。
- 启用流式且满足 2.4 时，编译器把唯一 ResultPopup ID写入 Model 的
  `streamingResultPopupNodeId`。模型运行适配器在请求前创建
  runId/modelNodeId/popupNodeId绑定的流式预览；模型 delta只通过排队信号更新该预览。
  Model完成后，Output和ResultPopup节点接管同一个预览并提交最终文字，不能再
  创建第二个窗口。模型失败、取消或预览在生成中被关闭时取消当前流程。
- 生成中的 busy预览尚不是 post-run editable surface，不增加
  openEditableSurfaceCount。只有 Model成功、ResultPopup动作提交最终文字并接管
  窗口时才登记 opened一次；失败/取消销毁预览不得留下编辑状态计数。
- busy预览阶段不启动 ResultPopup的 displaySeconds自动关闭计时；最终接管后才按
  published配置启动，避免长模型请求被展示超时误取消。

### 3.13 结果动作、历史和兜底

- 结果动作不直接重复调用现有“单目的地 + 自动保存历史”的 `VoiceResultPresentationController::present()`。
- 新增流程结果动作控制器，复用 ResultChoicePopup、ScreenshotResultWindow、ClipboardWriter 和现有样式/设置能力。
- 保留经典调用方使用的 void `ClipboardWriter::pasteTextToWindow()`，另增
  `pasteTextToWindowChecked()`返回类型化结果。checked版本只有在目标通过验证、
  `SetForegroundWindow`成功且再次读取前台句柄仍等于冻结目标、`SendInput`返回
  预期事件数时才报告输入注入成功；失败返回稳定原因且不继续发送后续按键。它
  证明的是 Windows已接收输入事件，不伪称目标应用已经语义确认文字。
- checked版本保存剪贴板的 text/html/urls/image完整快照，并随待粘贴文字写入
  Vocekit私有 MIME token。延迟恢复前只有 token仍匹配才还原；用户期间复制了新
  内容就放弃恢复，绝不能用旧剪贴板覆盖用户的新操作。发送失败则按同一 token
  规则立即恢复。经典 void接口是否随后委托 checked实现必须保持现有可见行为。
- 连续两个 Vocekit写入在恢复计时窗内必须由单一 GUI线程 clipboard lease按
  generation合并：后一次继承第一次写入前的原始快照、替换 token并重启计时，
  旧 timer不得把中间 Vocekit文字恢复成“原剪贴板”。检测到用户 token外变更时
  立即放弃整条 lease。

```cpp
struct ClipboardWriteResult
{
    bool ok = false;
    QString errorCode;
};
```

流程结果 access只暴露返回该结果的 checked回调，单测使用假 writer，不直接注入
真实键盘事件。
- ResultChoicePopup和ScreenshotResultWindow增加可选的流程 checked
  write/replace回调。设置后按钮不得再走窗口内部的直接 ClipboardWriter/旧 void
  回调；checked返回 false时保留窗口并显示安全提示，只有成功才 resolve/close。
  未设置时完全保留经典现有直接写入/void回调行为，避免流程安全改造改变经典路径。
- 执行控制器在第一个结果动作前生成一次 canonicalInput，并通过
  `FunctionFlowResultActionRequest`传给每个动作；compare/detail模板和历史使用
  同一份文本，结果控制器不得自己从当前 UI或设置重新拼输入。
- 一次流程所有结果动作终结后，由流程执行控制器调用 `HistoryRecordService`一次。
- 唯一 Output 的值是历史最终结果；不能由多个 Output竞争“最后一个结果”。
- 每个 `Started`运行只执行一次统一 finalizer。成功运行保存一条历史；失败或取消
  时，只要已经采集到非空来源/Output文字或 voice/screenshot provenance，也保存
  同一条带失败/取消 trace的历史；在取得任何正文和 provenance前取消则不建空
  历史，只写脱敏运行日志。
- `saveHistory`失败返回 `flow_history_save_failed`并显示非阻塞提示，但不把已经
  完成的模型/写入动作重新执行，也不回退经典流程；自动重试不得冒险生成重复记录。
- terminal `FunctionFlowRunExecutionEvent`在统一 finalizer结束后才发出。历史保存
  失败不伪造某个业务节点失败，但 run事件的 terminalError使用
  `flow_history_save_failed`；节点 trace保持其真实终态。
- 历史适配器使用运行开始时冻结的 recordDirectory/functionTitle和运行值中的
  voice payload构造 `HistoryRecordSaveRequest`，不得在 finalizer里重新读取当前
  设置；screenshot payload只提取 engine/elapsed/fallback/rect等安全元数据，不
  保存 QImage、blocks或临时图片路径。成功后返回 detailPath并发布一次
  `HistoryChangeSet`；保存失败不发布“已新增”事件。后续 editedText更新成功后
  对同一 detailPath再发布一次更新事件。
- 流程 ResultPopup和ScreenshotPanel都允许编辑。用户编辑后关闭任一窗口时，
  把文字提交到 runId绑定的结果编辑状态；统一 finalizer尚未保存历史时把最新
  `pendingEditedText`并入首条记录，历史已保存后才按 flowRunId更新
  draft/output字段。多个可编辑结果窗按控制器收到提交的顺序执行显式
  last-write-wins；每次成功更新仍只修改同一条历史，不能追加第二条记录。
- 执行控制器为每个 run维护
  `pendingEditedText/detailPath/frozenRecordDirectory/openEditableSurfaceCount`。
  ResultPopup或ScreenshotPanel成功显示时计数，窗口无论正常关闭还是销毁都恰好
  递减一次；只有 finalizer完成且计数归零后才释放该 run的编辑状态。这样最终
  内容显示后动作节点虽已完成，稍后关闭窗口仍能使用冻结目录更新原历史。实时
  `onLiveDraft`只更新窗口自身，不写磁盘；关闭时的 draft提交才进入上述协调器。
- 新一轮流程可以在旧结果窗仍打开时开始。节点 completion仍按当前 generation
  丢弃旧 run回调，但 `editedTextCommitted/editableSurfaceClosed`是受保留编辑
  状态管理的后运行事件，不能只因已有新 run就丢弃；它们必须按自己的 runId精确
  命中状态，未知或已释放 run才忽略。
- 最多一个 AutoWrite；写入失败且 `fallbackToPopup=true`时，只在同一 Output没有显式 ResultPopup时显示一次兜底小框。
- 自动写入兜底小框没有图中 ResultPopup配置可继承，固定使用
  `FunctionFlowResultPopupConfig`安全默认动作和模板，并使用运行开始时冻结的
  全局透明度/窗口位置；不得重新启用 regenerate/retryModel/followUp。
- AutoWrite写入失败始终把该动作节点终结为 Failed并保留
  `flow_auto_write_failed`；兜底小框只是同一失败动作的尽力恢复 UI，不能把运行
  改成成功或增加第二条节点 trace。兜底窗也按可编辑 surface登记，编辑只回写
  同一历史；兜底窗自身创建失败记录安全诊断，但不覆盖最初的自动写入错误。
- AutoWrite前重新验证冻结 targetWindow仍存在且不是 Vocekit窗口；失效时返回
  `flow_target_window_unavailable`，不能退而写入当时的新前台窗口。
- 流程 ResultPopup中用户稍后点击 write/replace时执行同一目标校验并继续使用
  捕获句柄；失效只提示该按钮操作失败，不重新运行图、不写入新前台窗口。
- AutoWrite把 `collectedSelection`传给现有 ClipboardWriter语义；该值只在本 run
  的 SelectionSource成功采集非空文字时为 true，不从当前剪贴板或新前台窗口
  猜测。
- AutoWrite `writeMode=replace`要求 collectedSelection=true，并在写入前通过注入
  的无正文 `hasCurrentSelection(targetWindow)`探针确认冻结目标当前仍有选区；
  任一条件不满足都在调用 ClipboardWriter/SendInput前返回
  `flow_replace_selection_unavailable`并按配置显示兜底窗，绝不能让底层把
  “替换”静默降级成普通插入。生产探针调用新增的
  `SelectedTextReader::hasSelectionInWindow(targetWindow)`：先激活并核对冻结目标，
  再做 UIA/剪贴板读取和同步恢复，只返回 bool，不记录或持久化探测文字，不能
  误读当前 Vocekit结果窗的选区。`writeMode=insert`传给 writer
  的 hasSelection使用 `collectedSelection && liveSelectionPresent`；原选区已经
  丢失时不得仍发送 VK_RIGHT造成额外光标移动。
- ResultPopup/ScreenshotPanel的 replace按钮也在点击时执行同一实时选区探针；
  目标仍存在但选区已经丢失时只提示，不粘贴。
- ScreenshotPanel必须能从上游 provenance 取得截图 payload，否则发布失败 `flow_screenshot_context_missing`。
- `FunctionFlowResultActionRequest`为统一调用临时携带 output provenance，但结果
  控制器按节点类型最小化保留：ResultPopup/AutoWrite只复制所需文字、runId、
  target和 selection标志，回调不得捕获整份 request；只有 ScreenshotPanel窗口
  在打开期间持有 QImage/blocks，关闭后立即释放。
- ResultPopup/ScreenshotPanel在窗口成功显示后即完成动作节点，不等待用户关窗；
  AutoWrite在写入成功或失败后完成。最终内容显示后关闭窗口只提交可能存在的
  编辑并关闭 UI，不反向取消已经结束的流程；只有生成中的流式预览关闭才请求
  取消。
- 结果窗中的人工编辑是展示后的历史修订，不反向修改冻结的 Output，也不改变
  已排队或后续 AutoWrite的文字；AutoWrite始终使用
  `FunctionFlowResultActionRequest::output`。用户在结果窗主动点击 write/replace
  时才写入该窗口当前编辑文字。Inspector要提示这一点，避免“先弹窗再自动写入”
  被误解为等待人工确认。
- 结构损坏、未来 schema、停用或当前触发画像不可用：启动前走经典流程。
- 已启用发布图中的模型、提示词、服务或权限在运行前检查失败：显示流程配置错误，不走经典流程。
- 流程开始后失败或取消：只结束流程，不走经典流程。

历史使用 3.9 的 `FunctionFlowHistoryRequest/FunctionFlowNodeTrace`，只新增以下
流程元数据：

每条历史增加：

- `flowRunId`
- `flowPublishedRevision`
- `flowPublishedHash`
- `flowTrigger`
- `flowNodeTraces`
- `flowFailedNodeId`
- `flowFailedNodeType`

沿用现有 `HistoryEntry.input/output`作为用户可见正文：input保存按来源采集顺序
和 Input角色组装的 canonical input，但只包含直接前驱为 Source的 Input；用于
`Model A -> Input -> Model B`的中间 Input不得把模型中间结果再次写进用户原始
输入。output只保存唯一 Output的最终文字。它们
可以包含用户原文，这是历史功能本身，不得误判为 trace泄漏。流程新增的
`flowNodeTraces`、运行日志和错误 detail不得再复制系统提示词、任意节点输入、
选中文字、录音转写或模型中间结果；截图 QImage/blocks/Base64始终不持久化。
HistoryEntry.error只保存稳定错误码对应的用户提示，不保存 Provider原始响应体。

适配器层把底层错误归一到以下流程错误码，再交给集中映射；取消不能伪装成失败：

| 场景 | 流程错误码 |
|---|---|
| 用户或全局取消 | `flow_cancelled` |
| 选中文字读取失败 | `flow_selection_failed` |
| 录音/识别失败 | `flow_voice_failed` |
| 截图/OCR失败 | `flow_screenshot_failed` |
| 模型请求失败 | `flow_model_failed` |
| 结果小框创建失败 | `flow_result_popup_failed` |
| 截图对照窗创建失败 | `flow_screenshot_panel_failed` |
| 自动写入失败 | `flow_auto_write_failed` |

`functionFlowUserMessage()`只根据白名单 flow code返回用户提示；未知底层错误使用
安全通用文案，不能把 `OperationError.detail`或 Provider原始 message直接显示、
写历史或写日志。允许另行记录经过白名单筛选的 HTTP状态等诊断数字。

## 四、代码边界和文件结构

### 4.1 新增领域与持久化模块

- `src/domain/function_flow_graph.h/.cpp`
  - 节点、类型化配置、边、图、版本状态、编辑器状态、规范化和稳定哈希。
- `src/domain/function_flow_ports.h/.cpp`
  - 固定端口注册表和端口查找。
- `src/domain/function_flow_runtime_types.h/.cpp`
  - 瞬态值、截图 payload、运行上下文、节点结果、节点 trace。
- `src/domain/function_flow_validation.h/.cpp`
  - 结构、端口、配置、触发画像、流式限制、规模限制和发布校验。
- `src/domain/function_flow_publication_types.h`
  - 发布/草稿分析的共享返回 DTO，供 publication service和 UI访问契约共同使用。
- `src/domain/function_flow_compiler.h/.cpp`
  - 禁用节点剪除、稳定拓扑排序、输入绑定、触发画像和动作顺序。
- `src/domain/function_flow_scheduler.h/.cpp`
  - 纯串行状态机，不访问 QObject、UI、文件或网络。
- `src/domain/function_flow_errors.h/.cpp`
  - 稳定错误码到用户提示和 FAQ ID的集中映射，不包含用户正文。
- `src/config/function_flow_json.h/.cpp`
  - 流程 JSON、未知字段保留和未来 schema 往返。
- `src/controllers/function_flow_publication_service.h/.cpp`
  - 草稿乐观版本保存、发布事务、停用和自定义功能创建/删除协调。

### 4.2 新增运行模块

- `src/controllers/function_flow_execution_controller.h/.cpp`
  - 冻结发布快照、运行 ID、目标窗口、调度循环、取消、代次和一次历史收尾。
- `src/controllers/function_flow_plan_cache.h/.cpp`
  - 启动编译、按 functionId局部刷新和不可变已发布计划缓存。
- `src/controllers/function_flow_runtime_adapters.h/.cpp`
  - 选中文字、语音、截图、模型的窄适配。
- `src/controllers/function_flow_result_controller.h/.cpp`
  - 结果小框、截图对照窗、自动写入和多动作串行执行，不自行重复保存历史。
- `src/domain/function_flow_model_message.h/.cpp`
  - 按角色和顺序组装单次模型请求。

### 4.3 新增画布编辑器

- `src/ui/function_canvas_scene.h/.cpp`
  - 图形项所有权、选择、连接预览和编辑意图。
- `src/ui/function_canvas_node_item.h/.cpp`
  - 节点卡片、端口、状态绘制和拖动。
- `src/ui/function_canvas_edge_item.h/.cpp`
  - 贝塞尔连线、箭头、命中区域和顺序。
- `src/ui/function_canvas_palette.h/.cpp`
  - 节点库和搜索。
- `src/ui/function_canvas_inspector.h/.cpp`
  - 与 2.3 一致的真实可用设置。
- `src/ui/function_canvas_editor.h/.cpp`
  - 工具栏、画布、设置栏、状态栏和发布区。
- `src/ui/function_flow_settings_access.h`
  - UI使用的流程读取、只读分析、草稿、发布、停用和生命周期回调；不暴露具体
    publication service或 AppSettingsStore。
- `src/controllers/function_flow_editor_controller.h/.cpp`
  - 工作副本、撤销栈、500ms 保存、冲刷、校验和发布。
- 修改 `src/ui/function_canvas_view.h/.cpp`
  - 只保留手势、缩放、平移和背景绘制。

### 4.4 修改现有入口

- `src/domain/function_settings.h/.cpp`
  - 增加 `FunctionFlowState flow`。
- `src/config/app_settings_data.h`
  - 增加孤儿/未知流程保留字段。
- `src/config/app_settings_defaults.h/.cpp`
  - 提供语音服务和 OCR 引擎稳定 ID目录，供设置 UI、发布校验和运行适配器复用。
- `src/config/app_settings_json.cpp`
  - 调用 `function_flow_json`读写顶层 `functionFlows`。
- `src/config/app_settings_store.h/.cpp`
  - 提供发布服务所需的事务式快照保存，以及保留最新流程版本的 `replaceNonFlowSettingsAndSave()`；不向 UI暴露 JSON。
- `src/ui/function_command_page.h/.cpp`
  - 使用完整 FunctionCanvasEditor。
- `src/ui/hub_settings_state.h/.cpp`
- `src/ui/function_command_page_access_factory.h/.cpp`
  - 在现有 `HubWindowAccess`和`FunctionCommandPageAccess`中注入流程读取、草稿、
    发布、停用和功能生命周期的窄访问接口；不新建第二套设置状态。
- `src/ui/custom_function_creation_coordinator.*`
- `src/ui/function_management_page_access_factory.*`
- `src/ui/function_pages_access_factory.*`
- `src/ui/function_workspace_controller.*`
- `src/ui/hub_function_workspace_controller.*`
  - 创建和删除自定义功能走同一事务服务，普通设置保存不能改变功能 ID集合。
- `src/controllers/function_command_controller.h/.cpp`
  - 按触发入口询问流程启动结果；只有 `NotAvailable`时执行经典流程。
- `src/input/hotkey_settings_snapshot.h/.cpp`
  - 从已发布触发画像生成 hold和截图快捷键运行配置。
- `src/app/application_events.h/.cpp`
  - 继续使用 SettingsChangeSet，但明确区分 function definitions、draft、published和enabled。
- `src/ui/hub_refresh_coordinator_bundle.*`
  - 按 SettingsChangeSet.keys局部刷新，草稿事件不触发整套设置刷新。
- `src/app/vocekit_application_runtime.cpp`
  - 创建服务与控制器；只在 function definitions、published或enabled变化时刷新运行时和快捷键。
- `src/controllers/voice_recording_workflow_controller.*`
  - 增加不破坏经典路径的流程完成回调入口。
- `src/controllers/screenshot_workflow_controller.*`
  - 增加返回截图瞬态 payload 的流程入口。
- `src/controllers/voice_result_presentation_controller.*`
  - 提取流程结果控制器可复用的窗口配置和动作能力；经典 present行为保持不变。
- `src/domain/history_types.*`
- `src/domain/history_record_builder.*`
- `src/storage/history_record_service.*`
  - 增加一次流程 trace 元数据。
- `vocekit.pro`
  - 加入新增源文件和头文件，不引入 QML。

## 五、分批实施任务

### Task 1: 建立图模型、端口和运行时类型

**Files:**
- Create: `src/domain/function_flow_graph.h`
- Create: `src/domain/function_flow_graph.cpp`
- Create: `src/domain/function_flow_ports.h`
- Create: `src/domain/function_flow_ports.cpp`
- Create: `src/domain/function_flow_runtime_types.h`
- Create: `src/domain/function_flow_runtime_types.cpp`
- Create: `tests/domain/function_flow_graph_tests.cpp`
- Create: `tests/domain/function_flow_graph_tests.pro`
- Modify: `vocekit.pro`

- [ ] **Step 1: 写默认值、端口和无损规范化失败测试**

```cpp
void FunctionFlowGraphTests::normalizationNeverDeletesDuplicateIds()
{
    FunctionFlowGraph graph;
    graph.nodes << node("same", FunctionFlowNodeType::Input)
                << node("same", FunctionFlowNodeType::Output);

    const FunctionFlowGraph normalized =
        normalizeFunctionFlowGraph(graph);

    QCOMPARE(normalized.nodes.size(), 2);
}

void FunctionFlowGraphTests::portsComeFromNodeTypeRegistry()
{
    const QVector<FunctionFlowPortSpec> ports =
        functionFlowPortSpecs(FunctionFlowNodeType::Model);
    QVERIFY(hasFunctionFlowPort(
        FunctionFlowNodeType::Model,
        "text_in",
        FunctionFlowPortDirection::Input
    ));
    QVERIFY(hasFunctionFlowPort(
        FunctionFlowNodeType::Model,
        "text_out",
        FunctionFlowPortDirection::Output
    ));
}
```

- [ ] **Step 2: 运行测试并确认缺少类型**

```powershell
Push-Location tests\domain
qmake -o Makefile.codex.function_flow_graph function_flow_graph_tests.pro -spec win32-g++ CONFIG+=debug
mingw32-make -f Makefile.codex.function_flow_graph -j2
Pop-Location
```

Expected: 编译失败，提示 `FunctionFlowGraph`或端口类型未定义。

- [ ] **Step 3: 实现 3.1–3.4 和 3.8–3.9 中的类型**

必须实现：

`function_flow_runtime_types.h`因瞬态截图 payload使用 `QImage`而依赖 QtGui，
但仍不得依赖 QtWidgets；`function_flow_graph_tests.pro`及任何直接编译该头的
测试工程必须显式包含 `QT += core gui testlib`。

```cpp
QString functionFlowNodeTypeId(FunctionFlowNodeType type);
FunctionFlowNodeType functionFlowNodeTypeFromId(
    const QString &id,
    bool *ok = nullptr
);
QVector<FunctionFlowPortSpec> functionFlowPortSpecs(
    FunctionFlowNodeType type
);
bool hasFunctionFlowPort(
    FunctionFlowNodeType type,
    const QString &portId,
    FunctionFlowPortDirection direction
);
bool isFunctionFlowConnectionAllowed(
    FunctionFlowNodeType fromType,
    const QString &fromPortId,
    FunctionFlowNodeType toType,
    const QString &toPortId
);
FunctionFlowGraph normalizeFunctionFlowGraph(
    const FunctionFlowGraph &graph
);
QString functionFlowGraphHash(const FunctionFlowGraph &graph);
QString newFunctionFlowObjectId();
QStringList supportedFunctionFlowPopupActionIds();
QStringList defaultFunctionFlowPopupActionIds();
bool isFunctionFlowPopupActionSupported(const QString &id);
```

- [ ] **Step 4: 补充边界测试**

覆盖：

- 未知节点类型返回 `ok=false`。
- 连线矩阵只接受 3.1 中列出的六类组合。
- `Model -> Model`被拒绝，`Model -> Input -> Model`被接受。
- 规范化不删除重复节点或悬空边。
- NaN/Inf坐标替换为安全值。
- zoom约束为 0.35–3.0。
- 录音时长、OCR超时、顺序、显示时间和透明度等非法运行数值不会被
  normalize静默改成合法值。
- graph hash不受节点/边输入数组顺序影响。
- graph hash固定为 64 个小写十六进制字符。
- 固定最小图的 graph hash等于测试中的 golden SHA-256；改变对象插入顺序或
  当前 locale不改变结果。
- graph hash不包含节点 title/position、viewport/zoom和 retainedValues。
- graph hash包含节点配置、enabled、端口和 edge.order。
- voice/screenshot payload只存在于 runtime type，不存在于 JSON图结构；voice
  payload能承载 sourceAudioPath、分段和 speechElapsedMs供历史收尾。
- 流程 popup默认动作不包含 regenerate/retryModel/followUp，且不调用经典动作
  normalizer。

- [ ] **Step 5: 运行专项测试和静态检查停靠点**

Expected: `function_flow_graph_tests`全部通过。

```powershell
Invoke-CodexQtTest tests\domain\function_flow_graph_tests.pro
git diff --check -- src/domain/function_flow_graph.* src/domain/function_flow_ports.* src/domain/function_flow_runtime_types.* tests/domain/function_flow_graph_tests.*
```

### Task 2: 实现发布校验、触发画像和确定性编译

**Files:**
- Create: `src/domain/function_flow_validation.h`
- Create: `src/domain/function_flow_validation.cpp`
- Create: `src/domain/function_flow_compiler.h`
- Create: `src/domain/function_flow_compiler.cpp`
- Create: `tests/domain/function_flow_validation_tests.cpp`
- Create: `tests/domain/function_flow_validation_tests.pro`
- Create: `tests/domain/function_flow_compiler_tests.cpp`
- Create: `tests/domain/function_flow_compiler_tests.pro`
- Modify: `vocekit.pro`

- [ ] **Step 1: 写结构校验失败测试**

覆盖并断言稳定错误码：

- 空图：`flow_empty`
- 重复节点 ID：`flow_duplicate_node_id`
- 重复边 ID：`flow_duplicate_edge_id`
- 相同 fromNode/fromPort/toNode/toPort重复连线：`flow_duplicate_connection`
- 未知端口：`flow_unknown_port`
- 方向错误：`flow_port_direction`
- 端口数量超限：`flow_port_cardinality`
- 必需输入端口没有连线或 Output没有动作出边：`flow_port_connection_missing`
- 不支持的节点类型组合：`flow_edge_type_unsupported`
- 自连接：`flow_self_edge`
- 环：`flow_cycle`
- 悬空边：`flow_dangling_edge`
- edge.order越界：`flow_edge_order_invalid`
- 启用 Output数量不是 1或没有结果动作：`flow_output_count`
- 超过 128节点或256边：`flow_size_limit`
- 禁用节点剪除后，启用节点不能从任何活跃来源到达，或不能继续到达唯一
  Output/结果动作：`flow_enabled_node_unreachable`

关键断言：

```cpp
const FunctionFlowValidationResult result =
    FunctionFlowValidator::validateForPublish(
        graph,
        validFunctionFlowValidationContext()
    );
QVERIFY(!result.ok);
QVERIFY(result.issueCodes.contains(
    QStringLiteral("flow_duplicate_node_id")
));
QCOMPARE(graph.nodes.size(), 2);
```

- [ ] **Step 2: 写节点配置和触发画像测试**

覆盖：

- Model缺模型或提示词。
- 3.2 表中的每类非法枚举和越界数值。
- 模型、提示词、语音服务或 OCR 引擎 ID不在引用目录中，分别返回
  `flow_model_reference_missing`、`flow_prompt_reference_missing`、
  `flow_speech_provider_reference_missing`和 `flow_ocr_engine_reference_missing`。
- 剪除 disabled节点后，同一种 VoiceSource、SelectionSource或ScreenshotSource
  出现多个。
- Source直接连接 Model、Model直接连接 Model、Output直接连接 Model。
- 每个 Model 的直接前驱都经过 Input，编译结果保留 role/sequence/required。
- Input角色为空、sequence重复时按节点 ID稳定排序。
- Input角色包含换行、控制字符或方括号。
- required输入在当前触发画像没有活跃来源。
- MainHotkey画像缺 function主快捷键。
- ScreenshotHotkey画像缺 separateShortcut，或与主/其它全局快捷键冲突。
- hold语音不是第一个采集节点。
- 多个hold语音节点。
- hold语音和primary截图同时出现在MainHotkey画像。
- 独立截图入口只激活 separate截图来源。
- ScreenshotPanel没有截图 provenance。
- 多个 AutoWrite。
- AutoWrite配置 fallbackToPopup且 Output已有显式 ResultPopup时仍可发布，但编译
  结果必须标记兜底已被显式 Popup覆盖，运行时不能创建第二个小框。
- ResultPopup包含 regenerate/retryModel/followUp。
- ResultPopup动作重复返回 `flow_popup_action_duplicate`，含未知 ID返回
  `flow_popup_action_unsupported`。
- edge.order为负数、超过 10,000或 `INT_MAX`时返回
  `flow_edge_order_invalid`，新建边不会执行溢出的 max+1。
- 不支持的流式拓扑。
- 每个可用 trigger分别做 ScreenshotPanel provenance检查；整图有截图节点但
  MainHotkey画像无法产生截图 payload时仍返回
  `flow_screenshot_context_missing`。
- 每个可用 trigger分别做 AutoWrite replace的 Selection provenance检查；
  ScreenshotHotkey画像无法产生选区时返回
  `flow_replace_selection_context_missing`。

- [ ] **Step 3: 写确定性编译测试**

```cpp
const FunctionFlowCompileResult first =
    FunctionFlowCompiler::compile(graph, 4, fullHash());
const FunctionFlowCompileResult second =
    FunctionFlowCompiler::compile(shuffled(graph), 4, fullHash());

QVERIFY(first.ok);
QVERIFY(second.ok);
QCOMPARE(
    first.plan.topologicalNodeIds,
    second.plan.topologicalNodeIds
);
QCOMPARE(
    first.plan.triggers.value(FunctionFlowTrigger::MainHotkey)
        .acquisitionNodeIds,
    QStringList() << "voice" << "selection"
);
QCOMPARE(
    first.plan.topologicalNodeIds.mid(0, 4),
    QStringList()
        << "voice"
        << "input_instruction"
        << "selection"
        << "input_source"
);
```

- [ ] **Step 4: 实现校验器和编译器**

编译结果必须包含完整 `FunctionFlowCompiledNode`、输入绑定、触发画像和动作顺序；不能只返回节点 ID列表。

- [ ] **Step 5: 运行两个测试工程**

```powershell
Invoke-CodexQtTest tests\domain\function_flow_validation_tests.pro
Invoke-CodexQtTest tests\domain\function_flow_compiler_tests.pro
```

Expected:

- 重复 ID被拒绝且原图不丢节点。
- 三种触发画像分别可用或给出具体错误。
- 同一语义图每次得到相同顺序和哈希。
- 合法流式拓扑把唯一 popup节点 ID编译进对应 Model。
- 循环/悬空/内部不一致图返回 `ok=false`和稳定错误码，不返回半成品计划。

- [ ] **Step 6: 仅对 Task 2列出的路径运行 `git diff --check`并检查新增文件尾随空白**

### Task 3: 实现无损 JSON、草稿保存和发布事务

**Files:**
- Create: `src/config/function_flow_json.h`
- Create: `src/config/function_flow_json.cpp`
- Create: `src/domain/function_flow_publication_types.h`
- Create: `src/controllers/function_flow_publication_service.h`
- Create: `src/controllers/function_flow_publication_service.cpp`
- Create: `src/controllers/function_flow_plan_cache.h`
- Create: `src/controllers/function_flow_plan_cache.cpp`
- Modify: `src/domain/function_settings.h`
- Modify: `src/domain/function_settings.cpp`
- Modify: `src/config/app_settings_data.h`
- Modify: `src/config/app_settings_defaults.h`
- Modify: `src/config/app_settings_defaults.cpp`
- Modify: `src/config/app_settings_json.cpp`
- Modify: `src/config/app_settings_store.h`
- Modify: `src/config/app_settings_store.cpp`
- Modify: `src/app/application_events.h`
- Modify: `src/app/application_events.cpp`
- Modify: `src/ui/hub_settings_state.h`
- Modify: `src/ui/hub_settings_state.cpp`
- Modify: `tests/config/app_settings_json_tests.cpp`
- Modify: `tests/config/app_settings_json_tests.pro`
- Modify: `tests/config/app_settings_defaults_tests.cpp`
- Modify: `tests/config/app_settings_defaults_tests.pro`
- Modify: `tests/app/application_events_tests.cpp`
- Modify: `tests/app/application_events_tests.pro`
- Modify: `tests/ocr/ocr_core_tests.pro`
- Create: `tests/config/function_flow_json_tests.cpp`
- Create: `tests/config/function_flow_json_tests.pro`
- Create: `tests/controllers/function_flow_publication_service_tests.cpp`
- Create: `tests/controllers/function_flow_publication_service_tests.pro`
- Create: `tests/controllers/function_flow_plan_cache_tests.cpp`
- Create: `tests/controllers/function_flow_plan_cache_tests.pro`
- Modify: `vocekit.pro`

- [ ] **Step 1: 写 JSON 往返和未知字段测试**

```cpp
QJsonObject raw = validFunctionFlowJson();
raw.insert(QStringLiteral("futureField"), 42);
QJsonObject draft = raw.value(QStringLiteral("draft")).toObject();
QJsonObject graph = draft.value(QStringLiteral("graph")).toObject();
graph.insert(QStringLiteral("futureGraphField"), true);
draft.insert(QStringLiteral("graph"), graph);
raw.insert(QStringLiteral("draft"), draft);

const FunctionFlowState state = functionFlowStateFromJson(raw);
const QJsonObject restored = functionFlowStateToJson(state);

QCOMPARE(restored.value("futureField").toInt(), 42);
```

还要覆盖：

- 旧设置没有 `functionFlows`。
- 内置和自定义功能都写到顶层 map。
- viewport/zoom只在 editor。
- API Key、截图、录音和运行值不进入 JSON。
- 未来 schema原样保留且 `supported=false`。
- future/corrupt分别保留 `flow_schema_newer`和`flow_json_invalid`。
- 当前 schema中的未知节点 type完整保留并标记
  `flow_node_type_unsupported`，不转换成 Input。
- draft磁盘 hash错误时使用重算值且仍不能直接执行；revision>0或enabled的
  published缺失 hash、非 64位小写十六进制或与重算值不符时标记
  `flow_published_hash_mismatch`并经典回退。
- future published不能被当前 draft发布覆盖；停用不破坏其 retainedRaw。
- 损坏 draft不覆盖合法 published。
- 损坏 published不影响经典设置。
- 孤儿流程加载后保留。
- 普通设置保存后孤儿流程仍逐字段无损。
- `functionFlows`不重复进入 retainedRootValues，写回只有一个顶层对象。
- `normalizeFunctionSettings()`不会清空或重建现有 flow。

- [ ] **Step 2: 写草稿乐观版本测试**

```cpp
int savedRevision = 0;
QVERIFY(service.updateDraft("custom_1", 3, graph,
                            &savedRevision, &error));
QCOMPARE(savedRevision, 4);

QVERIFY(!service.updateDraft("custom_1", 3, graph,
                             &savedRevision, &error));
QCOMPARE(error.code, QStringLiteral("flow_draft_stale"));
```

- [ ] **Step 3: 写旧设置快照不覆盖新流程测试**

先从 `HubSettingsState`取得 revision 3 的旧 `AppSettingsData`，再通过发布服务把
草稿保存到 revision 4。随后只修改旧快照中的一个普通 UI 设置，并调用
`replaceNonFlowSettingsAndSave()`。最终从 AppSettingsStore 重读并断言：

- 普通 UI 设置已更新。
- draft revision 仍为 4，graphHash 和图内容不变。
- published/enabled 不被旧快照覆盖。
- 对应设置事件会把 revision 4 同步到 HubSettingsState。
- 当前编辑器仍 dirty 时不替换其本地图和撤销栈。
- 功能 ID集合不同返回 `settings_function_set_stale`且文件保持不变。
- `updateEditorState()`只更新 viewport/zoom，保留最新图且 draft/published
  revision都不增加。
- 先保存较新 editor再调用 `updateDraft()`，视口不回退；先保存较新 draft再调用
  `updateEditorState()`，图和 revision不回退。两个接口都不接受另一侧快照。

- [ ] **Step 4: 写发布事务测试**

覆盖：

- 校验失败不改变旧 published。
- expectedDraftRevision过期。
- 发布成功增加 published revision。
- published.sourceDraftRevision等于当前 draft revision。
- publish成功在同一事务中设置 enabled=true。
- 相同语义哈希重复 publish且已启用时不保存、不增 revision、不发事件；停用时
  只重新启用。
- 仅 title/position/editor变化不制造新 published revision。
- setEnabled(true)会重校验已有 published；无发布版或引用失效时保持停用。
- setEnabled(false)不依赖草稿是否合法。
- future schema/未知节点 published永不允许当前版本覆盖；当前版本损坏或 hash
  不匹配的 published只有 `replaceCorruptPublished=true`且 draft完全合法时才可
  原子修复，默认返回 `flow_published_repair_confirmation_required`。
- 保存失败恢复内存快照。
- 调用方不能传入任意 published graph。
- 创建自定义功能时忽略调用方 flow并生成停用空流程。
- 创建时拒绝 builtIn=true、空/重复 ID和空名称；失败不改变 functionOrder。
- retainedOrphanFunctionFlows中的 ID同样视为已占用，创建不能覆盖孤儿原始 JSON。
- 删除自定义功能和流程在同一 `replaceAndSave()`中完成。
- 删除事务同时清理 functionOrder、已知流程和同 ID 孤儿流程。
- 删除 built-in或不存在的 ID会失败且不保存。
- 普通设置保存不能删除自定义功能，也不能隐式创建或删除流程。
- add/remove/draft/editor/publish/enabled只在保存成功后发布对应设置事件；相同
  hash的重新启用只发 enabled，不伪报 published内容变化。
- `analyzeDraft()`返回与 publish同源的校验结果、语义 hash和三个 trigger可用性，
  但不保存、不增 revision、不发事件。
- `supportedSpeechProviderIds()/supportedOcrEngineIds()`返回稳定、无重复的内部 ID，
  与设置页/运行适配器使用同一常量，不创建 Provider也不做网络自检。
- plan cache只缓存 enabled且supported的 published；按
  functionId/revision/full hash返回不可变计划，draft/editor事件不重建，删除、
  停用、损坏或编译失败时移除条目。持有旧 QSharedPointer的运行不受重建影响。
- 即使构造一个绕过 JSON解析、supported=true但 graphHash不匹配的 published，
  plan cache也拒绝并返回 `flow_published_hash_mismatch`。

- [ ] **Step 5: 实现 JSON、非流程合并和发布服务**

`AppSettingsStore`继续通过 `QSaveFile`原子保存。`replaceNonFlowSettingsAndSave()`
以 store 当前快照为基准，只从调用方合并非流程字段，并保留
`FunctionSettings::flow`和 `retainedOrphanFunctionFlows`；函数 ID集合不一致时
整次拒绝。发布服务只接收类型化快照访问和保存回调，不读取 UI、不弹窗。

- [ ] **Step 6: 运行配置和服务专项测试**

Expected: 新旧配置、未来 schema、损坏分支、版本冲突、旧快照合并和保存回滚全部通过。

```powershell
Invoke-CodexQtTest tests\config\function_flow_json_tests.pro
Invoke-CodexQtTest tests\config\app_settings_json_tests.pro
Invoke-CodexQtTest tests\config\app_settings_defaults_tests.pro
Invoke-CodexQtTest tests\controllers\function_flow_publication_service_tests.pro
Invoke-CodexQtTest tests\controllers\function_flow_plan_cache_tests.pro
Invoke-CodexQtTest tests\app\application_events_tests.pro
Invoke-CodexQtTest tests\ocr\ocr_core_tests.pro
```

`ocr_core_tests.pro`当前也编译 `app_settings_json.cpp/app_settings_store.cpp`；新增
function flow JSON依赖后必须同步它的 SOURCES/HEADERS，否则全量测试会在该旧工程
链接失败。

- [ ] **Step 7: 检查真实配置隔离**

测试不得读取或写入：

- `config/settings.json`
- `config/secrets.json`
- `config/prompts.json`
- `records/`

### Task 4: 实现纯确定性串行调度器

**Files:**
- Create: `src/domain/function_flow_scheduler.h`
- Create: `src/domain/function_flow_scheduler.cpp`
- Create: `tests/domain/function_flow_scheduler_tests.cpp`
- Create: `tests/domain/function_flow_scheduler_tests.pro`
- Modify: `vocekit.pro`

- [ ] **Step 1: 写两输入 waitAll 测试**

```cpp
FunctionFlowScheduler scheduler(plan, FunctionFlowTrigger::MainHotkey);

QCOMPARE(scheduler.nextReadyNode(), QStringLiteral("voice"));
scheduler.succeed("voice", values("请翻译"));
QCOMPARE(scheduler.nextReadyNode(), QStringLiteral("input_instruction"));
scheduler.succeed("input_instruction",
                  scheduler.inputValues("input_instruction"));

QCOMPARE(scheduler.nextReadyNode(), QStringLiteral("selection"));
scheduler.succeed("selection", values("Hello"));
QCOMPARE(scheduler.nextReadyNode(), QStringLiteral("input_source"));
scheduler.succeed("input_source",
                  scheduler.inputValues("input_source"));

QCOMPARE(scheduler.nextReadyNode(), QStringLiteral("model"));
QCOMPARE(scheduler.inputValues("model").size(), 2);
```

- [ ] **Step 2: 写 optional、空值和失败测试**

覆盖：

- required来源成功但文字为空 -> Input失败。
- optional来源正常完成但无数据 -> Input跳过，其它非空输入存在时 Model仍运行一次。
- optional来源文字为空但带 voice/screenshot payload -> Input成功转发
  provenance，模型忽略空文字但最终 Output仍保留 payload。
- optional来源服务失败 -> 流程失败；用户取消 -> 整条流程取消。
- 所有 Model输入都 optional且都为空 -> `flow_model_input_empty`，Provider调用为 0。
- required前驱失败 -> fail-fast，后继Blocked。
- 模型成功只允许发射一次。
- 第二次 `succeed()`返回状态错误，不改变第一次结果。
- cancel后 Running进入 Cancelling，其它未终结节点进入 Cancelled，不再返回 ready。
- `completeCancelled()`只接受 Cancelling节点并使整个调度器可终结。

- [ ] **Step 3: 写分支、汇合和动作顺序测试**

覆盖：

- 一个模型结果进入唯一 Output，再按 edge.order进入两个结果动作。
- 两个 Input汇合到一个 Model。
- ResultPopup和AutoWrite按 edge.order串行 ready。
- 任意时刻最多一个节点处于 Running。
- separate截图触发时，MainHotkey来源不会错误执行。
- 人工构造不一致计划时检测 `flow_scheduler_deadlock`，不挂起测试事件循环。

- [ ] **Step 4: 实现纯调度器**

公开最小接口：

```cpp
FunctionFlowScheduler(
    const FunctionFlowExecutionPlan &plan,
    FunctionFlowTrigger trigger
);
QString nextReadyNode();
QList<FunctionFlowValue> inputValues(const QString &nodeId) const;
bool start(const QString &nodeId, OperationError *error = nullptr);
bool succeed(
    const QString &nodeId,
    const QList<FunctionFlowValue> &values,
    OperationError *error = nullptr
);
bool skip(const QString &nodeId, OperationError *error = nullptr);
bool fail(
    const QString &nodeId,
    const OperationError &failure,
    OperationError *error = nullptr
);
bool completeCancelled(
    const QString &nodeId,
    OperationError *error = nullptr
);
void cancel();
FunctionFlowNodeState state(const QString &nodeId) const;
bool finished() const;
OperationError terminalError() const;
```

- [ ] **Step 5: 运行调度器专项测试**

Expected: 顺序、分支、汇合、optional、失败和取消全部由纯测试证明；没有 QObject、网络和文件依赖。

```powershell
Invoke-CodexQtTest tests\domain\function_flow_scheduler_tests.pro
```

### Task 5: 实现假适配器的一次运行控制器

**Files:**
- Create: `src/controllers/function_flow_execution_controller.h`
- Create: `src/controllers/function_flow_execution_controller.cpp`
- Create: `tests/controllers/function_flow_execution_controller_tests.cpp`
- Create: `tests/controllers/function_flow_execution_controller_tests.pro`
- Modify: `vocekit.pro`

- [ ] **Step 1: 定义运行访问接口**

```cpp
struct FunctionFlowRuntimeAccess
{
    std::function<bool(
        const FunctionFlowExecutionPlan &,
        FunctionFlowTrigger,
        FunctionFlowTargetWindowHandle,
        QSharedPointer<const FunctionFlowResolvedDependencies> *,
        OperationError *
    )> resolveDependencies;

    std::function<void(
        const FunctionFlowRunContext &,
        const FunctionFlowCompiledNode &,
        const FunctionFlowNodeCompletion &
    )> collectVoice;

    std::function<void(
        const FunctionFlowRunContext &,
        const FunctionFlowCompiledNode &,
        const FunctionFlowNodeCompletion &
    )> collectSelection;

    std::function<void(
        const FunctionFlowRunContext &,
        const FunctionFlowCompiledNode &,
        const FunctionFlowNodeCompletion &
    )> collectScreenshot;

    std::function<void(
        const FunctionFlowRunContext &,
        const FunctionFlowCompiledNode &,
        const QList<FunctionFlowValue> &,
        const FunctionFlowNodeCompletion &
    )> runModel;

    std::function<void(
        const FunctionFlowRunContext &,
        const FunctionFlowCompiledNode &,
        const FunctionFlowResultActionRequest &,
        const FunctionFlowNodeCompletion &
    )> runResultAction;

    std::function<FunctionFlowHistorySaveResult(
        const FunctionFlowHistoryRequest &
    )> saveHistory;

    std::function<FunctionFlowHistoryEditResult(
        const FunctionFlowHistoryEditRequest &
    )> updateHistoryEditedText;
};
```

`saveHistory`生产适配器必须把“调用 HistoryRecordService、取得 detailPath、成功
后发布 HistoryChangeSet”封装成一个完成语义；执行控制器不直接依赖
ApplicationEvents。`alreadyExists=true`只用于防御同一 runId已经成功写入后的
重复 finalizer，仍视为成功但不得再次发布“新增”事件。
`updateHistoryEditedText`同样封装历史服务和更新事件；请求必须使用该 run冻结的
recordDirectory及 `saveHistory`返回的 detailPath，不能让窗口根据当前设置重新
定位历史。

- [ ] **Step 2: 写一次运行冻结测试**

断言：

- 开始时捕获一次 targetWindow。
- 当前画像含 SelectionSource或AutoWrite时，resolveDependencies在任何输入适配器
  启动前验证外部 target；无效句柄不发送 Ctrl+C/粘贴且返回
  TargetUnavailable。纯展示流程允许 null target。
- 运行期间替换 published不会改变当前 revision/hash。
- 开始时解析一次活跃节点 dependencies，运行中修改提示词、模型或全局网络设置
  不改变当前快照。
- 运行中修改历史目录、结果窗透明度或位置不改变当前 run的首次展示/保存目的地。
- 活跃节点引用无效时返回 ConfigurationError且不进入 scheduler。
- 同一 CancellationToken传给所有节点。
- runId等于 CancellationSource.executionId，模型子任务不生成第二个运行 ID。
- 节点事件包含 runId和 nodeId。

- [ ] **Step 3: 写同步与异步 completion 测试**

覆盖：

- 适配器在调用栈内同步完成。
- 适配器稍后异步完成。
- completion重复调用只接受第一次并记录防御日志。
- 取消后的迟到 completion不改变节点状态。
- 上一运行迟到回调不污染下一运行。
- 底层取消不回调时，3000ms后由完成门终结节点和流程。
- 宽限到期后的成功回调保持 Cancelled且不重复保存历史。

测试通过 `FunctionFlowExecutionOptions`注入极短宽限或假时钟，不能让单测真实
等待 3000ms；生产构造固定使用默认值。

- [ ] **Step 4: 写运行顺序和一次收尾测试**

假适配器调用顺序：

```text
voice -> input_instruction -> selection -> input_source
      -> model -> output -> popup -> auto_write -> save_history
```

断言：

- 模型只调用一次。
- 同时 Running节点数不超过 1。
- 历史保存回调只调用一次。
- 成功和取得正文/provenance后的失败或取消各保存一条；取得任何正文和
  provenance前取消不建空历史。
- Failed/Cancelled来源的 historyObservations只进入 finalizer，不会让下游 Input
  ready或触发模型/结果动作；Succeeded来源使用正常 values。
- voice payload中的音频路径、长录音分段、触发方式和 speechElapsedMs进入同一
  历史；运行中修改 recordDirectory后仍写入启动时冻结的目录。
- screenshot payload只把 engine/elapsed/fallback/rect写入历史请求，不复制
  QImage、blocks或临时文件。
- 历史新增成功只发布一次 HistoryChangeSet；保存失败不发布，editedText后续
  更新成功发布更新事件但不新增记录。
- `saveHistory`成功后把 detailPath留在 run编辑状态；后续结果窗编辑只通过
  `updateHistoryEditedText`更新这个路径。新增历史失败时不得借编辑回调再创建
  一条记录。
- 历史保存失败只报告 `flow_history_save_failed`，不重跑动作或经典流程。
- AutoWrite失败后不启动经典流程。

- [ ] **Step 5: 实现执行控制器**

执行控制器拥有：

- 当前 generation。
- 当前 CancellationSource。
- 冻结执行计划。
- FunctionFlowScheduler。
- 节点计时和 trace。
- 一次完成/失败/取消收尾。

公开入口和观察信号：

```cpp
class FunctionFlowExecutionController : public QObject
{
    Q_OBJECT
public:
FunctionFlowStartOutcome start(
    const QString &functionId,
    const FunctionFlowExecutionPlan &plan,
    FunctionFlowTrigger trigger,
    FunctionFlowTargetWindowHandle targetWindow
);
void cancel();
bool isRunning() const;

signals:
    void nodeExecutionChanged(FunctionFlowNodeExecutionEvent event);
    void runExecutionChanged(FunctionFlowRunExecutionEvent event);
};
```

信号必须在控制器线程发出；跨线程 completion先排队回控制器。测试用
`QSignalSpy`断言节点事件含 runId/functionId/full hash/nodeId/type/state、运行
事件成对报告 started/terminal，且旧 run的迟到回调不再发出终态覆盖事件。
应用启动和测试都要在连接 queued signal前
`qRegisterMetaType<FunctionFlowNodeExecutionEvent>()`及
`qRegisterMetaType<FunctionFlowRunExecutionEvent>()`。

- [ ] **Step 6: 运行控制器专项测试**

Expected: 同步、异步、重复回调、迟到回调、有界取消和版本冻结全部通过。

```powershell
Invoke-CodexQtTest tests\controllers\function_flow_execution_controller_tests.pro
```

### Task 6: 把空白视图升级为最小真实图场景

**Files:**
- Create: `src/ui/function_canvas_scene.h`
- Create: `src/ui/function_canvas_scene.cpp`
- Create: `src/ui/function_canvas_node_item.h`
- Create: `src/ui/function_canvas_node_item.cpp`
- Create: `src/ui/function_canvas_edge_item.h`
- Create: `src/ui/function_canvas_edge_item.cpp`
- Modify: `src/ui/function_canvas_view.h`
- Modify: `src/ui/function_canvas_view.cpp`
- Modify: `tests/ui/function_canvas_view_tests.cpp`
- Modify: `tests/ui/function_canvas_view_tests.pro`
- Create: `tests/ui/function_canvas_scene_tests.cpp`
- Create: `tests/ui/function_canvas_scene_tests.pro`
- Modify: `vocekit.pro`

- [ ] **Step 1: 写空图、装载图和端口测试**

验证：

- 空图没有节点和连线。
- 装载一个最小图后数量正确。
- 节点位置来自领域模型。
- 节点端口与端口注册表一致。
- 不允许场景创建模型中不存在的任意端口。
- 运行 overlay不改变领域图；hash不匹配时不把旧发布版状态画到草稿节点。
- hash匹配但 nodeId或nodeType不匹配的运行事件同样不得给错误节点着色。
- 视图保持 35%–300%缩放。
- viewportCenter/zoom可以无副作用读取和恢复；恢复时不制造一串用户编辑事件。

- [ ] **Step 2: 实现 `FunctionCanvasNodeItem`**

继承 `QGraphicsObject`并提供：

```cpp
QString nodeId() const;
FunctionFlowNodeType nodeType() const;
QPointF portScenePosition(const QString &portId) const;
void setRuntimeState(FunctionFlowNodeState state);
```

节点移动结束只发：

```cpp
void positionCommitted(QString nodeId, QPointF position);
```

拖动过程中不打开 Inspector，不直接写设置。

- [ ] **Step 3: 实现 `FunctionCanvasEdgeItem`**

使用贝塞尔曲线、方向箭头和较宽命中区域：

```cpp
QPainterPath FunctionCanvasEdgeItem::shape() const
{
    QPainterPathStroker stroker;
    stroker.setWidth(14.0);
    return stroker.createStroke(path());
}
```

- [ ] **Step 4: 实现场景装载和增量更新**

- 首次 setGraph可以重建。
- 节点移动、状态变化和选择不得整场重建。
- 场景不访问 AppSettingsStore。
- 场景只发 placement/connection/removal/position意图。
- `FunctionCanvasView`不再私有持有第二个通用 QGraphicsScene；editor创建
  FunctionCanvasScene并交给 view。view公开
  `viewportCenter()/zoomLevel()/restoreViewport()`和
  `viewportChanged(center, zoom)`观察信号，供独立 editor-state防抖；这些调用
  不修改 FunctionFlowGraph。

- [ ] **Step 5: 运行 offscreen UI专项测试**

Expected: 空白、装载、端口、连线、位置、选择和缩放通过。

```powershell
Invoke-CodexQtTest tests\ui\function_canvas_view_tests.pro
Invoke-CodexQtTest tests\ui\function_canvas_scene_tests.pro
```

### Task 7: 完成编辑器、撤销和无副作用草稿保存

**Files:**
- Create: `src/ui/function_canvas_palette.h`
- Create: `src/ui/function_canvas_palette.cpp`
- Create: `src/ui/function_canvas_inspector.h`
- Create: `src/ui/function_canvas_inspector.cpp`
- Create: `src/ui/function_canvas_editor.h`
- Create: `src/ui/function_canvas_editor.cpp`
- Create: `src/ui/function_flow_settings_access.h`
- Create: `src/controllers/function_flow_editor_controller.h`
- Create: `src/controllers/function_flow_editor_controller.cpp`
- Modify: `src/ui/function_command_page.h`
- Modify: `src/ui/function_command_page.cpp`
- Modify: `src/ui/hub_settings_state.h`
- Modify: `src/ui/hub_settings_state.cpp`
- Modify: `src/ui/function_command_page_access_factory.h`
- Modify: `src/ui/function_command_page_access_factory.cpp`
- Modify: `src/ui/custom_function_creation_coordinator.h`
- Modify: `src/ui/custom_function_creation_coordinator.cpp`
- Modify: `src/ui/function_management_page_access_factory.h`
- Modify: `src/ui/function_management_page_access_factory.cpp`
- Modify: `src/ui/function_pages_access_factory.h`
- Modify: `src/ui/function_pages_access_factory.cpp`
- Modify: `src/ui/function_workspace_controller.h`
- Modify: `src/ui/function_workspace_controller.cpp`
- Modify: `src/ui/hub_function_workspace_controller.h`
- Modify: `src/ui/hub_function_workspace_controller.cpp`
- Modify: `src/ui/hub_navigation_controller.h`
- Modify: `src/ui/hub_navigation_controller.cpp`
- Modify: `src/ui/hub_window.cpp`
- Modify: `src/controllers/tray_controller.h`
- Modify: `src/controllers/tray_controller.cpp`
- Modify: `src/ui/hub_refresh_coordinator_bundle.h`
- Modify: `src/ui/hub_refresh_coordinator_bundle.cpp`
- Modify: `src/app/application_events.h`
- Modify: `src/app/application_events.cpp`
- Modify: `src/app/vocekit_application_runtime.cpp`
- Create: `tests/ui/function_canvas_editor_tests.cpp`
- Create: `tests/ui/function_canvas_editor_tests.pro`
- Modify: `tests/ui/function_canvas_cleanup_tests.cpp`
- Modify: `tests/ui/function_canvas_cleanup_tests.pro`
- Modify: `tests/ui/function_command_page_header_tests.cpp`
- Modify: `tests/ui/function_command_page_header_tests.pro`
- Create: `tests/controllers/function_flow_editor_controller_tests.cpp`
- Create: `tests/controllers/function_flow_editor_controller_tests.pro`
- Modify: `tests/ui/custom_function_creation_coordinator_tests.cpp`
- Modify: `tests/ui/custom_function_creation_coordinator_tests.pro`
- Modify: `tests/ui/function_management_page_access_factory_tests.cpp`
- Modify: `tests/ui/function_management_page_access_factory_tests.pro`
- Modify: `tests/ui/function_pages_access_factory_tests.cpp`
- Modify: `tests/ui/function_pages_access_factory_tests.pro`
- Modify: `tests/ui/function_workspace_controller_tests.cpp`
- Modify: `tests/ui/function_workspace_controller_tests.pro`
- Modify: `tests/ui/hub_function_workspace_controller_tests.cpp`
- Modify: `tests/ui/hub_function_workspace_controller_tests.pro`
- Modify: `tests/ui/hub_navigation_controller_tests.cpp`
- Modify: `tests/ui/hub_navigation_controller_tests.pro`
- Modify: `tests/ui/hub_refresh_coordinator_bundle_tests.cpp`
- Modify: `tests/ui/hub_refresh_coordinator_bundle_tests.pro`
- Modify: `tests/ui/hub_settings_state_tests.cpp`
- Modify: `tests/ui/hub_settings_state_tests.pro`
- Modify: `tests/ui/hub_window_header_tests.cpp`
- Modify: `tests/ui/hub_window_header_tests.pro`
- Create: `tests/controllers/tray_controller_exit_tests.cpp`
- Create: `tests/controllers/tray_controller_exit_tests.pro`
- Modify: `vocekit.pro`

- [ ] **Step 1: 写编辑命令测试**

覆盖：

- 放置、移动、删除节点。
- 新增、删除连线。
- 修改节点名称、启用状态和类型化配置。
- 调整 Output已连接结果动作顺序。
- 删除节点同时删除关联边。
- 一次拖动只产生一个命令。
- 撤销/重做恢复完全相同的语义图。
- 修改已知节点字段仍保留该节点、关联边和图层的 retainedValues。
- viewport变化不进入语义图撤销栈。
- 非法连接不进入撤销栈。

- [ ] **Step 2: 实现 QUndoStack命令**

场景只发：

```cpp
void nodePlacementRequested(FunctionFlowNodeType type, QPointF position);
void connectionRequested(FunctionFlowEndpoint from,
                         FunctionFlowEndpoint to);
void nodeRemovalRequested(QString nodeId);
void edgeRemovalRequested(QString edgeId);
void positionCommitted(QString nodeId, QPointF position);
```

控制器先用端口注册表和 `isFunctionFlowConnectionAllowed()`做即时检查，
并拒绝重复 endpoint连接和已满输入端口，发布时再做完整图校验。

- [ ] **Step 3: 写 500ms草稿保存测试**

使用假时钟或 `QSignalSpy/QTRY_COMPARE`验证：

- 连续修改只保存一次。
- 图 dirty以 `QUndoStack::isClean()`和保存 generation为准：成功保存对应的命令
  代次后才 `setClean()`；撤销回最近保存点会取消待执行图保存，重做后重新安排。
  远端无冲突重载清空栈并设为 clean，不能维护一套会漂移的独立布尔值。
- 保存带 expectedRevision。
- 同步收到本次 `updateDraft()`产生的 revision+1事件不会误报远端冲突；先完成
  本地 savedRevision提交再合并该事件。
- local-save guard只延迟同 functionId的本次 draft事件，其它功能或更高 revision
  仍按远端更新处理。
- 版本冲突显示“草稿已在其它位置更新”，不覆盖新版本。
- dirty时收到更高远端 revision会分别保留 base/observed revision，暂停自动保存
  和发布；事件本身不能让下一次保存绕过乐观锁。
- 不 dirty时收到远端 draft事件会装载新图并重置撤销栈。
- 保存失败保持 dirty并允许重试。
- 切换 functionId、离开功能页或销毁编辑器前调用 `flushPendingSave()`；失败时保持
  当前编辑器和 dirty状态，不切换到会丢失工作副本的页面。
- 应用退出入口先调用 `flushAllPendingFlowDrafts()`；失败时允许“重试/取消退出/
  明确丢弃未保存草稿”，未经明确丢弃不得继续退出。
- `HubNavigationController`在从 function页跳到 home/tool/settings或其它页面前先
  调用窄 `canLeaveFunctionPage`回调；flush失败时 `HubPageRouter`和左侧选中态都
  保持原页面。只在 functionId内部切换时由 FunctionWorkspaceController做同样
  门控，不能只覆盖其中一种离开路径。
- 托盘“退出”和 trayResident=false时的 HubWindow关闭都调用同一个
  `requestApplicationQuit`；任何入口都不能直接连接 `QApplication::quit`绕过冲刷。
- 点击“应用流程”先同步 `flushPendingSave()`；保存失败或版本冲突时不得调用
  publish，成功时只使用刚返回的 savedRevision。
- 当前版本损坏/hash不匹配的 published先返回修复确认，用户取消不重试，确认后
  只用同一 savedRevision调用 `publish(..., true)`；未来 schema/未知节点不显示
  可覆盖确认。
- 发布请求进行中禁用重复发布/停用操作，完成后再恢复。
- 只移动 viewport/zoom走独立防抖 `updateEditorState()`，不增加 draft或
  published revision，也不把本地图作为参数写回。
- 保存成功事件把最新 flow revision同步到 HubSettingsState。
- 编辑器仍有本地未保存命令时，事件不替换图、清空撤销栈或改变
  baseDraftRevision。
- 每次图命令后调用 `analyzeDraft()`更新完整校验和三个 trigger状态；该只读调用
  不进入撤销栈、不保存、不发事件。运行事件只有四元组
  functionId/publishedHash/nodeId/nodeType匹配时才更新节点 overlay；run terminal
  事件负责清除底部“运行中”，不能靠超时猜测流程已结束。

- [ ] **Step 4: 写设置事件隔离测试**

```cpp
SettingsChangeSet draftChange;
draftChange.keys << functionFlowDraftSettingsKey();
draftChange.functionIds << QStringLiteral("custom_1");

coordinator.apply(draftChange);

QCOMPARE(runtimeReloadCount, 0);
QCOMPARE(hotkeyRegistrationCount, 0);
QCOMPARE(activeCanvasRefreshCount, 1);
```

`application_events.h/.cpp`集中定义
`functionFlowDraftSettingsKey()`、`functionFlowEditorStateSettingsKey()`、
`functionFlowPublishedSettingsKey()`和`functionFlowEnabledSettingsKey()`，
生产代码和测试都不得散落重复字符串。
再验证 published/enabled事件会刷新运行时和快捷键。
同时保留现有语义：空 keys和未知普通设置 key仍走全量/普通设置刷新。
同步更新现有 `function_canvas_cleanup_tests`：它当前明确断言
FunctionCommandPage只创建空白 `FunctionCanvasView`且不存在节点模型；接入
FunctionCanvasEditor后必须改成“页面只持有完整 editor，scene/node/edge仍不回流
到 FunctionCommandPage”的新边界断言，不能删除该回归测试逃避失败。

- [ ] **Step 5: 实现真实节点库和 Inspector**

节点库只列 1.1 中的九种节点。Inspector严格使用 2.3 中的设置：

- 麦克风设备显示“系统默认”且不可编辑。
- 不出现“保留格式”。
- OCR语言只读显示。
- 不出现 `eachInput`。
- 不出现“目标节点/目标动作”。

放置节点时只用当前 FunctionSettings/全局设置填充一次合理默认值：Model复制
当前 modelId/promptId，Voice复制录音/语音服务默认，Screenshot复制 OCR、
triggerMode和独立截图快捷键，结果节点复制当前模板/透明度。放置完成后这些值
属于草稿；后续经典设置变化不得暗改既有节点。

- [ ] **Step 6: 接入 FunctionCommandPage**

`vocekit_application_runtime.cpp`创建一个长于 HubWindow 的
`FunctionFlowPublicationService`，组装 `FunctionFlowSettingsAccess`并沿
`HubWindowAccess`、workspace/page access factory传给编辑器。
构造/销毁顺序固定为 AppSettingsStore与PromptLibraryStore、ApplicationEvents、
publication service、各 access lambda、HubSettingsState/HubWindow；access
不得捕获比页面更早销毁的局部对象。
`FunctionCommandPage`只创建编辑器并传入 functionId和窄访问接口，不处理 JSON、
发布事务或节点运行。
draft.supported=false时编辑器按 unavailableCode显示“较新版本不兼容”或
“草稿数据损坏”的只读状态，禁用编辑、自动保存和发布，且不得用空图覆盖
retainedRaw；独立存在且 enabled的合法旧 published仍可执行。
draft合法但 published为当前版本可判定的损坏/hash不匹配时，“应用流程”第一次
只显示明确修复确认，用户确认后才以 `replaceCorruptPublished=true`重试同一
expectedDraftRevision；未来 schema或未知节点 published不提供覆盖按钮，只允许
停用并提示使用较新版本处理。

- [ ] **Step 7: 把自定义功能生命周期切到事务服务**

- 创建协调器先构造不含 flow 的功能设置，再调用
  `FunctionFlowSettingsAccess::addCustomFunction`；成功后才选中新功能。
- 把 hub_settings_state.cpp中现有 `fromCustomFunction()`提取为可测试的类型化转换
  helper供创建访问层使用；`updateCustomFunction()`更新已有功能时必须先复制旧
  `FunctionFlowState`，不能用默认构造的 flow覆盖它。
- `nextCustomFunctionId()`同时扫描现有 functions和
  retainedOrphanFunctionFlows键，永不复用孤儿 custom_N。
- 管理页删除动作调用 `removeCustomFunction`，不再先改 HubSettingsState 再执行
  通用保存。
- add/remove保存失败时 HubSettingsState、functionOrder、管理页和当前选中项都
  保持原状态。
- 普通设置保存出现 `settings_function_set_stale`时重新加载状态并提示用户重试，
  不自动重放可能过期的编辑。
- Hub/Voice等普通设置生产回调全部调用 `replaceNonFlowSettingsAndSave()`；完整
  `replaceAndSave()`只提供给事务服务。

测试先给自定义功能写入非空 draft/published，再修改名称、快捷键、模型和经典
输出设置，断言 HubSettingsState内存副本及 store中的 flow逐字段不变。

- [ ] **Step 8: 运行 UI和控制器专项测试**

Expected: 编辑、撤销、保存失败、冲刷、功能生命周期事务和事件隔离全部通过；
草稿保存不重新注册快捷键。

```powershell
Invoke-CodexQtTest tests\ui\function_canvas_editor_tests.pro
Invoke-CodexQtTest tests\controllers\function_flow_editor_controller_tests.pro
Invoke-CodexQtTest tests\ui\function_canvas_cleanup_tests.pro
Invoke-CodexQtTest tests\ui\function_command_page_header_tests.pro
Invoke-CodexQtTest tests\ui\function_command_page_access_factory_tests.pro
Invoke-CodexQtTest tests\ui\custom_function_creation_coordinator_tests.pro
Invoke-CodexQtTest tests\ui\function_management_page_access_factory_tests.pro
Invoke-CodexQtTest tests\ui\function_pages_access_factory_tests.pro
Invoke-CodexQtTest tests\ui\function_workspace_controller_tests.pro
Invoke-CodexQtTest tests\ui\hub_function_workspace_controller_tests.pro
Invoke-CodexQtTest tests\ui\hub_navigation_controller_tests.pro
Invoke-CodexQtTest tests\ui\hub_refresh_coordinator_bundle_tests.pro
Invoke-CodexQtTest tests\ui\hub_settings_state_tests.pro
Invoke-CodexQtTest tests\ui\hub_window_header_tests.pro
Invoke-CodexQtTest tests\controllers\tray_controller_exit_tests.pro
```

### Task 8: 接入通用模型、选中文字和普通结果小框纵向流程

**Files:**
- Create: `src/domain/function_flow_model_message.h`
- Create: `src/domain/function_flow_model_message.cpp`
- Create: `src/controllers/function_flow_runtime_adapters.h`
- Create: `src/controllers/function_flow_runtime_adapters.cpp`
- Create: `src/controllers/function_flow_result_controller.h`
- Create: `src/controllers/function_flow_result_controller.cpp`
- Create: `src/tasks/function_flow_model_task_runner.h`
- Create: `src/tasks/function_flow_model_task_runner.cpp`
- Modify: `src/controllers/selected_text_workflow_controller.h`
- Modify: `src/controllers/selected_text_workflow_controller.cpp`
- Modify: `src/input/selected_text_reader.h`
- Modify: `src/input/selected_text_reader.cpp`
- Modify: `src/ui/result_choice_popup.h`
- Modify: `src/ui/result_choice_popup.cpp`
- Modify: `src/output/clipboard_writer.h`
- Modify: `src/output/clipboard_writer.cpp`
- Modify: `src/controllers/voice_result_presentation_controller.h`
- Modify: `src/controllers/voice_result_presentation_controller.cpp`
- Create: `tests/domain/function_flow_model_message_tests.cpp`
- Create: `tests/domain/function_flow_model_message_tests.pro`
- Create: `tests/tasks/function_flow_model_task_runner_tests.cpp`
- Create: `tests/tasks/function_flow_model_task_runner_tests.pro`
- Create: `tests/controllers/function_flow_runtime_adapters_tests.cpp`
- Create: `tests/controllers/function_flow_runtime_adapters_tests.pro`
- Create: `tests/controllers/function_flow_result_controller_tests.cpp`
- Create: `tests/controllers/function_flow_result_controller_tests.pro`
- Modify: `tests/controllers/selected_text_workflow_controller_tests.cpp`
- Modify: `tests/controllers/selected_text_workflow_controller_tests.pro`
- Modify: `tests/controllers/voice_result_presentation_controller_tests.cpp`
- Modify: `tests/controllers/voice_result_presentation_controller_tests.pro`
- Modify: `vocekit.pro`

- [ ] **Step 1: 写角色消息组装测试**

```cpp
QList<FunctionFlowValue> values;
values << value("请翻译", "用户要求", 0)
       << value("Hello", "待处理原文", 1);

QCOMPARE(
    buildFunctionFlowUserPrompt(values),
    QString::fromUtf8(
        "[用户要求]\n请翻译\n\n"
        "[待处理原文]\nHello"
    )
);
```

覆盖重复 sequence按 sourceNodeId稳定排序、空 optional值忽略、角色为空使用“输入”。

- [ ] **Step 2: 写模型适配器测试**

断言：

- promptId解析为运行开始时的 system prompt快照。
- modelId解析为当前发布图指定模型。
- 运行开始后修改提示词、模型或全局代理设置不改变当前 dependencies。
- 调用 `ModelRequestTask`而不是 `VoiceRunExecutor`。
- networkPolicy和同一个 CancellationToken向下传递。
- 返回 promptVersion、模型耗时和 executionId。
- 模型 Provider在测试中使用假实现，不访问网络。

- [ ] **Step 3: 实现并测试后台模型任务运行器**

`FunctionFlowModelTaskRunner`使用
`QtConcurrent::run`和`QFutureWatcher<ModelRequestTaskResult>`执行
`runModelProviderRequestTask()`。它必须：

- 在工作线程创建和执行模型请求，避免在画布控制器调用栈内进入网络事件循环。
- 把流式 delta通过排队信号送回模型适配器所在的控制器线程，由结果控制器更新
  `streamingResultPopupNodeId`对应的唯一预览。
- 完成时核对 runId、nodeId和 CancellationToken。
- 重复启动同一节点前取消旧 watcher对应的任务。
- 销毁时取消并屏蔽迟到结果。

测试覆盖普通完成、流式 delta、取消、销毁、重复启动，以及取消/换代后的迟到
delta和最终结果都不再更新预览。

- [ ] **Step 4: 写选中文字和目标窗口测试**

流程入口给 `SelectedTextWorkflowRequest`增加
`suppressMissingPrompt=true`。断言：

- 读取使用 `runContext.targetWindow`。
- 启动前共享验证器已拒绝 null、已销毁或 Vocekit自身窗口；防御性适配器再次遇到
  无效 target时返回 `flow_target_window_unavailable`，Provider/模拟 Ctrl+C
  调用次数为 0。
- 只返回 QString文字，不伪造富文本格式。
- 没有选中文字时不立即弹通用错误，返回空值交给 Input节点判断 required/optional。
- 经典入口继续保持原来的缺失输入提示。

现有 `selected_text_workflow_controller_tests`必须同时证明默认
`suppressMissingPrompt=false`，防止为流程加入的静默空值语义改变经典路径。
`SelectedTextReader::hasSelectionInWindow()`对 null返回 false；Windows实现先
验证/激活传入窗口并确认 `GetForegroundWindow()==target`后再查询选区，激活失败
时不发送 Ctrl+C；查询完同步恢复完整剪贴板格式。它不得调用不带目标的
`selectedTextViaUiAutomation()`后把当前结果窗选区误认为外部选区。

- [ ] **Step 5: 写普通结果小框测试**

断言：

- 使用已有 ResultChoicePopup样式、透明度和位置偏好。
- simple/detail/compare/outputOnly模板都使用
  FunctionFlowResultActionRequest中的 canonicalInput/final output，尤其 compare
  不从经典 VoiceRunContext或当前选区补数据。
- 流程模式不注册重新生成、换模型和继续追问回调。
- 流式 Model启动时只创建一个预览，delta排队更新，最终 ResultPopup复用它。
- busy预览不登记 editable surface；最终接管时只登记一次，模型失败/取消后计数
  为 0。
- displaySeconds计时只在最终接管后启动，模型生成期间不会自动关闭。
- 关闭生成中预览会请求取消当前流程；最终内容显示后关闭只关闭窗口。
- 最终 Popup稍后点击 write/replace仍只使用 run捕获的 target；句柄失效时不写
  当前前台窗口。
- 流程 Popup安装 checked回调后不调用自身直接 ClipboardWriter路径；checked
  失败时不 resolve/close，成功时才关闭。未安装回调的经典 Popup行为不变。
- ResultPopup显示成功即完成动作，不等待用户关窗。
- 展示动作本身不保存历史。
- vocabulary按钮复用现有词库修改服务并在成功后发布 VocabularyChangeSet；窗口
  不直接写词库文件。
- 统一历史尚未创建时关闭小框会缓存 pendingEditedText，创建时并入；已创建后
  按 flowRunId更新，不新增历史。
- opacity=-1时使用运行开始冻结的 inheritedResultPopupOpacity，而不是窗口真正
  显示时重新读取 HubSettingsState。

`function_flow_result_controller_tests.pro`必须使用 `QT += core gui widgets
testlib`并实际编译流程结果控制器和 ResultChoicePopup；测试用 checked writer假
回调，不能只扫描源码字符串。

`FunctionFlowResultController`只回调
`requestCancel(ExecutionId)`、
`editableSurfaceOpened(ExecutionId)`、
`editedTextCommitted(ExecutionId, QString)`和
`editableSurfaceClosed(ExecutionId)`；可编辑窗口在暴露给用户前先登记 opened，
然后才以动作成功 completion继续调度。执行控制器按 finalizer状态决定缓存文字
还是调用历史更新接口。每个 surface必须用一次性关闭门保证销毁/关闭只通知
一次，窗口不得直接调用 HistoryStore。

- [ ] **Step 6: 实现第一个真实纵向流程**

验收图：

```text
SelectionSource -> Input -> Model -> Output -> ResultPopup
```

必须由已发布图启动，模型只请求一次；生成中关闭预览可取消，最终结果不创建
第二个窗口。

- [ ] **Step 7: 运行专项测试和假 Provider集成测试**

Expected: 不访问真实模型接口，角色顺序、取消和结果展示通过。

```powershell
Invoke-CodexQtTest tests\domain\function_flow_model_message_tests.pro
Invoke-CodexQtTest tests\tasks\function_flow_model_task_runner_tests.pro
Invoke-CodexQtTest tests\controllers\function_flow_runtime_adapters_tests.pro
Invoke-CodexQtTest tests\controllers\function_flow_result_controller_tests.pro
Invoke-CodexQtTest tests\controllers\selected_text_workflow_controller_tests.pro
Invoke-CodexQtTest tests\controllers\voice_result_presentation_controller_tests.pro
```

### Task 9: 接入语音、截图瞬态上下文和触发入口

**Files:**
- Modify: `src/controllers/voice_recording_workflow_controller.h`
- Modify: `src/controllers/voice_recording_workflow_controller.cpp`
- Modify: `src/controllers/screenshot_workflow_controller.h`
- Modify: `src/controllers/screenshot_workflow_controller.cpp`
- Modify: `src/controllers/function_flow_runtime_adapters.h`
- Modify: `src/controllers/function_flow_runtime_adapters.cpp`
- Modify: `src/capture/screenshot_launcher.h`
- Modify: `src/capture/screenshot_launcher.cpp`
- Modify: `src/input/hotkey_settings_snapshot.h`
- Modify: `src/input/hotkey_settings_snapshot.cpp`
- Modify: `src/input/global_hotkeys.h`
- Modify: `src/input/global_hotkeys.cpp`
- Modify: `src/app/vocekit_application_runtime.cpp`
- Modify: `tests/controllers/voice_recording_workflow_controller_tests.cpp`
- Modify: `tests/controllers/voice_recording_workflow_controller_tests.pro`
- Modify: `tests/controllers/screenshot_workflow_controller_tests.cpp`
- Modify: `tests/controllers/screenshot_workflow_controller_tests.pro`
- Create: `tests/controllers/function_flow_input_adapters_tests.cpp`
- Create: `tests/controllers/function_flow_input_adapters_tests.pro`
- Create: `tests/capture/screenshot_launcher_target_tests.cpp`
- Create: `tests/capture/screenshot_launcher_target_tests.pro`
- Modify: `tests/input/hotkey_settings_snapshot_tests.cpp`
- Modify: `tests/input/hotkey_settings_snapshot_tests.pro`
- Modify: `vocekit.pro`

- [ ] **Step 1: 为录音控制器增加流程完成入口**

接口必须携带 runId和 completion，不改变经典 `begin/handleHotkey/handleHotkeyReleased`：

```cpp
bool beginForFlow(
    const FunctionFlowRunContext &run,
    const FunctionFlowCompiledNode &node,
    const FunctionFlowNodeCompletion &completion
);
```

验证：

- 一个流程最多一个 VoiceSource。
- 流程入口从 `node.config.voice.recording`构造 coordinator请求，不再调用读取
  当前经典 FunctionSettings/全局倒计时或提示音开关的 helper；节点放置时已复制
  一次有效值。运行中修改经典设置不改变 countdown、beep、long recording、
  segment或maximum。
- 语音 Provider ID和 effectiveNetworkPolicy来自 run.dependencies；录音完成后
  不重新读取当前全局 provider/proxy。
- beepEnabled=true但 beepPath为空/文件不存在时沿用经典 `QApplication::beep()`
  回退；测试注入提示音回调，不播放真实声音。
- hold press/release只绑定当前 runId。
- 取消停止倒计时、录音、识别和长录音队列。
- 用户明确取消返回 Cancelled并终止整条流程；正常识别为空才交给 Input处理。
- 已生成受控录音文件/分段后识别失败或取消时，把 voice payload放入
  historyObservations但不放入可调度 values。
- 识别完成只回调一次。
- 成功 completion的 FunctionFlowValue附带
  `FunctionFlowVoicePayload`，其中 sourceAudioPath、segments、
  speechElapsedMs、recordingTriggerMode和longRecording与现有
  VoiceRunSession/长录音结果一致；不得只返回文字导致流程历史丢失录音。

- [ ] **Step 2: 为截图控制器增加流程入口**

```cpp
bool beginForFlow(
    const FunctionFlowRunContext &run,
    const FunctionFlowCompiledNode &node,
    const FunctionFlowNodeCompletion &completion
);
```

成功结果构造 `FunctionFlowScreenshotPayload`：

```cpp
payload->image = capturedImage;
payload->blocks = ocrResult.blocks;
payload->recognizedText = correctedText;
payload->engine = ocrResult.engine;
payload->elapsedMs = ocrResult.elapsedMs;
payload->usedFallback = ocrResult.usedFallback;
payload->rect = capturedRect;
```

图片不写入设置、日志或历史。
流程入口使用 published节点的 ocrEngineId/timeoutMs和运行开始冻结的
effectiveNetworkPolicy，不重新读取经典 FunctionSettings中的截图入口/OCR值；
timeout既终止 Provider/辅助进程，也只完成一次节点回调。
OCR必须使用每次运行唯一的临时图片路径，并在图片保存失败、识别成功/失败、
用户取消、超时、控制器销毁和迟到回调所有路径删除；测试目录最终为空，日志和
错误 detail不包含该路径。
截图框按 Esc或用户取消返回 Cancelled并终止整条流程；OCR正常完成但没有文字
才返回空值，交给 required/optional Input处理。OCR服务错误不得伪装成空结果。
截图已经确认并进入 OCR后失败/取消时可把 screenshot payload放入
historyObservations；执行控制器只取安全元数据，图片和 blocks在 finalizer后释放。
当节点解析到 `customCloud`等会上传图片的 OCR能力时，继续复用现有逐次云端授权
确认；未获得明确同意不得上传，拒绝按用户取消处理。流程发布不能被当作永久云
上传授权。测试用假授权回调证明拒绝时 Provider调用次数为 0。

- [ ] **Step 3: 写三种触发入口测试**

覆盖：

- MainHotkey按 acquisitionSequence依次采集。
- 三种入口都在任何 Vocekit UI/截图层激活前捕获目标窗口；Launcher使用最后一个
  非 Vocekit前台窗口。
- `ScreenshotLauncher::functionTriggeredCallback`升级为同时传
  functionId和 rememberedTargetWindow；鼠标按下/菜单显示前先读取并验证当前
  外部前台窗口，QMenu动作和单功能直达分支都传同一保存值。
- 当前画像需要 SelectionSource/AutoWrite而目标为 null、已销毁或 Vocekit窗口
  时在启动前拒绝；纯 Popup/ScreenshotPanel画像仍可运行，但写入按钮不可误写
  当前新前台窗口。
- `handleScreenshotLauncherTrigger()`传入 `ScreenshotLauncher`保存的目标并生成
  `ScreenshotLauncher`枚举；不能经过
  `handleScreenshotTrigger()/screenshot:<functionId>`而被误判为
  `ScreenshotHotkey`。
- Launcher流程 NotAvailable时经典 `beginScreenshot`收到
  targetAlreadyRemembered=true，期间不再调用 captureTargetWindow。
- ScreenshotHotkey只激活 separate截图来源。
- ScreenshotLauncher只激活 launcher截图来源。
- separateAndLauncher截图来源同时出现在截图快捷键和悬浮入口画像。
- ScreenshotHotkey注册已发布节点的 separateShortcut；草稿修改不改变当前注册。
- 主快捷键、separateShortcut或其它全局快捷键冲突时发布失败。
- 已启用流程缺少某个触发画像时，只让该入口继续使用经典快捷键/Launcher配置，
  不影响其它已发布画像。
- 当前画像 required输入不可满足时发布失败。
- hold节点不是第一个来源时发布失败。
- 正在运行同一 toggle流程时再次按键先到 flow start协调器并取消；不能被现有
  `processing()`早退截住。hold释放先由 flow消费，未消费才进入经典录音释放。
- published配置为 hold但 functionId不在 `activeHoldFunctions()`时，本次运行按
  toggle处理；不得启动后等待未安装 hook的 release。

- [ ] **Step 4: 从已发布图生成热键运行画像**

`hotkey_settings_snapshot`不重新编译图，也不依赖具体
FunctionFlowPlanCache类；增加只读 provider：

```cpp
using FunctionFlowPlanProvider = std::function<
    QSharedPointer<const FunctionFlowExecutionPlan>(const QString &)
>;

GlobalHotkeySettingsSnapshot globalHotkeySnapshotFromData(
    const AppSettingsData &settings,
    const FunctionFlowPlanProvider &flowPlanProvider
);
```

应用组装传入 `planCache.plan(functionId)` lambda；测试传入假 plan。provider返回
空指针时该功能/入口严格使用经典 FunctionSettings，不允许
hotkey_settings_snapshot自行调用 compiler制造第二套缓存。
Task 9先在启动加载设置后 `rebuildAll()`以保证主工程调用签名和初始注册完整；
Task 11再把 function definitions/published/enabled的增量事件刷新接齐。

草稿保存不改变：

- useVoice
- recordingTriggerMode
- screenshotTriggerMode
- screenshotShortcut

每个入口独立选择配置：已启用 published有 ScreenshotHotkey画像时，
`screenshotShortcut`来自 `ScreenshotSource.separateShortcut`；没有该画像时
仍读取 `FunctionSettings::input.screenshotShortcut`并保留经典截图入口。
MainHotkey的 hold/toggle和 Launcher列表同理。发布或停用后才刷新上述运行快照。

发布校验只能拒绝 Vocekit设置内可确定的快捷键冲突；其它进程占用等
`RegisterHotKey`失败只能在实际注册时发现。沿用
`GlobalHotkeys::registerFromSnapshot()`的失败返回、运行日志和可见警告，不因
系统注册失败回滚已成功发布的图，也不得把失败入口伪装成已注册。主快捷键和截图
快捷键独立报告，某一个注册失败不删除另一个成功入口；ScreenshotLauncher不依赖
系统热键，继续可用。hold hook配置失败时保留现有“本次运行退化为 toggle”的明确
警告语义，流程控制器必须依据 `activeHoldFunctions()`决定是否消费 release，不能
仅凭 published节点仍写着 hold就等待永远不会到达的释放事件。

- [ ] **Step 5: 运行语音、截图和快捷键专项测试**

测试使用假录音/OCR，不打开真实麦克风或截图层；真实验收留到 Task 12。

```powershell
Invoke-CodexQtTest tests\controllers\voice_recording_workflow_controller_tests.pro
Invoke-CodexQtTest tests\controllers\screenshot_workflow_controller_tests.pro
Invoke-CodexQtTest tests\controllers\function_flow_input_adapters_tests.pro
Invoke-CodexQtTest tests\capture\screenshot_launcher_target_tests.pro
Invoke-CodexQtTest tests\input\hotkey_settings_snapshot_tests.pro
```

### Task 10: 完成多结果动作、一次历史和隐私日志

**Files:**
- Modify: `src/controllers/function_flow_result_controller.h`
- Modify: `src/controllers/function_flow_result_controller.cpp`
- Modify: `src/controllers/function_flow_execution_controller.h`
- Modify: `src/controllers/function_flow_execution_controller.cpp`
- Modify: `src/output/clipboard_writer.h`
- Modify: `src/output/clipboard_writer.cpp`
- Modify: `src/capture/screenshot_result_window.h`
- Modify: `src/capture/screenshot_result_window.cpp`
- Modify: `src/domain/history_types.h`
- Modify: `src/domain/history_types.cpp`
- Modify: `src/domain/history_record_builder.h`
- Modify: `src/domain/history_record_builder.cpp`
- Modify: `src/storage/history_record_service.h`
- Modify: `src/storage/history_record_service.cpp`
- Modify: `src/runtime_log.h`
- Modify: `src/runtime_log.cpp`
- Modify: `src/ui/history_detail_widgets.cpp`
- Modify: `src/ui/faq_panel.cpp`
- Create: `src/domain/function_flow_errors.h`
- Create: `src/domain/function_flow_errors.cpp`
- Modify: `tests/controllers/function_flow_result_controller_tests.cpp`
- Modify: `tests/controllers/function_flow_result_controller_tests.pro`
- Modify: `tests/storage/history_store_tests.cpp`
- Modify: `tests/storage/history_store_tests.pro`
- Create: `tests/runtime/function_flow_runtime_log_tests.cpp`
- Create: `tests/runtime/function_flow_runtime_log_tests.pro`
- Create: `tests/domain/function_flow_errors_tests.cpp`
- Create: `tests/domain/function_flow_errors_tests.pro`
- Modify: `vocekit.pro`

- [ ] **Step 1: 写多动作顺序和 exactly-once测试**

覆盖：

- Popup后AutoWrite按 edge.order运行。
- 最多一个AutoWrite。
- AutoWrite失败且没有显式Popup时只显示一个兜底Popup。
- 已有显式Popup时不重复显示兜底Popup。
- 兜底Popup显示成功也不把 AutoWrite节点或整次运行改成成功，不新增第二条
  trace；其编辑回写同一历史。
- 捕获的目标窗口已销毁或属于 Vocekit时返回
  `flow_target_window_unavailable`，绝不改写当前新前台窗口。
- checked writer报告目标激活失败或 SendInput事件数不足时 AutoWrite返回
  `flow_auto_write_failed`并进入同一兜底规则；不能因为旧 void API没有返回值而
  一律标记成功。经典 void调用签名和既有测试保持不变。
- `FunctionFlowResultControllerAccess`注入
  `isUsableExternalTargetWindow(handle)`和 `hasCurrentSelection(handle)`；Windows
  目标验证必须同时检查句柄非空、`IsWindow()`为真且窗口进程 ID不是当前
  Vocekit进程，选区探针只返回 bool并恢复剪贴板。控制器测试使用假回调，不在
  测试里调用真实桌面窗口。
- AutoWrite insert/replace收到本 run的 collectedSelection；运行结束时的新选区
  状态不得反向改变该参数。
- replace且 collectedSelection=false或实时选区探针为 false时
  ClipboardWriter/SendInput调用次数为 0，返回
  `flow_replace_selection_unavailable`并按配置只显示一个兜底窗；insert且
  选区仍存在时保留“移出选区后粘贴”语义，选区已丢失时不发送 VK_RIGHT。
- Popup先显示后即使用户很快编辑，后续 AutoWrite仍收到冻结 Output；窗口自己的
  write/replace按钮收到当前编辑文字。
- 任意动作失败不运行经典流程。
- 两个动作完成后历史只保存一次。
- ResultPopup或ScreenshotPanel在 finalizer前关闭时，最新
  pendingEditedText并入首次历史；finalizer后关闭时更新同一 flowRunId，两种
  顺序都只有一个记录 ID。
- 多个可编辑结果窗按提交到控制器的先后执行 last-write-wins；某个窗口未经编辑
  关闭不覆盖其它窗口已提交的文字。
- surface关闭/销毁通知恰好一次；finalizer完成但仍有窗口打开时保留该 run的
  detailPath和冻结历史目录，最后一个窗口关闭后才释放。
- 新 run开始后关闭旧 run结果窗，编辑仍只更新旧 flowRunId；旧节点 completion
  继续按 generation丢弃，两类回调不能共用同一个“旧 run全丢弃”判断。
- flowRunId更新失败只提示/记录错误，不退化为新增历史。
- 编辑请求的 detailPath越出冻结历史根目录、文件不存在或文件内 flowRunId不符
  时拒绝更新，也不得退化为扫描其它目录。
- detail JSON中的 allDetailFile/textFile/allTextFile指向根目录外时整体拒绝且
  外部哨兵文件保持不变。
- 旧历史没有 flow字段时继续正常加载，详情页不显示空流程区块。
- 流程历史写完整 64位 published hash，解析缺失字段使用向后兼容默认值。

- [ ] **Step 2: 写截图对照窗 provenance测试**

覆盖：

- ScreenshotSource经过Input、Model和Output后仍保留截图 payload。
- 没有截图 payload时 ScreenshotPanel不能发布。
- ScreenshotResultWindow的 draft关闭回调接入与 ResultPopup相同的
  `editedTextCommitted`协调器；`onLiveDraft`不得直接写历史。
- ScreenshotResultWindow安装流程 checked write/replace回调后，失败保持窗口打开
  并释放 busy状态，经典 void回调路径仍通过既有回归测试。
- ScreenshotPanel编辑后无论在 finalizer前还是后关闭，都更新同一 flowRunId，
  且使用运行开始冻结的 recordDirectory。
- ResultPopup/AutoWrite完成后不因回调捕获整份 action request而继续持有 screenshot
  QSharedPointer；只有打开的 ScreenshotPanel保留图片引用。
- 窗口关闭后释放 QImage引用。
- 历史 JSON不包含图片、Base64或屏幕像素。

本任务把 ScreenshotResultWindow源码加入既有
`function_flow_result_controller_tests.pro`，在 offscreen平台实际点击
write/replace/close按钮验证 checked回调和一次性 surface关闭门。

- [ ] **Step 3: 写逐节点 trace测试**

`HistoryRecordService`增加窄接口：

```cpp
bool updateFlowEditedText(
    const ExecutionId &runId,
    const QString &detailPath,
    const QString &editedText,
    OperationError *error
);
```

它只更新调用方提供且已存在的 mode detail；先把 detailPath规范化并确认位于该
HistoryRecordService的受管根目录内，再读取 JSON并确认 flowRunId精确匹配。
路径越界、文件不存在或 runId不符都返回错误，不能扫描其它目录或调用普通新增
接口兜底。
同一逻辑记录的 `allDetailFile`、两个可读文本和 history index必须在受管根目录
内一起更新。`allDetailFile`物理副本不是第二条记录，任一写入失败都返回明确
错误且不得创建新记录。
写入前还要逐个规范化并验证 JSON内引用的 allDetailFile/textFile/allTextFile；
任一为空时按旧记录兼容规则跳过，任一非空但越界时整体拒绝，绝不能跟随恶意
历史 JSON写到受管根目录外。
调用它的适配器必须使用该 run冻结的 recordDirectory创建
HistoryRecordService；不能在结果窗稍后关闭时使用用户刚切换的新历史目录。更新
成功后发布对应 detailPath的 HistoryChangeSet。

历史字段至少断言：

```cpp
QCOMPARE(item.value("flowPublishedRevision").toInt(), 3);
QCOMPARE(item.value("flowPublishedHash").toString(), fullHash);
QCOMPARE(item.value("flowNodeTraces").toArray().size(), 6);
QCOMPARE(item.value("input").toString(), canonicalInput);
QCOMPARE(item.value("output").toString(), finalOutput);
QVERIFY(!traceSerialized.contains("完整选中文字"));
QVERIFY(!traceSerialized.contains("中间模型结果"));
QVERIFY(!traceSerialized.contains("data:image"));
```

- [ ] **Step 4: 实现一次运行日志**

每条节点日志只记录：

```text
功能ID
发布版本和哈希
运行ID
触发入口
节点ID和类型
状态
耗时
错误码
模型ID
提示词版本
```

禁止记录完整提示词、用户正文、音频内容、截图和接口密钥。
Provider返回的 `OperationError.detail`可能包含请求/响应正文，运行日志不得直接
拼接；只记录白名单错误码、HTTP状态和经过长度限制的无正文诊断字段。

- [ ] **Step 5: 增加错误编号和 FAQ**

覆盖：

- schema不支持。
- published hash不匹配及修复确认。
- 草稿版本冲突。
- 发布图环或端口错误。
- 当前触发入口缺必需输入。
- 模型/提示词引用失效。
- 截图上下文缺失。
- 替换写入缺少选区上下文或运行时选区为空。
- 目标窗口失效。
- 自动写入失败。
- 流程取消。

集中实现 `functionFlowErrorFaqId(const QString &code)`和
`functionFlowUserMessage(const OperationError &error)`，至少覆盖 Task 2–4、9–10
定义的全部稳定错误码，包括
`flow_cancelled/flow_selection_failed/flow_voice_failed/flow_screenshot_failed/
flow_model_failed/flow_result_popup_failed/flow_screenshot_panel_failed/
flow_auto_write_failed`。UI、历史和日志不得各自维护不一致的字符串映射。

- [ ] **Step 6: 运行结果、历史、日志和 FAQ测试**

Expected: 多动作一次历史、截图隐私和错误编号全部通过。

```powershell
Invoke-CodexQtTest tests\controllers\function_flow_result_controller_tests.pro
Invoke-CodexQtTest tests\storage\history_store_tests.pro
Invoke-CodexQtTest tests\runtime\function_flow_runtime_log_tests.pro
Invoke-CodexQtTest tests\domain\function_flow_errors_tests.pro
```

### Task 11: 快捷键分流、经典兜底和设置事件接线

**Files:**
- Modify: `src/controllers/function_command_controller.h`
- Modify: `src/controllers/function_command_controller.cpp`
- Modify: `src/controllers/voice_controller.h`
- Modify: `src/controllers/voice_controller.cpp`
- Modify: `src/app/vocekit_application_runtime.cpp`
- Modify: `src/ui/hub_window.cpp`
- Modify: `src/ui/hub_refresh_coordinator_bundle.h`
- Modify: `src/ui/hub_refresh_coordinator_bundle.cpp`
- Modify: `tests/controllers/function_command_controller_tests.cpp`
- Modify: `tests/controllers/function_command_controller_tests.pro`
- Modify: `tests/controllers/voice_controller_header_tests.cpp`
- Modify: `tests/controllers/voice_controller_header_tests.pro`
- Create: `tests/controllers/function_flow_fallback_tests.cpp`
- Create: `tests/controllers/function_flow_fallback_tests.pro`
- Create: `tests/app/function_flow_settings_event_tests.cpp`
- Create: `tests/app/function_flow_settings_event_tests.pro`
- Modify: `vocekit.pro`

- [ ] **Step 1: 使用 3.9 定义的启动结果完成分流**

应用启动先 `rebuildAll()`；function definitions/published/enabled事件只对
change.functionIds调用 `rebuildFunction()`，functionIds为空才全量重建。运行
控制器取得不可变计划后再启动。

`FunctionCommandController`只在 `NotAvailable`时进入经典流程：

```cpp
FunctionFlowTriggerRequest request;
request.functionId = functionId;
request.trigger = trigger;
request.targetWindow = frozenTargetWindow;
request.classicWorkflowBusy = classicWorkflowBusy;
const FunctionFlowStartOutcome flow = startPublishedFlow(request);
if (flow == FunctionFlowStartOutcome::Started
    || flow == FunctionFlowStartOutcome::CancelledExisting
    || flow == FunctionFlowStartOutcome::Busy
    || flow == FunctionFlowStartOutcome::TargetUnavailable
    || flow == FunctionFlowStartOutcome::ConfigurationError) {
    return;
}
runClassicFunction(functionId, trigger);
```

主快捷键、截图快捷键和 Launcher必须分别传入
MainHotkey/ScreenshotHotkey/ScreenshotLauncher，且 targetWindow在任何状态窗、
截图层或 Launcher菜单动作改变前台窗口之前已经冻结。

- [ ] **Step 2: 写分流矩阵测试**

覆盖每个触发入口：

- 没有流程 -> 经典。
- 只有草稿 -> 经典。
- 已发布但停用 -> 经典。
- 已发布 schema损坏 -> 记录错误并经典。
- 当前 trigger没有执行画像 -> 经典。
- 已启用有效画像 -> 流程。
- cache刷新替换 published时，正在运行的 QSharedPointer仍保持旧 revision/hash。
- draft/editorState事件不调用 cache rebuild。
- 模型或提示词引用失效 -> 配置错误，不经典。
- 当前画像需要外部目标但句柄不可用 -> TargetUnavailable，显示“请先切换到目标
  应用”，不发送 Ctrl+C/粘贴且不经典。
- 流程开始后节点失败 -> 不经典。
- 流程执行过AutoWrite后失败 -> 不经典。
- 忙碌时重复其它快捷键 -> 不启动第二流程。
- 经典录音/截图运行中启动流程，或流程运行中启动经典入口 -> 共用
  VoiceController忙碌仲裁，不并发占用录音、截图或目标窗口。
- 非 hold入口再次触发同一 functionId/trigger -> 取消当前流程并返回
  CancelledExisting；hold释放只结束当前 VoiceSource采集，不当作第二次启动。
- `processing()==true`时再次触发同一流程仍能到达 start协调器并取消；其它
  functionId/trigger返回 Busy。测试按调用序列断言 flow callback先于经典通用
  busy早退。
- Launcher调用不会落入 ScreenshotHotkey画像；无 Launcher画像时只对 Launcher
  走经典入口，不影响同功能已发布的 ScreenshotHotkey画像。

生产装配中的“应用正在处理”查询必须返回
`classicProcessing || flowExecutionController.isRunning()`；传给
`FunctionFlowTriggerRequest::classicWorkflowBusy`的值只包含经典录音/截图/模型
占用，避免把当前 flow自身误判成外部忙碌。start协调器在判断 plan是否存在前先
检查当前 flow：同一可取消 trigger执行取消，其它 trigger/function返回 Busy，
因此 plan缺失的经典入口也不能在另一个 flow运行时穿透并发启动。

- [ ] **Step 3: 接入设置事件**

事件策略：

| key | 编辑器刷新 | VoiceController重载 | 快捷键重注册 |
|---|---:|---:|---:|
| `functionDefinitionsSettingsKey()` | 是 | 是 | 是 |
| `functionFlowDraftSettingsKey()` | 是 | 否 | 否 |
| `functionFlowEditorStateSettingsKey()` | 仅同步非 dirty视口 | 否 | 否 |
| `functionFlowPublishedSettingsKey()` | 是 | 是 | 是 |
| `functionFlowEnabledSettingsKey()` | 是 | 是 | 是 |

`HubRefreshCoordinatorBundle`必须实际读取 keys，不能忽略参数后全量刷新。
draft事件只重读对应 functionId 的流程状态；活动编辑器 dirty 时只更新远端
revision提示，不覆盖本地图。published/enabled事件在成功保存后才允许刷新
运行时缓存和快捷键。editorState事件只在本地没有视口手势或未保存图命令时
同步视口，不触碰撤销栈。
为兼容现有调用方，keys为空仍表示全量设置刷新；未知 key按现有普通设置刷新
处理，不能被流程优化路径静默丢弃。

- [ ] **Step 4: 保持原生热键过滤器轻量**

`GlobalHotkeys::nativeEventFilter()`继续只投递逻辑 ID。图加载、校验、录音、截图、模型和结果动作都在 Qt事件循环中的控制器执行。

应用运行模块把 execution controller的
`nodeExecutionChanged/runExecutionChanged`信号连接到 HubWindow的窄转发
方法；Hub只把事件交给已创建且当前 functionId匹配的
FunctionCanvasEditor。页面尚未创建时直接丢弃 UI观察事件，不能为了着色主动创建
页面。该连接不经过 SettingsChangeSet，也不能触发设置重载。

- [ ] **Step 5: 运行分流和事件专项测试**

Expected: 草稿自动保存不会触发运行时重载；所有启动结果都不会产生重复经典执行。

```powershell
Invoke-CodexQtTest tests\controllers\function_command_controller_tests.pro
Invoke-CodexQtTest tests\controllers\function_flow_fallback_tests.pro
Invoke-CodexQtTest tests\controllers\function_flow_plan_cache_tests.pro
Invoke-CodexQtTest tests\app\function_flow_settings_event_tests.pro
Invoke-CodexQtTest tests\controllers\voice_controller_header_tests.pro
```

### Task 12: 完整验证、文档和测试包

**Files:**
- Modify: `docs/DEVELOPMENT_LOG.md`
- Modify: `docs/SOURCE_STRUCTURE.md`
- Modify: `docs/AI_PROJECT_GUIDE.md`
- Modify: `docs/TESTING.md`

- [ ] **Step 1: 运行新增专项测试工程**

必须包含：

- graph/ports/runtime types
- validation/compiler
- JSON/publication
- scheduler
- execution controller
- editor controller
- canvas scene/editor
- model message/runtime adapters
- voice/screenshot trigger adapters
- result/history/log
- fallback/settings events

Expected: 每个专项工程独立通过。

- [ ] **Step 2: 运行全部测试**

```powershell
powershell -ExecutionPolicy Bypass -File scripts\run-all-tests.ps1
```

Expected:

- `Failed: 0`
- `InfrastructureFailures: 0`

- [ ] **Step 3: 编译 Debug**

```powershell
$env:PATH="D:\QQQQQT0001\Tools\mingw530_32\bin;D:\QQQQQT0001\5.9\mingw53_32\bin;$env:PATH"
qmake vocekit.pro -spec win32-g++ CONFIG+=debug
mingw32-make -j2
```

Expected: 生成 `debug\vocekit.exe`。

- [ ] **Step 4: 编译 Release并验证运行库**

```powershell
qmake vocekit.pro -spec win32-g++ CONFIG+=release
mingw32-make -j2
powershell -ExecutionPolicy Bypass -File scripts\deploy.ps1
powershell -ExecutionPolicy Bypass -File scripts\verify-runtime.ps1 -Configuration release
```

Expected: `release\vocekit.exe`和运行库验证通过。

- [ ] **Step 5: 运行静态和格式检查**

```powershell
$cppcheck = Get-Command cppcheck -ErrorAction SilentlyContinue
if ($cppcheck) {
    & $cppcheck.Source --enable=warning,performance,portability --std=c++11 --quiet src
}
$planLines = Get-Content `
    docs\superpowers\plans\2026-07-26-function-flow-canvas.md `
    -Encoding UTF8
$paths = foreach ($line in $planLines) {
    if ($line -match '^- (Create|Modify): `([^`]+)`') {
        $path = $matches[2]
        if ($path -match '^(.*)\.h/\.cpp$') {
            "$($matches[1]).h"
            "$($matches[1]).cpp"
        } else {
            $path
        }
    }
}
git diff --check -- ($paths | Sort-Object -Unique)
```

对新增未跟踪文件另外扫描尾随空白、NUL和 UTF-8解码。先用 `Get-Command`探测
cppcheck/clang-tidy/clazy；可用时运行并要求零新增问题，缺失时在
`docs/DEVELOPMENT_LOG.md`记录 `tool_unavailable`，不得写成“已通过”。工具链
头文件不兼容时记录环境原因，不为规避工具错误改动无关源码。

- [ ] **Step 6: 人工 UI检查**

检查：

1. 小窗口、最大化窗口。
2. Windows 100%、125%、150%缩放。
3. 九种节点中文名称、端口、按钮、Inspector不裁剪。
4. 拖动节点时不弹Inspector，松手后只产生一次撤销记录。
5. 连线命中区域、箭头和错误定位。
6. 普通滚轮不移动页面或画布；空白处左键拖动平移；Ctrl+滚轮围绕鼠标缩放。
7. Inspector打开后仍有足够画布。
8. 保存中、保存失败、草稿已保存、可发布、发布失败状态清晰。
9. 流程运行状态不会被草稿编辑覆盖。

- [ ] **Step 7: 真实功能验收**

分别验证：

- Selection -> Input -> Model -> Output -> ResultPopup。
- Voice -> Input -> Model -> Output -> ResultPopup。
- Voice + Selection按采集顺序进入同一模型，模型只调用一次。
- Primary Screenshot -> Input -> Model -> Output -> ScreenshotPanel。
- Separate Screenshot快捷键运行对应截图画像。
- ScreenshotLauncher运行对应截图画像。
- Launcher和独立截图快捷键在同一功能上分别命中
  ScreenshotLauncher/ScreenshotHotkey画像，互不冒充。
- 用被其它进程占用的测试快捷键验证：发布版不被回滚，失败入口有运行日志和可见
  警告，未冲突入口及 ScreenshotLauncher仍可用；hold hook不可用时按一次开始、
  再按一次结束，不会卡在等待 release。
- Model A -> Input -> Model B 串联且串行执行，直接 Model -> Model 被编辑器和发布校验拒绝。
- 一个模型结果依次进入ResultPopup和AutoWrite，只保存一次历史。
- AutoWrite replace在原选区仍存在时替换；先取消选区再等待写入时只显示兜底，
  不退化成插入。insert路径在选区已丢失时不额外移动一个字符。
- 用同时含文本/HTML/图片的剪贴板验证写入后完整恢复；500ms恢复窗口内手动复制
  新内容时，新内容保留不被旧快照覆盖；500ms内连续两次 Vocekit写入最终仍恢复
  第一次写入前的原始快照，不停留在中间文字。
- 语音流程历史保留音频路径、长录音分段和识别耗时；运行中修改历史目录不改变
  当前 run的保存目录，新增/编辑成功后历史页和首页收到刷新事件。
- 取消倒计时、录音、OCR、模型和结果生成。
- 取消后的迟到结果不改变当前节点或下一次运行。
- 运行中修改草稿和重新发布不改变当前发布版本。
- 草稿自动保存不重新注册快捷键。
- 未发布、停用、当前trigger不可用和损坏published时分别正确走经典流程。
- 模型/提示词引用失效时显示配置错误且不经典兜底。
- 流程开始后失败不执行第二遍经典流程。

- [ ] **Step 8: 生成并检查测试包**

```powershell
powershell -ExecutionPolicy Bypass -File scripts\package-test.ps1 -PackageName vocekit-canvas-test
```

Expected:

- `dist\vocekit-canvas-test\`
- `dist\vocekit-canvas-test.zip`
- 不包含 `config/secrets.json`、真实settings/prompts、日志、历史、录音、截图、开发者路径、源码或Git文件。

## 六、推荐交付批次

### 第一批：语义核心可证明

完成 Task 1–5。

验收：

- 图模型、端口、校验、触发画像、发布事务和串行调度全部由纯测试证明。
- 重复 ID不会被静默删除。
- waitAll、optional、取消和迟到回调语义固定。
- 不访问真实麦克风、截图和API。

### 第二批：完整编辑器但不接管快捷键

完成 Task 6–7。

验收：

- 空白画布、放置、拖动、连线、设置、撤销、重做。
- 草稿保存和发布。
- 草稿事件不会重载运行时或重注册快捷键。
- 经典功能运行行为不变。

### 第三批：最小真实纵向流程

完成 Task 8。

验收：

- Selection -> Input -> Model -> Output -> ResultPopup真实运行。
- 使用通用 ModelRequestTask和角色消息，不复用固定经典模板。
- 取消和版本冻结生效。

### 第四批：全部输入、输出和快捷键

完成 Task 9–11。

验收：

- 语音、截图、三种触发入口、多结果动作、一次历史和经典兜底全部完成。
- 一次只运行一个节点和一个流程。
- 不发生重复写入或失败后再次经典执行。

### 第五批：稳定性和测试包

完成 Task 12。

验收：

- 日志、历史、FAQ、全量测试、Debug/Release、运行库、UI检查和测试包全部通过。

## 七、完成标准

只有同时满足以下条件，画布才算落地：

1. 用户从空白画布搭建流程，不是查看固定卡片。
2. 所有节点都能在右侧进入真实可用设置。
3. 节点、连线和编辑器视口可保存并重开恢复。
4. 规范化不会静默删除用户节点或连线。
5. 规范化不会把非法运行数值静默钳制成可发布值。
6. 草稿错误、保存失败或版本冲突不覆盖合法published。
7. 草稿保存不改变快捷键和运行时。
8. 普通设置保存保留最新 flow；自定义功能创建/删除与流程在同一事务中完成。
9. 发布服务原子校验、编译、复制、启用和保存，相同语义重复发布幂等。
10. 每张可发布图只有一个启用 Output，多结果动作从它按 edge.order发出。
11. 每个触发入口只运行自己的已发布触发画像，Launcher不冒充截图快捷键，缺失
    画像逐入口回退经典。
12. 运行冻结发布版本、依赖快照、历史目录、目标窗口和取消令牌。
13. 第一版只支持 waitAll和串行节点执行。
14. 多输入按角色和顺序只调用模型一次。
15. 模型节点使用通用请求，不套用经典功能固定模板。
16. 截图 payload只在内存中传递，不进入设置、日志或历史；语音 payload只把
    受控录音元数据交给一次历史收尾。
17. 云端 OCR每次上传前取得明确授权，发布流程本身不构成永久授权。
18. 多结果动作按稳定顺序执行，最多一个AutoWrite。
19. 有正文或录音/截图 provenance的运行最多保存一条历史；取得任何正文和
    provenance前取消不建空历史。
20. 历史新增/编辑成功发布刷新事件，保存失败不伪报成功。
21. 流程开始后的失败和取消绝不再次运行经典逻辑。
22. 未发布、停用、损坏或当前trigger不可用时经典功能保持可用。
23. 取消在 3000ms内有界收尾，迟到回调不会污染当前或下一次运行。
24. 运行事件只有版本和节点身份匹配时才给草稿节点着色。
25. 日志、flow trace和测试包不泄露用户正文、截图、录音、提示词或密钥。
26. 100%/125%/150%缩放下中文节点、端口、按钮和 Inspector不裁剪。
27. 专项测试实际执行、全量测试、Debug、Release、运行库和人工UI检查全部通过。
28. ResultPopup、ScreenshotPanel和自动写入兜底窗的编辑只按原 runId更新同一
    历史；新 run不会吞掉旧窗口的合法关闭回调。
29. SelectionSource/AutoWrite启动前及动作执行时验证冻结的外部目标窗口，无效
    句柄绝不接收 Ctrl+C或粘贴。
30. 发布校验通过与 Windows热键实际注册状态严格区分；单个入口注册失败不回滚
    发布版，也不破坏其它入口或 Launcher。
31. 同步返回的本地 draft保存事件不会被误判为远端冲突，图保存和 editor保存
    互不携带对方的旧快照。
32. published语义 hash装载和入缓存时都复核；未来版本不被降级覆盖，当前版本
    损坏数据只有用户明确确认后才能由合法草稿原子修复。
33. AutoWrite replace只有当前 run确实采集到选区时才发送输入；缺失选区绝不
    静默退化成插入。
