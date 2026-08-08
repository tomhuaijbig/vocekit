# 功能流程画布视觉与交互精修参考实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

> **计划定位：** 本文是 `2026-07-26-function-flow-canvas.md` 的视觉与交互补充计划，不替代原计划中的图模型、发布、编译、调度、运行时和经典流程兜底契约。本文提供默认方案、实施顺序和验收边界；实施时可以根据当前源码、Qt 5.9 行为、窗口尺寸和测试结果调整具体像素、类名与拆分方式。

**Goal:** 在不改变流程语义和持久化格式的前提下，把现有功能流程画布精修为清晰、现代、稳定的中文节点编辑器，并系统解决导航冲突、坐标漂移、事件抢占、状态表达和小窗口拥挤问题。

**Architecture:** 保留 `QGraphicsView/QGraphicsScene` 和现有控制器边界。新增一个只负责颜色、尺寸、字形和中文摘要的画布视觉契约；`FunctionCanvasEditor` 只组合编辑器外壳，`FunctionCanvasView` 只处理视口，`FunctionCanvasScene` 只处理场景交互，节点和连线图形项只负责绘制与命中。草稿、发布、执行和运行时状态继续由现有领域与控制器层提供。

**Tech Stack:** Qt 5.9 Widgets、C++11、`QGraphicsView/QGraphicsScene`、`QPainter`、`QUndoStack`、qmake、MinGW 5.3 32 位、QtTest；不新增 WebEngine、QML、第三方画布框架或运行时资源下载。

---

## 零、计划使用规则

### 0.1 冲突时的优先级

实施中遇到冲突时，按以下顺序决策：

1. 数据安全、草稿/发布隔离、经典流程兜底和退出冲刷。
2. 现有图模型、端口、校验、撤销和运行时契约。
3. 本计划定义的交互硬约束。
4. 中文可读性、键盘焦点和高 DPI 不裁剪。
5. 本计划推荐的布局、颜色、圆角、阴影和间距。
6. 与参考项目外观的相似程度。

不得为了匹配截图而牺牲前四项。

### 0.2 三类要求

本文使用三种强度：

- **硬约束：** 实施完成后必须满足，不能自行变更。
- **推荐默认值：** 优先采用；若 Qt 5.9、字体、DPI 或当前布局证明不合适，可以调整并在实施记录中说明。
- **延期增强：** 不阻塞本轮完成，只有基础体验稳定后才实施。

### 0.3 允许因地制宜的范围

可以调整：

- 颜色的明度、圆角、边距、阴影强度和控件精确尺寸。
- 视觉契约最终放在新文件还是并入现有 UI 样式文件。
- Inspector 是否抽取外壳类。
- 工具栏使用画布上方紧凑条，还是作为 `QGraphicsView` 的普通子控件悬浮。
- 小窗口下侧栏使用互斥显示、覆盖显示还是自动收起。

不能自行调整：

- 普通滚轮不得同时驱动画布和外层页面。
- 所有拖放坐标必须由视口坐标通过 `mapToScene()` 转换。
- 点击节点不得启动画布平移或自动改变视角。
- 删除、移动、连接和设置修改必须继续进入现有撤销体系。
- UI 不得直接修改 JSON、发布版本或执行计划。
- 内部稳定 ID 不得替换成中文 ID。
- 画布视觉状态不得写入语义图哈希。

### 0.4 与旧计划的关系

旧计划仍是以下内容的唯一权威来源：

- 节点类型与端口注册表。
- DAG、`waitAll`、唯一 Output 和结果动作顺序。
- 草稿、防抖保存、发布事务与版本冻结。
- 快捷键触发画像、运行时、历史和经典流程兜底。
- 错误码、隐私和测试包规则。

本文只在视觉和编辑交互层增加约束。若实施中发现必须改变上述语义，停止本计划，先单独更新领域设计和测试，不能把语义变化夹带进 UI 精修。

---

## 一、当前基线

### 1.1 已存在且应复用的能力

当前源码已经具备：

- `FunctionCanvasEditor` 组合工具栏、节点库、画布、Inspector、状态和发布区。
- `FunctionCanvasView` 隐藏内部滚动条，支持空白左键平移和 `Ctrl + 滚轮` 缩放。
- `FunctionCanvasEditor::eventFilter()` 使用 `mapToScene(drop->pos())` 处理节点拖放位置。
- `FunctionCanvasScene` 增量更新节点和连线，维护临时连线。
- `FunctionCanvasNodeItem`、`FunctionCanvasEdgeItem` 分别绘制节点和连线。
- `FunctionFlowEditorController` 持有 `QUndoStack`，图区视口状态不进入图撤销栈。
- 节点端口、Inspector 字段和动作名称已向用户显示中文，内部 ID 保持稳定。

本轮应精修这些能力，不能重新实现第二套画布或第二套状态。

### 1.2 当前结构风险

实施前重点注意：

