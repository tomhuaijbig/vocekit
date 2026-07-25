# vocekit 模块化架构重构设计

## 1. 目标

本次重构解决以下问题：

1. 将约 586 KB、14,100 行的 `voiceassistant.cpp` 拆分为真正独立编译的 `.h/.cpp`。
2. 将设置、历史、词库等 `.inc` 页面迁移为独立页面类。
3. 拆出语音任务控制器。
4. 拆出统一的功能执行管线。
5. 建立统一接口提供商抽象。
6. 建立统一任务取消接口。
7. 分离界面和业务逻辑。
8. 用明确的数据结构管理配置。
9. 将历史存储独立为模块。
10. 用统一事件中心刷新页面状态。

重构后的用户功能、界面操作、配置内容、历史记录和快捷键默认保持兼容。只有在修复明显缺陷或实现新模块接口确有必要时，才允许调整内部行为。

## 2. 不做的事情

- 不重写整个软件。
- 不更换 Qt 5.9、MinGW 5.3 或 C++11。
- 不改变历史记录目录结构。
- 不删除用户现有设置、提示词、词库或收藏夹。
- 不改变默认快捷键和功能名称。
- 不在一次提交中同时替换所有模块。
- 不为了拆文件引入大量只有转发作用的空壳类。

## 3. 核心原则

### 3.1 每一步都能运行

每个迁移阶段结束后都必须满足：

- Release 可以编译。
- 现有自动测试通过。
- 软件可以启动。
- 原有配置可以读取。
- 已迁移功能和未迁移功能可以同时工作。

### 3.2 先建立接口，再移动实现

历史页面当前直接访问 `HubWindow` 私有字段。如果直接把 `.inc` 改名为 `.cpp`，会出现大量编译错误和状态同步问题。

正确顺序是：

1. 先建立历史存储和页面输入接口。
2. 让旧页面改为使用这些接口。
3. 再把页面实现迁移为独立类。

设置页、词库页和语音控制也采用相同策略。

### 3.3 业务模块不知道具体页面

业务模块不能直接调用：

- `HubWindow::refresh...()`
- 页面中的标签、按钮或列表
- 页面私有成员

业务模块只：

- 返回数据。
- 返回错误。
- 发布状态事件。
- 接收取消信号。

页面收到事件后自行刷新。

## 4. 目标目录结构

```text
src/
  app/
    application_context.h/.cpp
    application_events.h/.cpp
    vocekit_application.h/.cpp

  config/
    app_settings.h/.cpp
    app_settings_data.h/.cpp
    secret_config.h/.cpp
    settings_migration.h/.cpp

  domain/
    function_definition.h/.cpp
    function_execution_types.h/.cpp
    history_types.h/.cpp
    vocabulary_types.h/.cpp

  providers/
    provider_interfaces.h
    provider_registry.h/.cpp
    network_policy_resolver.h/.cpp
    baidu_speech_provider.h/.cpp
    xfyun_speech_provider.h/.cpp
    custom_speech_provider.h/.cpp
    openai_compatible_provider.h/.cpp
    anthropic_provider.h/.cpp
    custom_ocr_provider.h/.cpp

  tasks/
    cancellation_token.h/.cpp
    speech_task_controller.h/.cpp
    function_execution_pipeline.h/.cpp
    execution_recovery_store.h/.cpp

  storage/
    history_store.h/.cpp
    vocabulary_store.h/.cpp
    prompt_store.h/.cpp

  ui/
    hub_window.h/.cpp
    pages/
      settings_page.h/.cpp
      history_page.h/.cpp
      vocabulary_page.h/.cpp
      prompts_page.h/.cpp
      diagnostics_page.h/.cpp
      ocr_page.h/.cpp
    widgets/
      floating_bar.h/.cpp
      result_choice_popup.h/.cpp
      history_row_frame.h/.cpp

  platform/
    global_hotkeys.h/.cpp
    selection_reader.h/.cpp
    clipboard_bridge.h/.cpp
    tray_controller.h/.cpp
    windows_autostart.h/.cpp

  main.cpp
  voiceassistant.h
  voiceassistant.cpp
```

迁移完成后，`voiceassistant.cpp` 只保留 `runVocekit()` 兼容入口，预计不超过 100 行。条件允许时，后续可将其完全删除并让 `main.cpp` 直接调用 `VocekitApplication`。

## 5. 明确的配置数据结构

### 5.1 功能配置

将目前散落在多个 `QMap` 和 `CustomFunctionDef` 中的配置统一为：

```cpp
struct FunctionInputSettings
{
    bool useSelection = false;
    bool useVoice = false;
    bool useScreenshot = false;
    QString screenshotTriggerMode;
    QString screenshotShortcut;
};

struct RecordingSettings
{
    QString triggerMode;
    bool longRecordingEnabled = false;
    int segmentSeconds = 55;
    int maximumMinutes = 30;
    int countdownSeconds = 0;
    bool beepEnabled = false;
    QString beepPath;
};

struct FunctionOutputSettings
{
    QString outputMode;
    QString resultTemplate;
    QStringList resultActions;
    int floatingBarSeconds = 0;
    int resultPopupSeconds = 0;
};

struct FunctionSettings
{
    QString id;
    QString name;
    bool builtIn = false;
    QString shortcut;
    QString modelId;
    QString promptId;
    FunctionInputSettings input;
    RecordingSettings recording;
    FunctionOutputSettings output;
    FunctionNetworkPolicies network;
};
```

### 5.2 全局配置

`AppSettingsData` 保存：

- 托盘和开机启动。
- 浮动条全局开关。
- 强力选中。
- 词库全局设置。
- 默认语音和 OCR 服务。
- 全局网络设置。
- 历史和日志分页数量。
- 窗口位置与大小。
- `QVector<FunctionSettings>`。

`AppSettings` 负责：

- 读取旧 JSON。
- 转换为明确的数据结构。
- 校验范围和默认值。
- 原子保存。
- 提供只读快照和针对性修改方法。

页面不直接修改内部 `QMap`。

## 6. 统一接口提供商

### 6.1 语音识别接口

```cpp
class ISpeechProvider
{
public:
    virtual ~ISpeechProvider() {}
    virtual QString id() const = 0;
    virtual ProviderCheckResult checkConfiguration() const = 0;
    virtual SpeechRecognitionResult recognize(
        const SpeechRecognitionRequest &request,
        const CancellationToken &cancellation
    ) = 0;
};
```

适配器：

- `BaiduSpeechProvider`
- `XfyunSpeechProvider`
- `CustomSpeechProvider`

### 6.2 大模型接口

```cpp
class IModelProvider
{
public:
    virtual ~IModelProvider() {}
    virtual QString id() const = 0;
    virtual ProviderCheckResult checkConfiguration() const = 0;
    virtual ModelResult complete(
        const ModelRequest &request,
        const ModelDeltaCallback &onDelta,
        const CancellationToken &cancellation
    ) = 0;
};
```

适配器：

- `OpenAiCompatibleProvider`，支持 DeepSeek、OpenAI 和自定义兼容接口。
- `AnthropicProvider`。

只有真实存在两个以上实现时才保留接口。DeepSeek、OpenAI 和兼容接口共用一个适配器，通过配置描述地址、请求头和模型名，避免复制多套 HTTP 代码。

### 6.3 OCR 接口

保留已有 `OcrManager` 的本地识别调度，云端 OCR 通过统一图片提供商接口接入。RapidOCR 和 Windows OCR 继续作为本地助手适配器，不改变隐私规则。

### 6.4 提供商注册中心

`ProviderRegistry` 负责：

- 根据稳定 ID 返回语音、大模型或 OCR 适配器。
- 根据密钥配置重新构建或刷新适配器。
- 执行单项自检。
- 拒绝未知提供商并返回清楚错误。

功能执行管线不再判断 `if provider == ...`。

## 7. 统一任务取消

### 7.1 取消接口

```cpp
class CancellationToken
{
public:
    bool isCancellationRequested() const;
    void throwIfCancellationRequested() const;
};

class CancellationSource
{
public:
    CancellationToken token() const;
    void cancel();
};
```