- `function_canvas_editor.cpp` 已接近 800 行，不应继续把绘制、主题和复杂响应式逻辑堆入构造函数。
- `function_canvas_inspector.cpp` 已超过 1,200 行，只做必要的视觉分组；若继续增长，优先抽取通用字段/分组辅助函数，不做无关重写。
- 节点颜色、画布颜色、端口颜色和连线颜色目前散落在多个 `.cpp` 中，后续容易不一致。
- 节点运行状态目前通过整个边框颜色表达，会和“节点类别色”“选中色”竞争。
- 节点库仍是一列外观相同的大按钮，类别和信息层级不足。
- 底部状态与发布操作占两行，紧凑窗口下容易压缩画布。
- 画布背景仍为横纵实线网格，视觉密度高于现代节点编辑器常用点阵。

### 1.3 工作树保护

当前工作树包含大量尚未提交的画布实现。实施时必须：

1. 保存开始时的 `git status --short`。
2. 只编辑本计划当前任务列出的文件。
3. 不使用 `git reset --hard`、`git checkout --` 或清理未跟踪文件。
4. 不使用通配符暂存。
5. 除非用户明确要求，不执行 commit、push 或创建分支。

---

## 二、目标设计

### 2.1 视觉方向

总体采用：

- **Sim Studio：** 浅色、留白、固定面板和克制的视觉层级。
- **React Flow UI：** 节点卡片的头部、内容、状态和选中表达。
- **Dify：** 节点摘要与右侧详细设置分离。
- **Blender/NodeGraphQt：** 类型端口、连线命中、分组和专业编辑器反馈。

不追求逐像素复制，也不复制第三方代码或资源。

### 2.2 编辑器外壳

推荐结构：

```text
┌─────────────────────────────────────────────────────────────┐
│ 放置  撤销  重做                 − 100% +  适应窗口          │
├──────────────┬──────────────────────────────┬───────────────┤
│ 节点库       │                              │ 节点设置      │
│ 搜索         │           画布               │ 基础设置      │
│ 内容来源     │                              │ 节点参数      │
│ 内容处理     │                              │ 行为设置      │
│ 结果动作     │                              │               │
├──────────────┴──────────────────────────────┴───────────────┤
│ 4 个节点 · 3 条连线  已保存           停用流程  应用流程    │
└─────────────────────────────────────────────────────────────┘
```

硬约束：

- 节点库、工具栏、Inspector 和状态栏都是视口外普通 QWidget，不能进入场景坐标系。
- 画布平移和缩放不能移动这些控件。
- Inspector 自己滚动，不能要求整个功能页向下滚动才能编辑节点。
- 状态栏合并为一行，不能恢复已移除的“快捷键不可用”底部说明行。

推荐默认值：

- 节点库展开宽度 224–248 px。
- Inspector 宽度 300–340 px。
- 工具栏高度 40–44 px。
- 状态栏高度 40–48 px。
- 面板间距 8 px；内部常用间距 4/8/12/16 px。
- 编辑器内容区域使用 `#F7F8FA` 附近的浅灰色。

小窗口适配：

- 小于约 1,000 px 可用宽度时，节点库和 Inspector 默认互斥显示。
- 若两侧面板同时显示会让画布宽度小于约 460 px，应自动关闭较早打开的面板。
- 不通过给整个功能页增加第二套滚动逻辑解决横向拥挤。

### 2.3 节点库

节点库按三组显示：

1. 内容来源：语音采集、选中文字、截图识别。
2. 内容处理：输入节点、调用大模型、输出节点。
3. 结果动作：结果小框、截图对照窗、自动写入。

每一项显示：

- 28–32 px 的类别色徽标或简单矢量字形。
- 中文节点名称。
- 一行短说明；空间不足时可以隐藏说明，但不能隐藏名称。

交互：

- 点击仍在当前视口中心放置。
- 拖动仍按实际落点放置。
- 搜索匹配中文名称和已有关键词。
- 搜索后无结果时显示“没有匹配的节点”，不能留下纯空白侧栏。
- 不引入网络图标、外部字体或来源不明的 SVG。

首轮可以用稳定中文单字徽标：

| 节点 | 徽标 |
|---|---|
| 语音采集 | 声 |
| 选中文字 | 选 |
| 截图识别 | 图 |
| 输入节点 | 入 |
| 调用大模型 | 模 |
| 输出节点 | 出 |
| 结果小框 | 显 |
| 截图对照窗 | 照 |
| 自动写入 | 写 |

该徽标是视觉实现，不进入持久化。

### 2.4 画布

硬约束：

- 不显示画布内部滚动条。
- 普通滚轮在画布上被画布消费，但不平移、不缩放，也不穿透到外层滚动区。
- `Ctrl + 滚轮` 围绕鼠标位置缩放。
- 空白处左键拖动平移。
- 节点、端口、连线处理左键时不能触发平移。
- 缩放范围继续为 35%–300%。