所有长任务接收同一个取消标记：

- 倒计时。
- 普通录音。
- 分段长录音。
- 语音识别。
- OCR。
- 流式模型请求。
- 普通模型请求。
- 历史批量导入导出。

### 7.2 任务编号

每次功能执行创建 `ExecutionId`。异步回调除检查取消外，还必须确认回调属于当前任务，防止旧任务更新新页面。

### 7.3 退出行为

程序退出时：

1. 取消当前功能任务。
2. 停止录音。
3. 中断可中断网络请求。
4. 等待受控工作线程结束。
5. 保存必要恢复现场。
6. 再销毁窗口和托盘。

## 8. 语音任务控制器

`SpeechTaskController` 只负责：

- 录音前倒计时和提示音。
- 普通切换录音。
- 按住说话。
- 分段长录音。
- 波形数据。
- 调用当前语音适配器。
- 合并分段结果。
- 生成录音元数据。
- 取消和清理。

它不负责：

- 读取选中文字。
- 调用大模型。
- 词库处理。
- 显示结果小框。
- 保存完整历史。
- 直接刷新主界面。

返回结果：

```cpp
struct SpeechTaskResult
{
    bool success = false;
    bool cancelled = false;
    QString text;
    QString error;
    QString audioPath;
    QVector<RecordingSegment> segments;
    qint64 elapsedMs = -1;
};
```

## 9. 功能执行管线

### 9.1 标准流程

```text
快捷键或页面触发
  -> 创建执行上下文和取消源
  -> 获取选中文字
  -> 可选截图和 OCR
  -> 可选语音任务
  -> 词库输入预修正
  -> 组装提示词和模型请求
  -> 流式模型调用
  -> 必要时普通请求回退
  -> 词库输出兜底修正
  -> 保存历史
  -> 发布结果事件
```

### 9.2 执行上下文

```cpp
struct FunctionExecutionRequest
{
    QString functionId;
    NativeWindowHandle targetWindow = nullptr;
    QString selectedText;
    QString textInput;
    ScreenshotInput screenshot;
};

struct FunctionExecutionResult
{
    ExecutionId id;
    QString functionId;
    QString inputText;
    QString recognizedText;
    QString outputText;
    QString error;
    QString modelId;
    QString promptVersion;
    QString audioPath;
    ExecutionTiming timing;
    bool cancelled = false;
    bool usedStreamFallback = false;
};
```

### 9.3 管线阶段

每个阶段返回数据，不操作 QWidget：

- `InputCollectionStage`
- `SpeechStage`
- `VocabularyInputStage`
- `PromptStage`
- `ModelStage`
- `VocabularyOutputStage`
- `HistoryStage`
- `PresentationDecisionStage`

第一轮实现不把每个阶段做成公开类。它们先作为 `FunctionExecutionPipeline` 的私有方法，避免产生一批浅模块。只有出现第二种实现时才建立新的接口。

## 10. 历史存储模块

`HistoryStore` 成为历史数据的唯一读写入口：

```cpp
class HistoryStore
{
public:
    HistoryQueryResult query(const HistoryQuery &query);
    HistoryRecord load(const QString &recordId);
    StoreResult append(const HistoryRecord &record);
    StoreResult update(const HistoryRecord &record);
    StoreResult remove(const QStringList &recordIds);
    StoreResult rebuildIndex();
    StoreResult importBackup(const QString &path);
    StoreResult exportRecords(const HistoryExportRequest &request);
};
```

内部负责：

- 新历史目录结构。
- 功能分类和日期目录。
- 总文本、总录音、总详细记录。
- `history_index.json`。
- 收藏夹。
- 备份、导入和导出。
- 分段识别重试后的同步更新。

页面不再扫描目录，也不自行拼接路径。

为了避免历史很多时卡顿：

- 查询支持分页。
- 搜索在索引上执行。
- 详情按需加载。
- 重建索引在后台进行。
- 返回纯数据后再由页面创建控件。

## 11. 词库存储模块

`VocabularyStore` 负责：

- 读取和原子保存词条。
- 新增、修改、删除。
- 导入和导出。
- 版本快照和回滚。
- 作用范围和匹配方式。
- 相关词条检索。
- 本地输入和输出修正。

AI 生成词条属于功能执行能力，不放进存储实现。页面向模型任务模块请求建议，用户确认后再交给 `VocabularyStore` 保存。

## 12. 统一页面状态刷新

### 12.1 事件中心

使用 Qt 信号实现 `ApplicationEvents`：

```cpp
class ApplicationEvents : public QObject
{
    Q_OBJECT

signals:
    void settingsChanged(const SettingsChangeSet &changes);
    void historyChanged(const HistoryChangeSet &changes);
    void vocabularyChanged(const VocabularyChangeSet &changes);
    void providersChanged();
    void executionStateChanged(const ExecutionState &state);
    void resultReady(const FunctionExecutionResult &result);
};
```

不引入第三方事件框架。

### 12.2 防止事件混乱

- 事件类型必须是明确结构，不能用任意字符串作为消息。
- 事件只表达已经发生的事实，不用事件执行命令。
- 页面订阅与自己相关的事件。
- 同一个保存动作只发布一次事件。
- 批量导入只发布一次汇总事件，避免刷新几千次。

## 13. 页面独立化

### 13.1 SettingsPage

输入：

- `AppSettings*`
- `ProviderRegistry*`
- `ApplicationEvents*`

职责：

- 显示设置。
- 修改设置。
- 保存密钥。
- 触发接口测试。

不负责直接刷新其它页面。

### 13.2 HistoryPage

输入：

- `HistoryStore*`
- `ApplicationEvents*`

职责：

- 查询、分页和搜索。
- 显示历史卡片和详情。
- 发起删除、收藏、导入和导出。

不负责理解历史目录结构。

### 13.3 VocabularyPage

输入：

- `VocabularyStore*`
- 词条建议回调。
- `ApplicationEvents*`

职责：

- 列表、搜索、分类。
- 增删改和导入导出。
- 显示候选建议。

不直接读写词库 JSON。

### 13.4 HubWindow

最终只负责：

- 主窗口外壳。
- 左侧功能导航。
- 顶部导航。
- 页面创建与切换。
- 窗口显示状态。

不再包含历史、词库、OCR、测试工具或设置页面的具体实现。

## 14. 应用对象组织

`ApplicationContext` 持有共享模块：

- `AppSettings`
- `ApplicationEvents`
- `ProviderRegistry`
- `HistoryStore`
- `VocabularyStore`
- `PromptStore`
- `SpeechTaskController`
- `FunctionExecutionPipeline`

`VocekitApplication` 负责创建顺序和销毁顺序：

```text
配置和存储
  -> 事件中心
  -> 提供商
  -> 任务控制器和执行管线
  -> 主窗口、浮动条和托盘
  -> 快捷键
```

共享模块不使用全局变量。

常见问题跳转不再依赖全局 `g_openFaqCallback`，改为主窗口路由接口或事件。

## 15. 迁移阶段

### 阶段 A：保护现有行为

- 增加配置、历史、词库和执行结果的核心测试。
- 给当前主要流程增加可重复冒烟测试。
- 记录当前配置 JSON 和历史 JSON 的兼容样例。

### 阶段 B：数据结构和存储

- 拆出配置结构、密钥结构和迁移逻辑。
- 建立 `HistoryStore`。
- 建立 `VocabularyStore` 和 `PromptStore`。
- 旧页面先改用新存储模块，页面暂不迁移。

### 阶段 C：提供商和取消

- 拆出统一网络请求基础能力。
- 建立语音和模型适配器。
- 建立 `ProviderRegistry`。
- 建立 `CancellationToken`。
- 旧 `VoiceController` 先改用新提供商。

### 阶段 D：任务控制

- 拆出 `SpeechTaskController`。
- 拆出 `FunctionExecutionPipeline`。
- 保留旧 `VoiceController` 作为临时协调入口。
- 管线稳定后删除旧任务实现。

### 阶段 E：页面独立化

- 迁移 `SettingsPage`。
- 迁移 `HistoryPage`。
- 迁移 `VocabularyPage`。
- 迁移 OCR、测试工具和提示词页面。
- `HubWindow` 只保留导航和页面切换。