推荐视觉：

- 使用点阵网格代替完整横纵实线。
- 普通点间距约 24 场景单位；每 5 格可以显示稍强的参考点。
- 点阵在低缩放时降低密度或只绘制主点，避免摩尔纹。
- 画布边框保持 1 px，圆角 8 px 左右。
- 空画布不自动放节点，可以在中心显示低对比度提示：
  “从节点库拖入节点，或点击节点进行放置”。
- 一旦存在节点，空状态提示消失。

### 2.5 节点卡片

推荐默认结构：

```text
┌──────────────────────────────┐
│ [模] 调用大模型         ●    │
│      等待全部输入             │
├──────────────────────────────┤
● 文字输入          文字输出 ● │
└──────────────────────────────┘
```

节点卡片显示：

- 类别徽标。
- 用户节点名称。
- 一行中文配置摘要。
- 输入与输出端口。
- 右上角运行/错误状态指示。

推荐尺寸：

- 宽 212–232 场景单位。
- 标题区高 44–48。
- 内容最小高 52。
- 圆角 8–10。
- 端口半径 5–6，交互命中半径继续不小于 12。

视觉状态分层：

1. 类别色只用于徽标和很小的强调区域。
2. 普通边框保持中性灰。
3. 选中状态使用 2 px 蓝色外框或外环。
4. 运行状态使用右上角状态点/短标签，不再完全替换节点类别表达。
5. 禁用节点降低透明度，并显示“已停用”。
6. 失败显示红色状态标识；即使节点同时被选中，也要同时看见选中和失败。

中文摘要示例：

| 节点 | 摘要示例 |
|---|---|
| 语音采集 | 系统麦克风 · 按键说话 |
| 选中文字 | 读取当前选中文字 |
| 截图识别 | 自动识别 · 简体中文 |
| 输入节点 | 内容角色：来源 · 必需 |
| 调用大模型 | 等待全部输入 |
| 输出节点 | 整理最终结果 |
| 结果小框 | 显示结果 · 手动关闭 |
| 截图对照窗 | 显示截图与识别结果 |
| 自动写入 | 插入到当前光标位置 |

摘要只能读取 `FunctionFlowNode` 已有配置；不得访问 Provider、网络、文件或控制器。

### 2.6 端口与连线

端口类型建议：

- 文字端口：蓝青色。
- 动作端口：紫色。
- 不再单纯用“输入灰、输出蓝”表示数据类型。
- 输入输出方向继续依靠左右位置表达。

连线状态：

- 普通：中性灰，2 px 左右。
- 悬停/选中：蓝色，2.5–3 px。
- 临时连线：蓝色或端口类型色。
- 合法目标：绿色高亮环。
- 非法目标：红色高亮环，并保持临时线但禁止提交。

硬约束：

- `shape()` 宽命中区域不能随视觉线宽缩小。
- 连线提交前继续使用现有端口和图校验。
- 非法连线不能进入图模型或撤销栈。
- 删除连线必须通过场景意图和控制器撤销命令。
- 本轮不改变连线方向、端口基数或 `waitAll` 语义。

### 2.7 Inspector

Inspector 分组建议：

- 基础：节点名称、启用状态。
- 输入/来源：采集、角色、顺序、必需。
- 处理：模型、提示词、网络策略、流式输出。
- 显示/输出：模板、时长、透明度、动作顺序、写入方式。

硬约束：

- 详细表单只放 Inspector，不塞回节点卡片。
- 关闭 Inspector 不取消节点选中以外的图状态。
- Inspector 控件获得焦点时，Delete/Backspace 不能删除节点或连线。
- 设置修改继续构造完整类型化节点，并保留 `retainedValues`。
- 中文长文本在 100%、125%、150% Windows 缩放下不裁剪。

推荐视觉：

- 白色面板、分组标题、浅色分隔线。
- 标签位于控件上方，避免窄宽度下左右挤压。
- 危险或不可用状态使用内联提示，不弹出阻塞窗口。
- 面板顶部明确显示“节点设置 · 节点名称”，并提供关闭按钮。

### 2.8 工具栏与状态栏

工具栏分组：

- 编辑：放置、撤销、重做。
- 视口：缩小、比例、放大、适应窗口。

要求：

- 撤销/重做不可用时保持可辨认但降低对比度。
- 缩放比例始终与实际视口同步。
- 按钮不得因中文字体或 DPI 裁剪。
- 首轮不增加自动布局、迷你地图或复杂菜单。

状态栏一行显示：

- 左侧：节点数、连线数。
- 中间：草稿保存状态、发布状态、运行状态。
- 右侧：停用流程、应用流程。

状态文字必须简短，例如：

- “已保存”
- “有未保存修改”
- “已应用 · 版本 3”
- “运行中”
- “运行失败”

不显示内部错误码、英文状态枚举或快捷键注册诊断长句。

---

## 三、明确边界

### 3.1 本轮允许修改

- 画布相关 QWidget 布局与样式。
- 节点库的分类、空搜索状态和视觉层级。
- 画布背景绘制。
- 节点、端口、连线和临时连线绘制。
- Inspector 视觉分组与焦点保护。
- 工具栏、状态栏和小窗口适配。
- 只验证视觉/交互的 QtTest 与人工验收文档。

### 3.2 本轮禁止修改

- `FunctionFlowGraph` JSON 结构与 schemaVersion。
- 节点类型、稳定 ID、端口 ID、动作 ID。
- 连接规则、端口基数和发布校验语义。
- `waitAll`、调度器、编译器和运行顺序。
- 草稿/发布事务、图哈希和经典流程兜底。
- 语音、截图、选中文字、模型、历史和写入适配器。
- 全局导航、设置页和其它非画布页面的整体重设计。
- Qt 版本、MinGW 版本和构建系统迁移。
- 第三方图标包、字体、Web 框架或在线资源。

### 3.3 发现相邻问题时

实施时发现本轮边界外问题：

1. 先添加最小复现或记录。
2. 判断是否阻塞本轮视觉交互验收。
3. 不阻塞则记录到后续任务，不顺手修改。
4. 阻塞则暂停当前任务，单独确认边界。

---

## 四、推荐文件结构

### 4.1 新增

- `src/ui/function_canvas_visual_style.h`
  - 画布颜色、尺寸、节点类别徽标、端口类型色、运行状态色和中文摘要接口。
- `src/ui/function_canvas_visual_style.cpp`
  - 纯 UI 映射；不得依赖 QWidget、控制器、网络或持久化。
- `tests/ui/function_canvas_visual_style_tests.cpp`
  - 验证九种节点都有中文名称、徽标、类别色和非英文摘要。
- `tests/ui/function_canvas_visual_style_tests.pro`
  - 独立 QtTest 工程。

### 4.2 修改

- `src/ui/function_canvas_editor.cpp`
  - 编辑器外壳、工具栏、侧栏互斥、小窗口布局和单行状态栏。
- `src/ui/function_canvas_editor.h`
  - 仅增加布局状态和必要私有辅助函数。
- `src/ui/function_canvas_view.cpp`
  - 点阵背景、空状态和视口视觉。
- `src/ui/function_canvas_view.h`
  - 仅增加绘制所需状态；不增加图模型职责。
- `src/ui/function_canvas_palette.cpp`
  - 分类、徽标、短说明和空搜索状态。
- `src/ui/function_canvas_palette.h`
  - 暴露测试所需的最小查询接口。
- `src/ui/function_canvas_node_item.cpp`
  - 新节点卡片、摘要、状态点和类型化端口。
- `src/ui/function_canvas_node_item.h`
  - 增加 hover/视觉状态所需最小接口。
- `src/ui/function_canvas_edge_item.cpp`
  - 连线颜色、悬停和选择反馈。
- `src/ui/function_canvas_edge_item.h`
  - 增加 hover 状态，不增加模型写入。
- `src/ui/function_canvas_scene.cpp`
  - 合法/非法目标高亮、焦点保护和临时连线视觉状态。
- `src/ui/function_canvas_scene.h`
  - 增加临时交互状态枚举或查询。
- `src/ui/function_canvas_inspector.cpp`
  - 分组、标题、内联提示和可读性。
- `tests/ui/function_canvas_editor_tests.cpp`
  - 编辑器外壳、小窗口、面板和中文状态测试。
- `tests/ui/function_canvas_view_tests.cpp`
  - 点阵、滚轮、缩放与空状态测试。
- `tests/ui/function_canvas_scene_tests.cpp`
  - 连接目标反馈和焦点安全测试。
- `vocekit.pro`
  - 注册新增样式源文件和头文件。
- `docs/TESTING.md`
  - 增加多 DPI、多窗口状态人工验收矩阵。
- `docs/DEVELOPMENT_LOG.md`
  - 记录最终采用的视觉默认值与偏离本计划的原因。

### 4.3 可选拆分

只有 `FunctionCanvasEditor` 或 `FunctionCanvasInspector` 在实施中继续显著膨胀时，才考虑：

- `FunctionCanvasInspectorPanel`：只包装固定标题、关闭按钮和滚动区。
- `FunctionCanvasToolbar`：只组合视口与编辑命令按钮。

不为了追求文件数量而提前拆分。

---

## 五、实施任务

### Task 1：建立视觉契约和基线测试

**Files:**
- Create: `vocekit/src/ui/function_canvas_visual_style.h`
- Create: `vocekit/src/ui/function_canvas_visual_style.cpp`
- Create: `vocekit/tests/ui/function_canvas_visual_style_tests.cpp`
- Create: `vocekit/tests/ui/function_canvas_visual_style_tests.pro`
- Modify: `vocekit/vocekit.pro`

- [ ] **Step 1：保存当前工作树和工具链基线**