### 阶段 F：应用启动和清理

- 建立 `ApplicationContext` 和 `VocekitApplication`。
- 拆出托盘、快捷键、选区读取和剪贴板。
- 删除 `.inc`。
- 缩小或删除 `voiceassistant.cpp`。
- 更新项目文档和构建文件。

## 16. 兼容策略

### 16.1 设置

- 新代码继续读取当前 `config/settings.json`。
- 缺失字段使用现有默认值。
- 未识别字段在迁移期间不应导致加载失败。
- 第一次保存可以写成规范化新结构，但必须先建立自动备份。
- 配置迁移失败时保留原文件，并使用默认设置启动。

### 16.2 密钥

- 继续读取当前 `config/secrets.json`。
- 不在迁移日志中输出密钥。
- 自定义大模型列表和旧单个自定义模型字段继续兼容。

### 16.3 历史

- 不改变现有目录结构和文件命名。
- 新 `HistoryStore` 必须读取现有索引和详情 JSON。
- 重建索引不删除录音和详情文件。
- 导入导出格式保持兼容。

### 16.4 词库和提示词

- 继续读取当前 JSON。
- 原有词条 ID、作用范围和匹配方式不变。
- 提示词 ID 不变。

## 17. 错误处理

所有模块统一返回：

```cpp
struct OperationError
{
    QString code;
    QString message;
    QString detail;
    bool retryable = false;
};
```

- `code` 用于日志和常见问题对应。
- `message` 用于用户可读提示。
- `detail` 只写技术摘要，不含密钥和完整隐私文本。
- `retryable` 用于决定是否允许自动回退或重试。

UI 决定如何显示错误；存储和任务模块不创建 `QMessageBox`。

## 18. 测试策略

### 18.1 模块测试

- 配置读取、迁移、校验和保存。
- 历史追加、分页、搜索、删除、收藏、导入导出和索引重建。
- 词库读写、匹配、相关词条和版本回滚。
- 提供商路由和错误转换。
- 取消前、取消中和完成后取消。
- 功能管线每个输入组合。
- 流式失败普通请求回退。

### 18.2 适配器测试

- 使用本地假服务器测试 HTTP 和流式协议。
- 使用假语音适配器测试分段识别。
- 使用假模型适配器测试模型选择和回退。
- 使用临时目录测试存储，不访问真实用户数据。

### 18.3 界面测试

- 页面能独立创建。
- 页面收到事件后刷新。
- 页面销毁后不会收到悬空回调。
- 小窗口和 125%/150% 缩放下文字不裁剪。
- 历史和词库大数据分页不卡顿。

### 18.4 每阶段发布检查

- 全部自动测试。
- Release 编译。
- SSL 冒烟测试。
- 测试包启动。
- 密钥和隐私扫描。
- 听写、翻译、问答、截图和自定义功能人工冒烟。

## 19. 回退方案

每个迁移阶段保留一个清楚的切换点：

- 新存储模块失败时，阶段内可以暂时回到旧调用。
- 新提供商适配器按提供商逐个替换，不一次替换全部。
- 新执行管线先只承接一个功能，再扩展到其它功能。
- 新页面逐页替换，未迁移页面继续由旧 `HubWindow` 显示。

完成某一阶段并经过完整回归后，才删除对应旧实现。禁止新旧实现长期同时写同一份数据。

## 20. 完成标准

本次架构重构完成时：

- `voiceassistant.cpp` 不超过 100 行，或已删除。
- `src/modules/*.inc` 全部删除。
- 设置、历史、词库、提示词、OCR 和测试工具为独立页面类。
- 语音控制和功能执行管线为独立模块。
- 语音、大模型和云 OCR 使用统一提供商注册中心。
- 所有长任务支持统一取消。
- UI 不直接读写历史、词库或接口数据。
- 配置使用明确结构体。
- 页面通过 `ApplicationEvents` 刷新。
- 当前自动测试、Release 构建、部署和测试包检查全部通过。
- 现有用户设置、密钥、提示词、词库和历史可以直接使用。