Run:

```powershell
git status --short
$env:PATH="D:\QQQQQT0001\Tools\mingw530_32\bin;D:\QQQQQT0001\5.9\mingw53_32\bin;$env:PATH"
qmake -v
mingw32-make --version
```

Expected:

- Qt 为 5.9 系列。
- MinGW 为 5.3 系列。
- 保存完整 `git status --short`，后续不得覆盖其中已有改动。

- [ ] **Step 2：先写视觉契约测试**

测试至少覆盖：

```cpp
void FunctionCanvasVisualStyleTests::allNodesHaveChinesePresentation()
{
    const FunctionFlowNodeType types[] = {
        FunctionFlowNodeType::VoiceSource,
        FunctionFlowNodeType::SelectionSource,
        FunctionFlowNodeType::ScreenshotSource,
        FunctionFlowNodeType::Input,
        FunctionFlowNodeType::Model,
        FunctionFlowNodeType::Output,
        FunctionFlowNodeType::ResultPopup,
        FunctionFlowNodeType::ScreenshotPanel,
        FunctionFlowNodeType::AutoWrite
    };
    for (FunctionFlowNodeType type : types) {
        QVERIFY(!functionCanvasNodeDisplayName(type).trimmed().isEmpty());
        QVERIFY(!functionCanvasNodeGlyph(type).trimmed().isEmpty());
        QVERIFY(functionCanvasNodeAccent(type).isValid());
    }
}
```

并验证摘要不直接返回 `text_in`、`source`、`insert`、`replace` 等内部 ID。

- [ ] **Step 3：运行测试确认红灯**

Run:

```powershell
cd vocekit/tests/ui
qmake -o Makefile.codex.flow_visual function_canvas_visual_style_tests.pro -spec win32-g++ CONFIG+=debug
mingw32-make -f Makefile.codex.flow_visual -j2
```

Expected: 因视觉契约文件或函数不存在而失败。

- [ ] **Step 4：实现最小视觉契约**

接口建议：

```cpp
QString functionCanvasNodeDisplayName(FunctionFlowNodeType type);
QString functionCanvasNodeGlyph(FunctionFlowNodeType type);
QString functionCanvasNodeSummary(const FunctionFlowNode &node);
QColor functionCanvasNodeAccent(FunctionFlowNodeType type);
QColor functionCanvasPortColor(const QString &portId);
QColor functionCanvasRuntimeColor(FunctionFlowNodeState state);
QColor functionCanvasSurfaceColor();
QColor functionCanvasPanelBorderColor();
```

实现必须是确定性的纯映射，不访问控制器、设置存储或外部资源。

- [ ] **Step 5：注册工程并执行测试**

Run:

```powershell
qmake -o Makefile.codex.flow_visual function_canvas_visual_style_tests.pro -spec win32-g++ CONFIG+=debug
mingw32-make -f Makefile.codex.flow_visual -j2
.\debug\function_canvas_visual_style_tests.exe -maxwarnings 0
```

Expected: `0 failed`，退出码 0。

### Task 2：精修编辑器外壳和单行状态栏

**Files:**
- Modify: `vocekit/src/ui/function_canvas_editor.cpp:83-223`
- Modify: `vocekit/src/ui/function_canvas_editor.h:30-101`
- Modify: `vocekit/tests/ui/function_canvas_editor_tests.cpp`

- [ ] **Step 1：添加外壳结构红灯测试**

新增断言：

- 工具栏、节点库、画布、Inspector、状态栏属于同一 `FunctionCanvasEditor`。
- 画布场景不包含工具栏和侧栏。
- 状态栏只有一行布局。
- 不存在快捷键可用性说明标签。
- 紧凑窗口内所有按钮几何都在编辑器可见区域。

- [ ] **Step 2：运行现有编辑器测试并保存基线**

Run:

```powershell
cd vocekit/tests/ui
qmake -o Makefile.codex.flow_editor function_canvas_editor_tests.pro -spec win32-g++ CONFIG+=debug
mingw32-make -f Makefile.codex.flow_editor -j2
.\debug\function_canvas_editor_tests.exe -maxwarnings 0
```

Expected: 现有测试 `0 failed`；新增结构测试先失败。

- [ ] **Step 3：重组工具栏和状态栏**

实施要求：

- 保留现有 objectName，避免无意义破坏测试。
- 工具栏按钮分为编辑组和视口组。
- 底部计数、草稿、发布、运行状态与按钮合并成一行。
- “应用流程”为主要按钮；“停用流程”为次要按钮。
- 不改变对应 signal/slot 和控制器调用。

- [ ] **Step 4：实现小窗口侧栏互斥**

建议增加私有辅助函数：

```cpp
void updateSidePanelPolicy();
void setPaletteVisible(bool visible);
void setInspectorVisible(bool visible);
```

规则：

- 宽度足够时允许两侧同时显示。
- 画布剩余宽度不足时只显示最后主动打开的侧栏。
- 调整布局不修改场景中心、缩放或图模型。

- [ ] **Step 5：运行编辑器测试**

Expected:

- 所有原有拖放、撤销、Inspector 和发布测试继续通过。
- 新外壳与紧凑窗口测试通过。

### Task 3：重做节点库的信息层级

**Files:**
- Modify: `vocekit/src/ui/function_canvas_palette.cpp:16-185`
- Modify: `vocekit/src/ui/function_canvas_palette.h`
- Modify: `vocekit/tests/ui/function_canvas_editor_tests.cpp:306-435`

- [ ] **Step 1：添加节点分类和空结果测试**

测试要求：

- 仍恰好列出九种节点。
- 三个分类都存在。
- 搜索“语音”只显示语音采集。
- 搜索不存在的文字时显示空结果提示。
- 清空搜索后恢复九项。
- 点击放置和拖放 MIME 类型保持不变。

- [ ] **Step 2：运行测试确认分类测试失败**

- [ ] **Step 3：增加类别徽标和分类标题**

使用 `functionCanvasNodeGlyph()` 和 `functionCanvasNodeAccent()`，不引入图片资源。

每个条目至少包含：

- 徽标。
- 中文名称。
- 工具提示“点击放置，或拖到画布”。

短说明可以在空间不足时隐藏。

- [ ] **Step 4：增加空搜索状态**

空结果只影响节点库显示，不改变节点集合和过滤关键字。

- [ ] **Step 5：运行编辑器测试**

Expected: 九节点、重复拖动生命周期和精确落点测试继续通过。

### Task 4：升级画布背景和视口反馈

**Files:**
- Modify: `vocekit/src/ui/function_canvas_view.cpp:19-178`
- Modify: `vocekit/src/ui/function_canvas_view.h`
- Modify: `vocekit/tests/ui/function_canvas_view_tests.cpp`

- [ ] **Step 1：增加点阵和空状态测试**

测试通过自定义 `QPaintDevice/QPaintEngine` 或截图像素采样验证：

- 背景绘制点而非完整横纵线。
- 空场景存在低对比度提示。
- 有节点后不绘制空状态提示。
- 35% 缩放时不会绘制过密网格。

- [ ] **Step 2：保持现有导航回归测试**

以下测试不能删除或放宽：

- `canvasOwnsNoVisibleScrollBarsAndPlainWheelDoesNotPan`
- `blankLeftDragPansCanvasWithoutChangingTheGraph`
- `resizingViewportDoesNotMoveExistingContent`
- `zoomRangeIsBoundedAndCanBeReset`
- `restoresViewportWithoutReportingUserChanges`

- [ ] **Step 3：实现点阵背景**

建议根据当前缩放选择步长：

```cpp
const qreal effectiveSpacing =
    zoomLevel() < 0.55 ? 120.0 : 24.0;
```

具体阈值是推荐默认值，可根据实际截图调整；不得影响场景坐标或视口状态。

- [ ] **Step 4：实现空画布提示**

提示属于视口绘制，不写入场景，不影响 `itemsBoundingRect()`、适应窗口或拖放命中。

- [ ] **Step 5：运行视口测试**

Expected: 导航、缩放、恢复、点阵和空状态全部通过。

### Task 5：升级节点卡片、中文摘要和状态层级

**Files:**
- Modify: `vocekit/src/ui/function_canvas_node_item.cpp:12-374`
- Modify: `vocekit/src/ui/function_canvas_node_item.h`
- Modify: `vocekit/tests/ui/function_canvas_editor_tests.cpp:555-575`
- Modify: `vocekit/tests/ui/function_canvas_scene_tests.cpp`

- [ ] **Step 1：添加九种节点摘要测试**

构造九种节点配置，验证：

- 名称、徽标、摘要均可绘制。
- 摘要不为空且不暴露内部英文 ID。
- 长名称在节点宽度内使用省略号。
- 中文端口标签继续存在。

- [ ] **Step 2：添加状态叠加测试**

测试 normal、selected、disabled、running、succeeded、failed：

- 选中不会覆盖失败状态标识。
- 禁用状态仍可看见节点名称和端口。
- 状态变化不修改 `FunctionFlowNode`。

- [ ] **Step 3：实现新卡片结构**

使用视觉契约统一颜色与摘要。保留：

- `ItemIsMovable`
- `ItemIsSelectable`
- `ItemIsFocusable`
- `ItemSendsGeometryChanges`
- 一次拖动只在释放时发送一次 `positionCommitted`

- [ ] **Step 4：扩大视觉层级但保持命中几何稳定**

若节点宽高改变：

- 同步 `boundsForType()`、端口位置和边路径。
- 保证端口命中半径不小于 12。
- 保证节点移动后只刷新相邻连线。

- [ ] **Step 5：运行场景和编辑器测试**

Expected: 节点位置、选择、端口、连线、增量刷新和撤销测试全部通过。

### Task 6：改进端口、连线和连接目标反馈

**Files:**
- Modify: `vocekit/src/ui/function_canvas_edge_item.cpp:14-117`
- Modify: `vocekit/src/ui/function_canvas_edge_item.h`
- Modify: `vocekit/src/ui/function_canvas_scene.cpp:257-352`
- Modify: `vocekit/src/ui/function_canvas_scene.h`
- Modify: `vocekit/tests/ui/function_canvas_scene_tests.cpp`

- [ ] **Step 1：添加临时连线状态测试**

测试：

- 开始拖线后出现临时线。
- 合法目标进入 Valid 状态。
- 非法目标进入 Invalid 状态。
- 释放到非法目标不发送 `connectionRequested`。
- `Escape` 清除临时线。

- [ ] **Step 2：添加 Delete 焦点安全测试**

验证：

- 场景有焦点且选中连线时，Delete 发送一次删除意图。
- Inspector 文本框有焦点时，Delete/Backspace 不发送场景删除意图。
- Delete 自动重复事件不导致重复删除或退出应用。

- [ ] **Step 3：增加纯瞬态连接状态**

建议：

```cpp
enum class FunctionCanvasConnectionTargetState
{
    None,
    Valid,
    Invalid
};
```

该状态只存在于 Scene/Item，不进入领域模型、JSON、图哈希或撤销栈。

- [ ] **Step 4：更新端口和连线绘制**

保留现有贝塞尔曲线和 14 px 命中宽度。只改变颜色、hover、选中和临时反馈。

- [ ] **Step 5：运行场景测试**

Expected: 合法连接、删除、同步刷新、运行时覆盖和新增反馈测试全部通过。

### Task 7：精修 Inspector 和焦点行为

**Files:**
- Modify: `vocekit/src/ui/function_canvas_inspector.cpp:140-1206`
- Modify: `vocekit/src/ui/function_canvas_inspector.h`
- Modify: `vocekit/src/ui/function_canvas_editor.cpp`
- Modify: `vocekit/tests/ui/function_canvas_editor_tests.cpp:576-792`

- [ ] **Step 1：添加分组和焦点测试**

验证：

- 基础分组始终存在。
- 不同节点只出现适用设置。
- Inspector 控件可通过键盘正常编辑。
- 输入框中 Delete/Backspace 不删除场景选择。
- 关闭 Inspector 后图和撤销栈不改变。

- [ ] **Step 2：运行现有类型化设置测试**

以下契约必须继续通过：

- 中文角色显示但发送稳定 ID。
- 中文结果动作显示但保留稳定 ID。
- 修改设置不丢 `retainedValues`。
- 透明度控件不能产生非法值。

- [ ] **Step 3：增加视觉分组**

优先使用小型辅助函数生成分组标题和分隔线，不复制每个字段的样式。

- [ ] **Step 4：处理中文长文本**

要求：

- 标签开启合理换行或设置足够最小高度。
- 组合框、按钮和数字框使用 `Microsoft YaHei UI`。
- 不用固定高度裁切两行中文提示。

- [ ] **Step 5：运行编辑器测试**

Expected: Inspector 类型、数据保留、焦点和布局测试全部通过。

### Task 8：完整回归和多状态人工验收

**Files:**
- Modify: `vocekit/docs/TESTING.md`
- Modify: `vocekit/docs/DEVELOPMENT_LOG.md`

- [ ] **Step 1：运行画布专项测试**

Run:

```powershell
cd vocekit/tests/ui

qmake -o Makefile.codex.flow_visual function_canvas_visual_style_tests.pro -spec win32-g++ CONFIG+=debug
mingw32-make -f Makefile.codex.flow_visual -j2
.\debug\function_canvas_visual_style_tests.exe -maxwarnings 0

qmake -o Makefile.codex.flow_view function_canvas_view_tests.pro -spec win32-g++ CONFIG+=debug
mingw32-make -f Makefile.codex.flow_view -j2
.\debug\function_canvas_view_tests.exe -maxwarnings 0

qmake -o Makefile.codex.flow_scene function_canvas_scene_tests.pro -spec win32-g++ CONFIG+=debug
mingw32-make -f Makefile.codex.flow_scene -j2
.\debug\function_canvas_scene_tests.exe -maxwarnings 0

qmake -o Makefile.codex.flow_editor function_canvas_editor_tests.pro -spec win32-g++ CONFIG+=debug
mingw32-make -f Makefile.codex.flow_editor -j2
.\debug\function_canvas_editor_tests.exe -maxwarnings 0
```

Expected: 每个测试程序都显示 `0 failed` 并返回退出码 0。

- [ ] **Step 2：运行领域和控制器回归**

至少执行：

- `function_flow_graph_tests`
- `function_flow_validation_tests`
- `function_flow_compiler_tests`
- `function_flow_editor_controller_tests`
- `function_flow_publication_service_tests`

Expected: UI 精修没有改变图哈希、发布、撤销或编译语义。

- [ ] **Step 3：构建主程序**

Run:

```powershell
cd vocekit
qmake -o Makefile.codex.visual vocekit.pro -spec win32-g++ CONFIG+=debug
mingw32-make -f Makefile.codex.visual -j2
```

Expected: 构建成功，新增源文件全部进入主工程。

- [ ] **Step 4：执行窗口矩阵**

人工检查：

| 窗口 | Windows 缩放 | 图状态 |
|---|---:|---|
| 约 1024×720 | 100% | 空图 |
| 约 1280×800 | 100% | 4 节点、3 连线 |
| 最大化 | 125% | 9 种节点至少各 1 个 |
| 约 1100×760 | 150% | Inspector 长文本 |
| 最大化 | 150% | 运行中、成功、失败状态 |

- [ ] **Step 5：执行交互矩阵**

必须手动确认：

1. 普通滚轮不移动外层页面或画布。
2. `Ctrl + 滚轮` 围绕鼠标缩放。
3. 空白左键拖动画布。
4. 点击节点只选中，不跳视角。
5. 拖动节点只移动节点。
6. 节点库拖放在 35%、100%、200% 缩放下落点正确。
7. 多条线可以连接到 `Many` 输入口。
8. 选中线后 Delete 只删除线，软件不退出。
9. Inspector 输入框中 Delete/Backspace 正常编辑文字。
10. 节点库和 Inspector 不跟着画布移动。

- [ ] **Step 6：记录实施偏差**

在 `DEVELOPMENT_LOG.md` 记录：

- 实际采用的节点宽度、侧栏宽度和颜色。
- 是否采用悬浮工具栏；若未采用，说明 Qt 5.9 或布局原因。
- 是否抽取 Inspector/Toolbar 类。
- 本计划中延期的项目。
- 任何涉及边界外问题的独立后续项。

- [ ] **Step 7：对工作树做范围检查**

Run:

```powershell
git status --short
git diff --check -- vocekit/src/ui vocekit/tests/ui vocekit/vocekit.pro vocekit/docs/TESTING.md vocekit/docs/DEVELOPMENT_LOG.md
```

Expected:

- 没有无关新路径。
- 没有尾随空白或补丁格式问题。
- 不自动暂存或提交。

---

## 六、推荐交付批次

### 第一批：视觉契约和编辑器外壳

包括 Task 1–2。

验收：

- 主题映射集中。
- 工具栏和状态栏不拥挤。
- 小窗口侧栏不会把画布压到不可用。
- 不改变任何图语义。

### 第二批：节点库、画布和节点卡片

包括 Task 3–5。

验收：

- 节点库分类清楚。
- 点阵画布视觉干净。
- 节点具有中文摘要、类别徽标和独立状态表达。
- 节点拖放和移动回归通过。

### 第三批：连线、Inspector 和完整验证

包括 Task 6–8。

验收：

- 合法/非法连接反馈明确。
- Delete 和输入焦点安全。
- Inspector 清晰且中文不裁剪。
- 多窗口、多 DPI 和领域回归全部通过。

每批完成后可以停下来运行和查看界面；不要求一次性实施全部批次。

---

## 七、延期增强

以下内容不阻塞本轮：

- 自动布局。
- 小地图。
- 节点分组和注释。
- 连线重路由点。
- 把节点拖到连线上自动插入。
- 复制粘贴子图。
- 深色画布主题。
- 动画流光连线。
- 自定义 SVG 图标资源。
- 画布搜索与定位节点。

只有基础交互和视觉验收稳定后，才为这些功能另写计划。

---

## 八、完成标准

本轮只有同时满足以下条件才算完成：

1. 仍使用原生 `QGraphicsView/QGraphicsScene`，没有引入 Web 画布。
2. 草稿、发布、运行时、经典兜底和图哈希语义未改变。
3. 画布没有可见内部滚动条或双滚动冲突。
4. 空白拖动、节点拖动、连线拖动三种手势互不抢占。
5. 所有拖放位置在不同缩放和平移下正确。
6. 节点库、工具栏、Inspector 和状态栏不随画布移动。
7. 九种节点都有统一中文名称、徽标、摘要和类别表达。
8. 端口和连线能清楚表达文字/动作类型与合法性。
9. 选中、禁用、运行、成功和失败状态可以同时辨认。
10. Delete 不会退出软件，不会在编辑文字时删除图元素。
11. 底部保持单行紧凑状态，不恢复被移除的长说明行。
12. 100%、125%、150% Windows 缩放下中文不裁剪。
13. 约 1024×720 的紧凑窗口仍能放置、连接和编辑节点。
14. 所有画布专项测试和相关领域回归显示 `0 failed`。
15. 主程序使用 Qt 5.9/MinGW 5.3 构建成功。
16. 实施偏差和延期项得到记录，没有把计划当作不可调整的像素规范。
