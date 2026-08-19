# vocekit AI 项目解释文档

> 当前结构提示（2026-07-25）：旧 `src/pages/*_methods.h`、`.inc` 页面拼接和 `ApiClient` 已删除。主窗口与页面在 `src/ui/`，业务协调在 `src/controllers/`，语音和大模型接口在 `src/providers/`，配置与持久化分别在 `src/config/` 和 `src/storage/`。历史修改通过 `HistoryRecordService` 落盘，再由 `ApplicationEvents` 通知页面刷新。后续 AI 修改前应先看 `docs/SOURCE_STRUCTURE.md` 和 `vocekit.pro`，不要恢复旧路径或在界面代码中直接发送网络请求。

> Qt 6 基线提示（2026-08-17）：当前主开发目录是 `vocekit-qt6`，使用 Qt 6.11.1、MinGW 13.1 64-bit 和 C++17。文档后部保留的 Qt 5.9/C++11 内容属于历史实施记录，不再是新代码兼容要求；当前构建、测试和部署命令以本节及 `README.md` 为准。

本文档给后续 AI 或接手开发者使用。目标是让对方在拿到项目后，能先理解项目宏观结构，再理解关键代码的微观分工，从而安全地继续修改。

本文档不包含真实 API Key、录音、历史记录或开发者个人路径。

最后集中更新：2026-06-24。

当文档、截图和代码不一致时，按以下优先级判断：

1. 当前源码和 `.pro` 文件。
2. `config/*.example.json`。
3. `docs/SOURCE_STRUCTURE.md`。
4. 本文档。
5. 开发日志和历史截图。

开发日志用于解释为什么这样改，不一定代表当前最终结构；旧截图只能作为历史参考。

## AI 接手项目时先做什么

不要一拿到项目就直接修改 `voiceassistant.cpp`。建议按下面顺序建立上下文：

1. 阅读本文件，理解产品行为和代码分工。
2. 阅读 `docs/SOURCE_STRUCTURE.md`，确认当前独立模块和依赖边界。
3. 阅读 `vocekit.pro`，确认实际参与编译的源文件和 Qt 模块。
4. 用 `rg` 搜索准备修改的类名、配置键、功能 ID 或弹窗文案。
5. 修改前查看 `git status --short`，不要覆盖用户尚未提交的改动。
6. 修改配置字段时同时检查默认值、读取、保存、示例文件和设置 UI。
7. 修改完成后至少重新编译 Debug 和 Release。

推荐搜索命令：

```powershell
rg -n "AppSettingsStore|class VoiceController|class HubWindow" src
rg -n "要修改的界面文字或配置键" src config docs
rg -n "logRuntimeEvent|showAttentionWarning|showAttentionInformation" src
```

项目当前技术条件：

- 操作系统：Windows。
- UI 框架：Qt 6.11 Widgets。
- 编译器：MinGW 13.1 64 位。
- C++ 标准：C++17。
- 构建入口：qmake + mingw32-make。
- 主程序名：`vocekit.exe`。
- 软件默认可后台托盘常驻。

## 第一部分：文件解释

### 项目根目录

```text
vocekit.pro
README.md
.gitignore
src/
config/
prompts/
docs/
scripts/
.qt6-build/     本机构建产物，不提交
.qt6-deploy/    独立运行目录，不提交
dist/           测试包产物，不提交
records/        用户历史和录音，不提交
logs/           运行日志，不提交
```

### `vocekit.pro`

Qt 6 qmake 项目文件；主工程显式拒绝 Qt 5 工具链。

当前需要的 Qt 模块包括：

- `widgets`：主窗口、设置页、弹窗、按钮、列表。
- `network`：HTTP 请求，大模型接口和百度接口。
- `websockets`：讯飞语音听写 WebSocket。
- `multimedia`：麦克风录音和提示音。
- `concurrent`：部分异步辅助能力。

如果新增 Qt 模块，必须同时检查：

- `.pro` 是否加入对应模块。
- Debug/Release 是否重新编译。
- `scripts/deploy.ps1` 是否能把新增 DLL 部署进测试包。

### `src/main.cpp`

程序入口很薄，只调用：

```cpp
return runVocekit(argc, argv);
```

`runVocekit()` 只是兼容入口，真实初始化逻辑在
`src/app/vocekit_application_runtime.cpp` 的
`runVocekitApplication()`。

### `src/voiceassistant.h`

只暴露 `runVocekit()` 声明。

主窗口、页面和业务控制器都已经是独立编译的 `.h/.cpp`，不要再把实现拼回这个入口。

### `src/voiceassistant.cpp`

兼容入口，只把 `runVocekit()` 转发到独立应用运行模块。该文件必须保持
在 100 行以内，不得创建 Qt 应用、设置存储、主窗口或控制器。

### `src/app/vocekit_application_runtime.*`

应用组装模块。它创建 Qt 应用、设置存储、应用事件中心、主窗口、浮动条、
全局快捷键、语音控制器、截图入口和托盘，再把这些独立对象连接起来。

这里不应重新实现页面、历史读写、语音识别、模型请求或结果展示。新增功能
优先扩展对应的 `src/ui/`、`src/controllers/`、`src/domain/`、
`src/providers/` 或 `src/storage/` 模块，运行模块只负责创建对象和注入回调。

### `src/recording/audio_recorder_legacy.h`

包含：

- `AudioCaptureDevice`：接收 Qt Multimedia 写入的 PCM 数据并计算音量峰值。
- `AudioRecorder`：打开默认麦克风、开始和停止录音、保存 WAV、返回 PCM 数据。

主要调用者是 `VoiceController` 和测试工具里的麦克风测试。

### `src/providers/` 与网络传输层

`src/providers/` 是外部接口的正式边界。业务任务通过 `ProviderRegistry` 获取 `IModelProvider` 或 `ISpeechProvider`，Provider 再使用统一网络执行器发出请求。

- `DeepSeekModelProvider` 已独立负责 DeepSeek 请求构造、普通和流式响应、代理、取消、错误转换和接口自检。
- `OpenAiCompatibleModelProvider` 已独立负责官方 OpenAI 与多个自定义兼容接口，支持当前表单快照自检，不要求先保存密钥。
- `ClaudeModelProvider` 已独立负责 Anthropic 请求头、模型名归一化、普通和流式响应、代理、取消、错误转换和接口自检。
- `BaiduSpeechProvider` 已独立负责 AccessToken 缓存、短语音请求、响应解析、代理、取消、错误转换和接口自检。
- `XfyunSpeechProvider` 已独立负责讯飞鉴权、WebSocket 音频分片、响应解析、代理、取消、错误转换和接口自检。
- `CustomSpeechProvider` 已独立负责自定义语音请求、可选鉴权、自定义模型名、响应解析、代理、取消、错误转换和接口自检。
- `IProviderNetworkTransport` 隔离 Provider 与真实网络，测试可注入假传输。
- `IProviderWebSocketTransport` 隔离 WebSocket 连接和分片发送，讯飞测试不访问真实接口。
- 旧 `ApiClient` 已删除。新增提供商时必须在 `src/providers/` 实现。

UI 不应该直接发送 HTTP 请求，也不应直接依赖具体提供商。

### `src/ui/floating_bar.h`

包含：

- `WaveformMeter`：绘制录音波形。
- `FloatingStatusIndicator`：绘制录音、处理、成功、失败状态。
- `FloatingBar`：显示临时状态、波形和快捷操作，并记录用户拖动的位置。

### `src/ui/result_choice_popup.h`

包含 `ResultChoicePopup`。

它负责：

- 显示和编辑生成结果。
- 流式追加模型输出。
- 复制、写入、替换选中文字。
- 重新生成、换模型、继续追问、展开全文。
- 加入词库。
- 保存用户编辑后的草稿。
- 保存窗口位置和大小。

### `src/ui/settings_panel.*`

包含：

- `TabBarWheelFilter`：在设置页较窄时支持标签和滚动区域操作。
- `SettingsPanel`：嵌入主窗口的完整设置界面。

当前真正显示的设置标签为：

1. 常用设置。
2. 词库。
3. 语音录音。
4. 网络。
5. 历史记录。
6. 快捷键。
7. 接口。

文件内可能仍保留不再挂到标签栏的旧构建函数。判断功能是否实际显示，应先查看构造函数中的 `m_tabs->addTab(...)`，不能只看函数是否存在。

### `src/ui/history_page.*` 与历史页面控制器

`HistoryPage` 是独立 `QWidget`，负责搜索、功能分类标签、首次加载和加载更多、批量选择以及可见列表状态。`HistoryPageController`、`HistoryEntryActionsController`、归档和导出控制器负责把页面操作协调到独立存储服务。

页面层只负责界面、筛选、按钮和弹窗。不得直接追加、删除或改写历史详情文件，也不得在页面中重写目录扫描和索引更新逻辑。

收藏、删除、导入、分段重试和收藏夹新增完成后必须发布 `HistoryChangeSet`，由 `HubRefreshCoordinatorBundle` 交给 `ApplicationEvents`。不要只刷新当前历史页，否则首页最近记录和其他订阅者会保留旧数据。

### `src/storage/history_store.*` 与 `history_record_service.*`

历史存储分为读取实现和修改服务：

- `HistoryStore` 负责目录结构、详情扫描、索引读取、`HistoryEntry` 解析和可读文本生成。
- `HistoryRecordService` 是生产环境唯一修改入口，负责语音和 OCR 保存、收藏更新、删除、分段重试及索引重建。
- `HistoryStore` 的低层追加、删除和索引写入接口保持私有，只允许记录服务和受控实现测试访问。
- 记录服务会校验详情文件属于当前历史根目录，避免页面参数误删或误改外部文件。

新增历史修改功能时必须扩展 `HistoryRecordService`，不能重新公开 `HistoryStore` 的低层写入方法。

### `src/ui/vocabulary_page.*` 与词库页面控制器

词库页面、页面控制器和传输控制器负责：

- 词库页面和分类标签。
- 词条增删改。
- 搜索。
- JSON、CSV 和文本导入导出。
- 词库候选推荐。
- AI 填充词条。
- 词条有效性检查。

### `src/ocr/` 与 `src/ui/ocr_page.*`

图片识别由独立 OCR 模块承担：

- `ocr_types.h`：引擎、请求、结果和图片校验。
- `ocr_helper_process.*`：通过受控子进程调用本地 OCR 助手，限制输出大小、超时并支持取消。
- `ocr_manager.*`：后台单任务调度和 RapidOCR 到 Windows OCR 的自动回退。
- `ocr_cloud_client.*`：用户主动选择时调用自定义云 OCR。
- `ocr_page.*` 与 `ocr_page_controller.*`：图片选择、预览、可编辑结果、AI 后续操作和 OCR 历史保存。
- `helpers/windows_ocr/`：使用 Windows 系统 OCR 的独立 MSVC 助手。

隐私边界：

- 本地 OCR 不上传图片。
- 自定义云 OCR 必须先由用户选择并确认。
- 历史记录只保存识别文字、引擎、耗时、语言、回退状态和原文件名，不复制原图片。
- 日志不能写入图片 Base64、接口密钥或完整识别文字。

### `src/ui/history_row_frame.h`

包含 `HistoryRowFrame`。这是可点击列表卡片的基础控件，历史记录、提示词等页面会复用它。

### `src/file_utils.cpp` 和 `src/file_utils.h`

集中放置文件系统辅助能力。涉及复制目录、路径合法性、原子写入或导入导出时优先复用这里的函数，不要在每个页面重复实现文件复制逻辑。

### `src/runtime_log.cpp` 和 `src/runtime_log.h`

负责写入运行日志：

```text
logs/vocekit-YYYY-MM-DD.log
logs/last_action.txt
logs/session-last.json
logs/runtime-health.json
logs/crashes/*.dmp
logs/crashes/crash-*.json
```

日志格式大致为：

```text
时间 | 会话=UUID | 分类 | 动作 | 耗时=123ms | 详细信息
```

日志不得写入完整 API Key、Secret、用户完整隐私文本或完整请求体。

`runtime_session.*` 负责会话生命周期、重复崩溃判定和安全模式恢复标记；`runtime_crash_handler.*` 在 Windows 未处理异常或 `std::terminate` 时写入 minidump。自动安全模式只关闭全局快捷键、选中文字监控和悬浮条，主窗口仍可打开用于检查配置。崩溃元数据只允许记录版本、会话、异常码、地址、时间和转储文件名，不得记录命令行、提示词或用户正文。

### `config/secrets.example.json`

接口密钥示例文件。可以提交到仓库。

真实运行时会使用：

```text
config/secrets.json
```

真实密钥文件必须被 `.gitignore` 忽略，不能提交。

### `config/settings.example.json`

设置示例文件。可以提交到仓库。

真实运行时会使用：

```text
config/settings.json
```

真实设置包含用户快捷键、窗口位置、历史目录、功能配置等本机信息，不能提交。

### `config/prompts.example.json`

提示词库示例文件。可以提交到仓库。

真实运行时会使用：

```text
config/prompts.json
```

用户自己改过的提示词可能包含个人工作内容，不能提交。

### `config/lexicon/entries.example.json`

词库示例文件。可以提交到仓库。

真实运行时会使用：

```text
config/lexicon/entries.json
```

用户词库可能包含公司名、项目名、专有名词，不能提交。

### `prompts/`

默认提示词目录：

- `prompts/asr.txt`：听写整理默认提示词。
- `prompts/translate.txt`：翻译默认提示词。
- `prompts/qa.txt`：问答默认提示词。
- `prompts/lexicon.txt`：词库 AI 生成默认提示词。

这些是初始默认值。用户在软件里编辑后，优先使用 `config/prompts.json`。

### `docs/`

文档目录。

当前重要文档：

- `docs/TESTING.md`：测试人员使用说明。
- `docs/ITERATION_REVIEW.md`：开发复盘和踩坑记录。
- `docs/DEVELOPMENT_LOG.md`：按功能和时间记录的开发日志。
- `docs/AI_PROJECT_GUIDE.md`：给 AI 和接手开发者看的项目解释文档。

### `scripts/deploy.ps1`

部署运行库脚本。

作用：

- 调用 Qt 的 `windeployqt`。
- 复制 Qt、MinGW、OpenSSL 等运行库。
- 保证 `debug/` 或 `release/` 目录里的 exe 能在测试机运行。

新增 Qt 模块或运行库依赖后，要检查这个脚本。

### `scripts/package-test.ps1`

生成测试包脚本。

作用：

- 从发布目录生成 `dist/vocekit-test/`。
- 生成 `dist/vocekit-test.zip`。
- 放入空白配置和测试说明。
- 排除真实密钥、录音、历史、源码和构建中间文件。

分享软件给别人测试时，应使用这个脚本生成测试包，不要手动复制开发目录。

### 被忽略的本地文件

以下文件和目录不应提交：

- `1.txt`
- `config/secrets.json`
- `config/settings.json`
- `config/prompts.json`
- `config/lexicon/entries.json`
- `records/`
- `debug/`
- `release/`
- `dist/`
- `Makefile*`
- `*.exe`
- `*.dll`
- `*.pro.user`
- `.learnings/`

原因：它们可能包含真实密钥、本机路径、用户数据、录音、历史或构建产物。

## 第二部分：代码解释

### 总体启动流程

入口链路：

```text
main.cpp
  -> runVocekit()
      -> 创建 QApplication
      -> 设置中文环境和控件样式
      -> 通过 AppSettingsStore 加载或创建类型化设置
      -> 创建 FloatingBar
      -> 创建 HubWindow
      -> 创建 VoiceController
      -> 创建 TrayController
      -> 注册 GlobalHotkeys
      -> 显示主界面或托盘后台运行
```

启动行为：

- 普通双击运行：显示主界面。
- Windows 开机自启动：注册表命令会带 `--autostart`，程序只进入托盘后台，不自动显示主界面。
- 不要删除 `--autostart` 判断，否则开机后会弹出主界面，违背当前产品行为。

核心对象关系：

```text
AppSettingsStore 设置读写的唯一生产入口
AppSettingsData  运行时使用的类型化设置快照
ProviderRegistry 路由语音识别和大模型提供商
AudioRecorder    录音并保存 WAV
FloatingBar      显示录音、识别、处理状态
HubWindow        主界面和各个功能页面
VoiceController  串联快捷键、选中文字、录音、接口调用和输出
TrayController   托盘菜单和快速切换
GlobalHotkeys    注册 Windows 全局快捷键
```

### 配置中心：`AppSettingsData` 与 `AppSettingsStore`

当前状态：

- `AppSettingsStore` 由程序入口唯一持有，负责把 `AppSettingsData` 与 `config/settings.json` 互相转换。
- 页面和控制器只接收 `AppSettingsData` 快照以及保存回调，不直接打开设置文件。
- `HubSettingsState` 是主窗口编辑中的类型化副本，不是第二套文件存储。
- 旧 `AppSettings` 和 `legacy_app_settings.h` 已经移除，不要重新引入。

`AppSettingsStore` 职责：

- 从指定路径读取 `config/settings.json`。
- 文件不存在或无法读取时创建默认类型化配置。
- 将 JSON 转换成明确的 `AppSettingsData`。
- 用 `QSaveFile` 原子保存，避免设置写到一半损坏。
- `replaceAndSave()` 写入失败时恢复原内存快照。
- 返回 `OperationError`，不直接弹窗。
- 更新单个功能配置时会先通过 `normalizeFunctionSettings()` 做基础校验。

修改配置项时必须同时检查：

- `AppSettingsData` 字段和默认值。
- `app_settings_json.*` 的读取与保存映射。
- `AppSettingsStore` 的事务式保存路径。
- `config/settings.example.json` 示例配置。
- 设置 UI 是否能展示和修改。
- 当前状态是否需要显示。
- 测试包是否需要默认值。

### 密钥配置：`SecretConfig`、`SecretStore`、`loadSecrets()`、`saveSecrets()`

职责：

- 读取和保存 `config/secrets.json`。
- `SecretConfig` 保存百度、讯飞、DeepSeek、OpenAI、Claude、自定义语音、自定义 OCR 和自定义大模型的接口密钥。
- `SecretStore` 负责 `config/secrets.json` 的实际读取和保存。
- `loadSecrets()` / `saveSecrets()` 只是迁移期兼容入口，后续新模块优先直接依赖 `SecretStore`。
- 不把密钥写进源码。

修改接口密钥字段时必须同时检查：

- `SecretConfig` 结构体。
- `loadSecrets()`。
- `saveSecrets()`。
- 设置页接口 UI。
- 接口自检。
- 常见问题。
- `config/secrets.example.json`。
- `scripts/package-test.ps1` 是否生成空白字段。

### 模型定义：`modelOptions()`、`modelProvider()`、`providerModelId()`

职责：

- 定义模型下拉框可选项。
- 判断模型属于 DeepSeek、OpenAI 还是 Claude。
- 把软件内部模型名转换成接口请求需要的模型名。

新增模型时通常修改：

- `modelOptions()`。
- `modelProvider()`。
- `providerModelId()`。
- 默认模型函数 `defaultModelForFunction()`。
- 功能自定义弹窗的模型下拉框会自动使用这些选项。

注意：模型下拉框应显示官方模型名，不写夸张描述。

### 语音识别服务：`ISpeechProvider` 与 `ProviderRegistry`

职责：

- 百度语音：由 `BaiduSpeechProvider` 管理令牌和短语音请求。
- 讯飞语音：由 `XfyunSpeechProvider` 通过 `IProviderWebSocketTransport` 发送 PCM 分片。
- 自定义语音：由 `CustomSpeechProvider` 直接发送 JSON 音频请求，可选使用密钥和模型名。

新增语音服务时要改：

- 服务 ID 和标题函数。
- 设置页接口 UI。
- 在 `src/providers/` 实现新的 `ISpeechProvider`。
- 在 `ProviderRegistry` 注册并为其增加配置检查。
- 接口自检。
- 当前状态显示。
- 常见问题。
- `config/secrets.example.json`。

### 百度示例代码解析：`src/config/baidu_sample_parser.*`

公开函数：`extractBaiduCredentialsFromSampleCode()`。

用途：

- 用户可以把百度智能云示例代码粘进接口页。
- 程序自动解析 `client_id` 和 `client_secret`。
- 解析结果写入百度 API Key 和 Secret Key。

### 历史记录类型：`src/domain/history_types.*`

职责：

- `HistoryEntry` 对应历史详情 json 里的完整字段。
- `HistoryQuery` / `HistoryQueryResult` 为后续 `HistoryStore` 查询接口预留。
- `HistorySegmentRetryResult` 记录长录音单段重试结果。
- 这里只放数据结构，不做 UI、不读写文件、不打开目录。

新增历史字段时通常要同步检查：

- `HistoryEntry`。
- 历史详情 json 写入位置。
- 历史索引读写。
- 历史详情页展示。
- 导出详细记录。
- 兼容性测试夹具。

### UI 样式：`appFont()`、`cardStyle()`、`buttonStyle()`、`ToggleSwitchStyle`

职责：

- 统一字体。
- 统一卡片样式。
- 统一按钮样式。
- 把默认对勾框绘制成滑动开关。

注意：

- 中文按钮不能太矮，避免文字底部被裁剪。
- 开关不要显示难看的蓝色焦点边框。
- 不要为了局部样式到处写重复 CSS，优先复用这些函数。

### 主页功能卡片：`ModeCardFrame`

职责：

- 显示听写、翻译、问答和自定义功能。
- 支持拖拽排序。
- 双击打开功能编辑弹窗。

如果新增功能类型，要检查：

- 卡片标题。
- 快捷键显示。
- 模型显示。
- 输入方式显示。
- 展现方式显示。
- 排序保存。
- 历史筛选标签。

### 主界面：`HubWindow`

职责：

- 左侧导航。
- 主页。
- 历史记录。
- 词库。
- 提示词。
- 功能自定义。
- 测试工具。
- 设置。
- 常见问题。

重要原则：

- 设置页嵌入主界面，不再单独弹窗。
- 功能级配置放在“功能自定义”。
- 常见问题要能搜索编号和错误文本。
- 长列表要可滚动，不要让小窗口裁剪关键按钮。

### 设置面板：`SettingsPanel`

职责：

- 常用设置。
- 词库设置。
- 语音和录音设置。
- 网络设置。
- 历史记录和日志加载设置。
- 快捷键。
- 接口。

已移出设置页的内容：

- 模型选择。
- 功能方式。
- 提示词编辑。
- 自定义功能编辑。

这些统一在左侧“功能自定义”中处理。

修改设置项时要保证：

- 保存按钮或自动保存逻辑清晰。
- 小卡片可以点击查看详细解释。
- 小窗口下右侧控件仍可见。

### 功能编辑弹窗

功能编辑弹窗由 `HubWindow` 内部相关函数创建。

职责：

- 编辑内置功能和自定义功能。
- 内置功能不可删除。
- 自定义功能可新增、编辑、删除。
- 提示词选择、模型、输入方式、输出方式、显示时间和录音设置都在这里修改。

注意：

- 主页卡片双击和功能自定义页卡片双击应打开同一套弹窗。
- 输入方式开关应放在对应文字后面。
- 浮动条时间为 0 表示不显示浮动条。

### 提示词系统

相关结构和函数：

- `PromptLibraryItem`
- `PromptTargetInfo`
- `sharedPromptTargets()`
- `sharedPromptText()`
- `saveSharedPromptText()`
- `promptForFunction()`
- `promptForVocabulary()`

职责：

- 管理听写、翻译、问答、自定义功能和词库的提示词。
- 支持独立增加提示词。
- 功能可以选择某个提示词。
- 提示词锁定后不能编辑。

修改提示词相关功能时要检查：

- 左侧提示词页。
- 功能编辑弹窗提示词下拉框。
- `config/prompts.example.json`。
- 旧的 `prompts/*.txt` 默认回退逻辑。

### 词库系统

相关结构和函数：

- `VocabularyEntry`
- `VocabularySuggestion`
- `loadVocabularyEntries()`
- `saveVocabularyEntries()`
- `applyVocabularyEntries()`
- `vocabularySuggestionFromModelReply()`

职责：

- 保存专有名词、易错词、固定译名和替换规则。
- 模型输出或听写整理后可以应用词库修正。
- 支持快捷键从选中文字加入词库。
- 支持 AI 自动生成词条。

注意：

- 不允许保存无修正效果的词条。
- `source == target` 且别名为空时，应提示用户补充错词或别名。
- 用户词库可能包含隐私，不能提交真实 `entries.json`。

### 选中文字和剪贴板：`ClipboardBridge`

职责：

- 读取鼠标拖选文字。
- 粘贴结果到目标窗口。
- 替换选中文字。
- 尽量恢复用户原剪贴板。

选中文字读取顺序：

```text
普通模式：
  Windows UI Automation

强力模式：
  Windows UI Automation
  必要时再使用更强读取手段
```

注意：

- 用户明确不要默认立即模拟 `Ctrl+C`。
- 翻译默认处理鼠标拖选文字。
- 如果读取失败，提示应为“未识别到有选中文字”。

### 选中文字上下文工具栏

`SelectionContextFeature` 是独立于经典功能和功能流程画布的桌面级入口。它组合
`SelectionObserver`、`SelectionProbeRunner`、`SelectionContextCoordinator`、
`SelectionContextToolbar`、结果卡片和模型任务，但不能把窗口钩子、选区读取、模型请求或
设置持久化重新塞进 UI 类。

用户在其它应用完成鼠标选择后，功能可自动显示工具栏；键盘选择由独立开关控制。自动观察
不可用或被暂停时，仍允许使用“选中文字工具栏”全局快捷键主动读取。程序自身窗口、密码框、
被阻止的应用、长度不合格或无法安全读取的选区不得显示工具栏。强力读取是用户主动选择的
兼容选项，不能成为默认路径，并且必须尽量恢复剪贴板的原内容和格式。

内置动作包括复制、保存到词库、翻译、解释和“AI 搜索”。当前“AI 搜索”只是普通模型解答，
不访问搜索引擎、不抓取网页、也不生成可验证的网页引用；结果卡必须固定显示：

```text
未进行联网搜索，已使用普通 AI 解答
```

用户在软件内通过“设置 → 常用设置 → 选中文字工具条 → 工具条功能”编辑这五个固定动作。
每项都可修改显示名称、是否显示和顺序，也可单独恢复默认值；AI 搜索可选模型和提示词，
翻译可选模型、提示词和目标语言，解释可选模型和提示词，保存可选默认词库作用范围，复制可选
保留原文或去除首尾空白。固定动作 ID 不随显示名称变化，且至少保留一个可见动作。

复制只在本地处理当前选中文字；保存只在本地打开可编辑的词库确认界面，并预选默认作用范围，
不得直接创建规则、调用模型或发送网络请求。旧设置缺少动作字段时补齐默认值；旧模型的明确退役
ID 按模型目录迁移，未知的新模型 ID 保留并在设置中标记为不可用，未识别的未来 JSON 字段也须
原样保留，避免旧版本保存时丢失新配置。

普通模型可能调用用户配置的云端模型，因此“未联网搜索”不等于“完全离线”。第一次将选中文字
发送到模型前必须取得一次明确同意。Classic 自定义功能可出现在“更多”菜单；功能流程画布版本
目前不接入此工具栏，不能悄悄改走经典流程或声称已支持。

结果卡支持复制、固定、继续追问和安全替换。固定后最多同时保留 3 张卡片；普通未固定卡片可在
点击外部时关闭。替换前必须重新验证原窗口和原选区，验证失败只提示，禁止退化为普通粘贴。
“暂停 30 分钟 / 恢复”、阻止当前应用、打开设置等状态要即时刷新；锁屏、休眠、系统恢复、
设置窗口或词库编辑期间要取消旧探测并避免迟到结果重新弹窗。

隐私边界：日志只允许记录动作 ID、文字长度、耗时和安全错误码，不能记录选中文字、模型正文、
剪贴板正文或窗口正文。

### 录音：`AudioCaptureDevice`、`AudioRecorder`

职责：

- 打开默认麦克风。
- 采集 PCM 音频。
- 保存 WAV 文件。
- 计算峰值音量，驱动浮动条波形。

注意：

- 百度短语音识别有时长限制，当前按短录音设计。
- 录音结束后把音频交给当前选择的语音识别服务。
- 录音文件保存位置来自设置。

### 接口提供商与兼容客户端

职责：

- `ProviderRegistry` 按提供商 ID 路由语音和大模型请求。
- `NetworkRequestExecutor` 统一处理普通请求、流式请求、代理、超时和取消。
- `DeepSeekModelProvider` 直接处理 DeepSeek 普通和流式调用。
- `OpenAiCompatibleModelProvider` 直接处理 OpenAI 和任意数量的自定义兼容接口。
- `ClaudeModelProvider` 直接处理 Claude 普通和流式调用。
- `BaiduSpeechProvider` 直接处理百度令牌和短语音调用。
- `XfyunSpeechProvider` 直接处理讯飞鉴权、WebSocket 分片和识别结果。
- `CustomSpeechProvider` 直接处理自定义语音调用。

注意：

- 不要在 UI 代码里散落 HTTP 细节。
- 网络错误要转成人能理解的提示。
- TUN、透明代理、VPN 相关错误要指向常见问题。
- 不要增加余额查询等敏感接口检查。

### 全局快捷键：`GlobalHotkeys`

职责：

- 注册 Windows 全局快捷键。
- 接收 `WM_HOTKEY`。
- 把逻辑功能 ID 投递给业务控制器。

默认快捷键定义：

- `src/input/hotkey_definitions.*` 保存内置快捷键、核心功能快捷键和截图快捷键默认值。
- 不要在 `voiceassistant.cpp`、设置页或旧设置中心里重复硬编码默认快捷键。
- `src/input/hotkey_parser.*` 负责把 `QKeySequence` 转成 Windows `RegisterHotKey` 参数；改快捷键格式、冲突检测或支持新按键时优先改这里。

重要安全规则：

- 不要在 `nativeEventFilter()` 里同步执行重业务。
- 当前做法是用 `QTimer::singleShot(0, ...)` 投递回 Qt 主事件循环。
- 这样可以避免 Qt 原生事件过滤器重入导致崩溃。

### 业务控制器：`VoiceController`

职责：

- 接收快捷键。
- 读取选中文字。
- 控制录音准备、倒计时、提示音和录音停止。
- 调用语音识别。
- 调用大模型。
- 应用词库。
- 保存历史。
- 触发自动写入或结果小框。

核心流程：

```text
handleHotkey(id)
  -> 如果是打开主界面，显示 Hub
  -> 如果是加入词库，读取选中文字并加入词库
  -> 如果正在处理，提示等待
  -> 如果正在倒计时，再按同一快捷键取消
  -> 如果正在录音，再按同一快捷键停止并处理
  -> 按功能设置读取选中文字
  -> 如果不使用语音，直接调用模型处理选中文字
  -> 如果使用语音，检查当前语音服务密钥
  -> 开始倒计时或录音
```

录音处理流程：

```text
stopAndProcess()
  -> 停止录音
  -> 调用当前语音识别服务
  -> 根据功能调用听写整理、翻译、问答或自定义处理
  -> 应用词库
  -> 保存历史
  -> 自动写入或弹出结果小框
```

注意：

- `m_processing` 用于防止重复触发。
- 处理期间不要再次进入模型调用。
- 如果新增长耗时操作，也要考虑是否需要处理锁。

### 浮动条：`FloatingBar`

职责：

- 显示当前状态：录音、处理中、完成、错误。
- 显示波形。
- 显示复制、撤销、重试等快捷按钮。
- 只在需要时出现。
- 支持拖动位置记忆。

注意：

- 用户不希望常驻挡住屏幕。
- 设置中关闭浮动条或某功能浮动条时间为 0 时，应不显示。

### 结果小框：`ResultChoicePopup`

职责：

- 展示模型输出。
- 支持用户编辑结果。
- 支持复制、写入、替换选中、关闭。
- 支持重新生成、换模型、继续追问、展开全文。
- 支持保存草稿。
- 支持加入词库。
- 支持位置和大小记忆。

注意：

- 结果文字不能被底部按钮裁剪。
- 文本编辑区需要足够高度。
- 普通弹窗不保留没有实际作用的标题栏问号。
- 新弹窗优先继承 `AppDialog`，不要直接使用裸 `QDialog`。

### 历史记录

相关逻辑分散在：

- 历史路径 helper。
- `HubWindow` 历史页面。
- `VoiceController::saveHistory()`。

保存结构：

```text
历史记录根目录/
  听写/
    2026-06-12/
      文本记录/
      录音记录/
      详细记录/
  翻译/
  问答/
  自定义功能名称/
  备份文件/
  总录音文件/
  总文本文件/
  总详细记录文件/
```

注意：

- 当前测试版不兼容旧结构。
- 不要再写旧结构兼容逻辑。
- 历史记录列表要分页加载，避免卡顿。
- 删除、导入、导出、备份都要适配新结构。

### 日志页面

日志页读取 `logs/*.log`，按文件时间和文件内行顺序组合成“最新记录在前”的列表。

加载策略：

- 进入日志页时重新读取磁盘。
- 点击刷新时重新读取磁盘。
- 搜索时使用内存缓存重新筛选，不重复读取文件。
- 首次只创建 `logInitialLoadCount` 条卡片。
- 滚动到底部时追加 `logLoadMoreCount` 条。

相关配置独立于历史记录：

- `logInitialLoadCount`
- `logLoadMoreCount`

不要恢复成一次性创建全部日志卡片，否则日志积累后会造成页面卡顿。

### 常见问题和弹窗编号

相关函数：

- `faqIdForTitle()`
- `faqIdForPopup()`
- `showAttentionMessageBox()`

规则：

- 弹窗错误标题会带问题编号。
- 常见问题页也带同一编号。
- 用户看到弹窗编号后，可以去常见问题搜索。

新增错误时必须做：

- 添加弹窗文案。
- 在 `faqIdForPopup()` 中映射。
- 在常见问题页添加同编号解决办法。
- 如果测试工具也会显示该问题，测试工具引导词也要同步。

### 托盘：`TrayController`

职责：

- 系统托盘图标。
- 托盘菜单。
- 打开主界面。
- 快速切换语音服务。
- 快速切换系统代理。
- 快速切换浮动条。
- 退出程序。

注意：

- 托盘常驻关闭主窗口时只隐藏窗口。
- 如果用户关闭托盘常驻，则关闭窗口会退出程序。

### 中文右键菜单：`ChineseTextContextMenu`

职责：

- 把 Qt 文本框默认英文右键菜单替换为中文。
- 保留快捷键显示，如 `Ctrl+C`。

注意：

- Qt 自带翻译文件不一定覆盖所有文本框菜单。
- 新增复杂文本控件时要检查右键菜单是否中文。

## 第三部分：运行模型和模块关系

### 核心对象生命周期

核心对象都在 `runVocekit()` 中创建，生命周期覆盖整个 Qt 事件循环：

```text
QApplication app
  ├─ AppSettingsStore settingsStore
  ├─ HubSettingsState settings
  ├─ FloatingBar bar
  ├─ GlobalHotkeys hotkeys
  ├─ HubWindow hub(settings access, &bar, ...)
  ├─ VoiceController voice(settings access, &bar, &hub)
  └─ TrayController tray(&hub, callbacks)
```

依赖关系：

- `HubWindow` 不拥有 `AppSettingsStore` 和 `FloatingBar`，只通过访问回调读取或保存类型化快照。
- `VoiceController` 不拥有 `AppSettingsStore`、`FloatingBar` 和 `HubWindow`。
- `TrayController` 不拥有主窗口和设置。
- 这些对象在 `runVocekit()` 栈上按固定顺序创建，并在事件循环结束后逆序销毁。
- 不要让异步回调捕获已经销毁的裸指针。
- 快捷键回调使用 `QPointer<VoiceController>` 防止悬空访问。

### 设置变更传播

设置页修改配置后，通常调用统一的变更回调。完整传播链路是：

```text
SettingsPanel 修改 AppSettingsData 副本
  -> applyAndSave 回调
  -> AppSettingsStore::replaceAndSave()
  -> notifySettingsChanged 回调（只调用一次）
      -> 同步 Windows 开机自启动
      -> VoiceController::reload()
      -> 重新注册 GlobalHotkeys
      -> 更新 FloatingBar 开关
      -> ApplicationEvents::publishSettingsChanged()
      -> HubRefreshCoordinatorBundle 统一刷新快捷键、状态和页面
```

新增会影响运行时的设置时，不要只把值写进 JSON，也不要在保存组件里直接刷新多个页面。保存成功后使用统一设置通知；保存失败必须停止后续刷新。运行对象先应用新设置，再由 `ApplicationEvents` 驱动页面刷新。

### 稳定的逻辑 ID

软件内部使用逻辑 ID，而不是用中文标题判断功能：

| ID | 含义 |
| --- | --- |
| `dictate` | 听写 |
| `translate` | 翻译 |
| `ask` | 问答 |
| `hub` | 打开主界面 |
| `vocabulary_add` | 把选中文字加入词库 |
| `custom_N` | 用户创建的自定义功能 |

规则：

- 中文名称可以修改，逻辑 ID 不应随 UI 文案改变。
- 历史记录、功能排序、提示词绑定、快捷键和词库作用范围都可能保存这些 ID。
- 删除自定义功能前要考虑历史记录和提示词中仍可能保留旧 ID。
- 新内置功能需要加入默认定义、快捷键、模型、输入方式、输出方式、提示词和历史分类。

### 功能配置模型

每个听写、翻译、问答和自定义功能都可以独立配置：

- 快捷键。
- 模型。
- 是否读取鼠标选中文字。
- 是否使用语音输入。
- 展现方式：自动写入或结果小框。
- 结果模板。
- 浮动条显示秒数。
- 结果小框显示秒数。
- 录音倒计时。
- 录音提示音开关和声音路径。
- 绑定的提示词。

输入方式约束：

- 至少启用“选中文字”或“语音输入”中的一种。
- 翻译默认只读取选中文字。
- 听写默认只使用语音。
- 问答可组合选中文字和语音问题。
- 自定义功能可以自由组合。

### 功能流程画布与发布运行时

每个功能可以在原有设置页和流程画布之间切换。画布支持九种节点：

- 语音采集、选中文字、截图识别。
- 输入节点、调用大模型、输出节点。
- 结果小框、截图对照窗、自动写入。

设置页与画布不得共用滚动工作区：设置页保留纵向滚动，画布放在独立的固定工作区中。画布内部不显示横向或纵向滚动条；普通滚轮不移动页面或画布，空白处按住鼠标左键拖动用于平移，`Ctrl + 滚轮` 用于缩放。节点、端口和连线仍保留各自的左键编辑行为。

离开画布前必须同步提交草稿和编辑器视口；任一保存失败都保留画布模式并显示错误。切换功能时，页面和上层控制器只能在 `FunctionCanvasEditor::setFunctionId()` 成功后提交新 ID，避免页面标题、编辑器控制器和实际保存目标分裂。草稿或视口设置事件只调用窄 `refreshCanvasState()`，不得全量重建已经显示的设置表单或重置其滚动位置。

编辑器视口保存事件可能携带尚未写入磁盘的旧草稿快照。只有远端草稿 revision 高于本地基线时，才把本地未保存图判定为冲突；相同 revision 的旧图回显不能阻断当前草稿。节点拖放以鼠标松开点对齐卡片中心，并按节点类型的实际高度计算偏移。Inspector 显示或隐藏造成画布宽度变化时必须保留现有内容的屏幕位置，不能重新以视口中心锚定。

流程数据位于对应 `FunctionSettings::flow`，但四类状态不能混用：

- `draft` 是可编辑图和草稿 revision。拖动、连线和 Inspector 修改只更新草稿。
- `editor` 只保存视口中心和缩放，不属于运行语义，也不能污染撤销栈。
- `published` 是校验和编译成功后的图、发布 revision、完整 SHA-256 哈希和触发画像。
- `enabled` 决定已发布画像是否参加快捷键路由；停用不删除草稿或发布快照。

发布必须经过同一事务：

```text
读取期望草稿 revision
  -> 校验节点、端口、DAG、触发入口和必需上下文
  -> 编译每个可用触发画像
  -> 计算完整图哈希
  -> 原子保存 settings.json
  -> 发布设置事件并刷新对应功能缓存
```

任一步失败都保留旧发布版。运行控制器只从 `FunctionFlowPlanCache` 取得不可变的发布计划，不直接读取或解释当前草稿；运行中继续持有启动时冻结的 revision/hash，即使用户随后编辑草稿或重新发布也不改变当前执行。

三个触发入口必须区分：

- `MainHotkey`：主功能快捷键。
- `ScreenshotHotkey`：独立截图快捷键。
- `ScreenshotLauncher`：截图悬浮入口。

`FunctionCommandController` 只有在启动结果为 `NotAvailable` 时才执行经典流程。已发布流程忙碌、目标窗口失效、配置错误、运行后节点失败或用户取消，都不能再跑一遍经典流程。流程和经典录音/截图共用 `VoiceController` 的忙碌仲裁，但传给流程的 `classicWorkflowBusy` 不包含流程自身。

运行分层如下：

```text
快捷键/Launcher 冻结目标窗口
  -> FunctionFlowExecutionController 取得发布计划
  -> FunctionFlowRuntimeAdapters 冻结模型、提示词、语音、OCR 和历史目录
  -> FunctionFlowScheduler 串行调度并等待依赖
  -> 输入/模型/结果控制器执行真实动作
  -> 一次性历史保存、节点日志和窄 UI 运行事件
```

取消令牌要贯穿倒计时、录音、OCR、模型和结果生成。控制器使用运行 ID 屏蔽取消后的迟到回调，旧结果窗口也只能取消创建它的那次运行，不能误伤下一次运行。

历史和日志有不同职责：

- 一次流程只保存一条历史，保留发布 revision/hash、触发入口、节点状态、耗时、模型 ID、提示词版本、失败节点和语音文件元数据。
- 截图历史只保存安全矩形、引擎、语言、耗时等元数据，不复制图片正文。
- `logs/function-flow.jsonl` 只写功能 ID、运行 ID、节点类型、状态、耗时、稳定错误码、模型 ID 和提示词版本；禁止写完整提示词、用户正文、音频、图片、请求响应正文或密钥。
- 用户提示、FAQ、历史和日志统一通过 `function_flow_errors.*` 使用同一组稳定错误码。

设置事件必须按 key 分流：草稿和编辑器视口只刷新已创建的编辑器；发布版、启用状态和功能定义才重建对应运行缓存、重载 `VoiceController` 并重注册快捷键。Hub 只把节点/运行事件转发给已经创建且功能 ID 匹配的画布，不能为了刷新或着色主动创建页面。

### 文本输入流程

不需要语音的功能流程：

```text
快捷键
  -> 记录目标前台窗口
  -> ClipboardBridge 读取选中文字
  -> 词库本地预修正
  -> 根据功能生成 system prompt 和 user text
  -> ModelRequestTask 通过 ProviderRegistry 调用模型
  -> 词库最终兜底修正
  -> 保存历史
  -> 自动写入或结果小框
```

如果输入方式要求选中文字但读取为空：

- 不应继续调用模型。
- 提示“未识别到有选中文字”。
- 日志记录功能 ID、是否开启强力选中以及读取结果字数。

### 语音输入流程

```text
快捷键
  -> 检查当前功能是否允许语音
  -> 检查语音服务配置
  -> 可选倒计时
  -> 可选提示音
  -> AudioRecorder 开始录音
  -> 浮动条显示波形
  -> 再次按同一快捷键停止
  -> AudioRecorder 返回 PCM 并保存 WAV
  -> SpeechRecognitionTask 通过 ProviderRegistry 调用语音服务
  -> 词库修正识别文本
  -> 可选大模型整理
  -> 保存历史
  -> 输出
```

状态变量：

- `m_recording`：是否正在录音。
- `m_countdownActive`：是否处于录音准备倒计时。
- `m_processing`：是否正在识别或模型处理。
- `m_modeId`：当前正在运行的功能 ID。
- `m_targetWindow`：结果需要写回的原前台窗口。

不要绕过这些状态变量直接启动录音或模型请求。

### 模型调用和流式显示

统一入口是 `ModelRequestTask`。它根据模型 ID 得到提供商，再从 `ProviderRegistry` 获取 `IModelProvider`；普通和流式调用统一使用 `IModelProvider::complete()`，是否流式由 `ModelRequest::stream` 决定。

```text
软件模型 ID
  -> modelProvider()
  -> providerModelId()
  -> ProviderRegistry
  -> DeepSeek / OpenAI / Claude / 自定义接口 Provider
```

流式调用要求：

- 每次增量通过 `onDelta` 追加到结果小框。
- 完成后仍要得到完整文本，用于历史保存、重试和词库修正。
- 网络错误时停止忙碌状态并显示可理解的错误。
- 关闭结果小框后，异步回调不能继续访问已销毁控件，应使用 `QPointer`。

### 输出方式

自动写入：

- 使用剪贴板临时放入结果。
- 激活原目标窗口。
- 模拟粘贴。
- 尽量恢复用户原剪贴板。
- 保存可撤销的上次输出。

结果小框：

- 不立即写入目标窗口。
- 用户可以先编辑。
- 允许复制、写入或替换选中。
- 编辑后关闭时可保存草稿到历史。
- 流式模式下会边生成边显示。

结果模板：

- 简洁。
- 详细。
- 对照原文。
- 仅输出结果。

模板只负责展示包装，不应改变模型实际任务语义。

## 第四部分：配置和数据格式

### `settings.json` 主要字段

下面是需要长期保持兼容的主要字段：

| 字段 | 类型 | 用途 |
| --- | --- | --- |
| `trayResident` | bool | 关闭主窗口时是否保留托盘后台 |
| `autoStartEnabled` | bool | Windows 开机自启动 |
| `strongSelectionEnabled` | bool | 普通读取失败后是否启用更强的选区读取 |
| `floatingBarEnabled` | bool | 全局浮动条开关 |
| `dictatePolishEnabled` | bool | 听写后是否调用模型整理 |
| `useSystemProxy` | bool | Qt 网络请求是否使用系统代理 |
| `speechProvider` | string | 当前语音服务 |
| `recordPath` | string | 历史记录根目录 |
| `hotkeys` | object | 内置功能快捷键 |
| `models` | object | 内置功能模型 |
| `outputModes` | object | 内置功能展现方式 |
| `inputModes` | object | 是否读取选区、是否使用语音 |
| `displayTimes` | object | 浮动条、结果框、倒计时和提示音设置 |
| `promptIds` | object | 功能绑定的提示词 ID |
| `customFunctions` | array | 自定义功能定义 |
| `functionOrder` | array | 主页功能卡片顺序 |
| `vocabularyEnabled` | bool | 词库总开关 |
| `vocabularyAddMode` | string | 快捷键加入词库时的 AI 策略 |
| `vocabularyOnlyForVoiceInput` | bool | 是否只在语音功能中应用词库 |
| `vocabularyPromptEntryLimit` | int | 最多注入模型的相关词条数 |
| `historyInitialLoadCount` | int | 历史首次加载数量 |
| `historyLoadMoreCount` | int | 历史追加数量 |
| `logInitialLoadCount` | int | 日志首次加载数量 |
| `logLoadMoreCount` | int | 日志滚动追加数量 |

配置兼容原则：

- 新字段缺失时必须有合理默认值。
- 读取数值时使用 `qBound()` 限制范围。
- 保存字段名不要随意改动。
- 真要重命名时，先读旧字段并保存为新字段，再经过版本迁移后删除旧逻辑。

### `secrets.json` 主要字段

语音接口：

- `baidu_api_key`
- `baidu_secret_key`
- `baidu_app_id`
- `xfyun_app_id`
- `xfyun_api_key`
- `xfyun_api_secret`
- `custom_speech_url`
- `custom_speech_api_key`
- `custom_speech_model`

大模型接口：

- `deepseek_api_key`
- `openai_api_key`
- `anthropic_api_key`
- `custom_models`

旧版单个自定义模型字段仍可能存在于结构体中，但当前 UI 应以可创建多个配置的 `custom_models` 为主。

安全要求：

- 不把密钥写入日志。
- 不在错误弹窗显示完整密钥。
- 不把真实 `secrets.json` 放入测试包。
- 接口自检只检查可用性，不查询余额。

### 提示词数据

提示词有三种来源：

1. `prompts/*.txt`：内置默认回退。
2. `config/prompts.json`：用户创建的独立提示词。
3. 自定义功能配置中的提示词或提示词 ID。

读取优先级由 `sharedPromptText()`、`promptForFunction()` 等函数统一处理。不要在业务流程里直接读取某个文本文件，否则会绕过用户在 UI 中的修改。

### 词库数据

每个 `VocabularyEntry` 主要包含：

- `id`：稳定标识。
- `source`：原词或错词。
- `target`：标准写法。
- `aliases`：其它可能写法。
- `scopeId`：作用范围。
- `matchMode`：匹配方式。
- `note`：备注。
- `enabled`：是否启用。

一个有效词条必须能产生实际修正：

- `source` 和 `target` 不同；或
- 别名中至少有一个值与 `target` 不同。

词库分三层参与处理：

1. 模型调用前进行本地预修正。
2. 只选择与当前文本相关的少量词条注入模型，默认最多 16 条。
3. 模型输出后再进行本地兜底修正。

这样可以支持大量词条，又不把整个词库塞进提示词。

### 历史详细记录

每条详细 JSON 应尽量保留：

- 功能 ID 和功能名称。
- 时间。
- 选中文字或识别文本。
- 最终输出。
- 错误信息。
- 使用模型。
- 总耗时。
- 录音路径。
- 文本文件路径。
- 收藏状态和收藏夹。
- 是否为结果小框草稿。

不要只保存 UI 当前显示的摘要，否则详情、搜索、导出和问题排查会丢失信息。

## 第五部分：线程、事件循环和稳定性

### Qt 主线程规则

以下操作应在主线程进行：

- 创建和销毁 QWidget。
- 修改控件文字、可见性和布局。
- 显示弹窗。
- 注册和刷新大部分 UI 状态。

耗时文件扫描、网络等待或大量历史读取不能长时间阻塞主线程。

### 原生快捷键重入

`WM_HOTKEY` 来自 Windows 原生消息。不能在 `nativeEventFilter()` 中直接开始录音、访问网络或创建复杂弹窗。

正确方式：

```text
nativeEventFilter()
  -> 只识别快捷键 ID
  -> QTimer::singleShot(0, ...)
  -> 回到 Qt 事件循环
  -> VoiceController::handleHotkey()
```

### 异步对象安全

异步回调常见风险：

- 结果小框已关闭，但网络回调还在追加文字。
- 页面已刷新，旧 `QFutureWatcher` 回调仍试图更新旧列表。
- 软件退出时仍有定时器触发。

防护方式：

- 对 QWidget/QObject 使用 `QPointer`。
- watcher 完成后调用 `deleteLater()`。
- 用 generation 编号忽略旧任务结果。
- 回调开始时检查目标指针是否为空。
- 不在 lambda 中长期捕获局部变量引用。

### 网络请求

所有请求都应具备：

- 明确超时。
- 网络错误转换。
- HTTP 状态和接口错误解析。
- 日志中的提供商、耗时和错误摘要。
- 代理模式说明。

注意：关闭“使用系统代理”不代表一定绕过 TUN、透明代理和虚拟网卡，因为这些会在更底层接管流量。

### 文件写入

设置、提示词、词库、历史索引等重要 JSON 优先使用原子写入：

```text
写临时文件
  -> 完整写入
  -> commit 替换正式文件
```

不要先清空正式文件再写，否则程序崩溃或磁盘错误可能留下空配置。

### 对话框规则

- 普通自定义对话框使用 `AppDialog`。
- `AppDialog` 默认移除无功能的标题栏“？”。
- 只有明确实现且测试过上下文帮助时才允许保留问号。
- 提示框只在显示时短暂置前，不设置永久 `WindowStaysOnTopHint`。
- 所有按钮要留足中文文字高度和宽度。

## 第六部分：常见修改配方

### 新增一个全局设置开关

按顺序修改：

1. 在 `AppSettingsData` 添加字段和默认值。
2. 在 `app_settings_json.*` 添加兼容读取和保存映射。
3. 需要主窗口编辑时，在 `HubSettingsState` 添加类型化访问方法。
4. 保存必须通过注入的 `applyAndSave` 回调进入 `AppSettingsStore::replaceAndSave()`。
5. 在 `SettingsPanel` 对应标签加入开关卡片。
6. 如果运行对象需要立即变化，加入设置变更传播。
7. 如果主页当前状态需要展示，更新状态区域。
8. 更新 `config/settings.example.json`。
9. 添加常见问题或测试工具说明。
10. 编译 Debug 和 Release。

### 新增一个大模型提供商

按顺序修改：

1. `SecretConfig` 和 `SecretStore` 增加必要字段。
2. 接口设置 UI。
3. `modelOptions()` 增加模型。
4. `modelProvider()` 增加提供商识别。
5. `providerModelId()` 处理实际请求模型名。
6. 在 `src/providers/` 实现独立 `IModelProvider`，并使用 `IProviderNetworkTransport`。
7. 在 `ProviderRegistry` 注册提供商。
8. 增加普通、流式、取消、代理、错误转换和接口自检测试。
9. 更新网络诊断域名。
10. 更新常见问题、示例配置和测试包。

### 新增一个语音识别服务

按顺序修改：

1. 定义稳定服务 ID。
2. 增加服务显示名称。
3. `SecretConfig` 增加字段。
4. 设置接口页根据当前服务只显示需要的字段。
5. `speechProviderConfigurationError()`。
6. `speechAsr()` 路由。
7. 具体识别实现。
8. 接口自检、测试工具和当前状态。
9. 常见问题和示例配置。

### 新增一个内置功能

按顺序修改：

1. 在内置功能定义中增加稳定 ID。
2. 设置默认快捷键。
3. 设置默认模型、输入和输出方式。
4. 增加默认提示词。
5. 在 `VoiceController::runContext()` 增加处理路由。
6. 增加主页卡片和功能自定义卡片。
7. 增加历史标签和词库作用范围。
8. 增加快捷键设置项。
9. 添加测试和常见问题。

不要通过复制整套听写代码实现新功能。应复用现有功能配置、模型调用、结果展示和历史保存流程。

### 修改历史或日志加载

检查：

- 是否只创建当前批次的控件。
- 搜索是读取磁盘还是筛选缓存。
- 滚动到底是否会重复加载。
- 刷新是否重置当前批次。
- 页面销毁或切换后回调是否仍安全。
- 首次加载和追加数量是否使用各自独立配置。

### 新增弹窗

推荐：

```cpp
AppDialog dialog(parent);
dialog.setWindowTitle(tr8("标题"));
```

不要直接使用：

```cpp
QDialog dialog(parent);
```

如果必须显式使用 `QMessageBox`，需要检查标题栏上下文帮助按钮，并使用项目统一提示函数生成错误编号。

## 第七部分：隐私和发布边界

### 绝对不能上传

- 真实 API Key、Secret、Token。
- `1.txt` 或其它临时密钥文件。
- 用户录音。
- 用户历史。
- 用户词库。
- 用户提示词。
- 本机绝对路径配置。
- 运行日志中的隐私内容。
- Debug/Release 二进制和 DLL，除非明确作为 Release 附件发布。

### AI 修改时不能做

- 不读取密钥后把值写入回答、日志或文档。
- 不为了测试接口把密钥硬编码到源码。
- 不提交用户当前 `config/*.json`。
- 不恢复从旧文本文件自动导入密钥的逻辑。
- 不增加余额查询。
- 不把完整选中文字或完整语音识别内容写入运行日志。

### 发布前隐私检查

```powershell
git status --short
git status --ignored --short
rg -n "sk-[A-Za-z0-9_-]{20,}|client_secret=|api[_ -]?key|C:\\Users" `
  --glob "!debug/**" --glob "!release/**" --glob "!dist/**" .
```

搜索结果需要人工判断。示例配置里的字段名可以存在，但不能出现真实值。

## 第八部分：验证层级

### 第一级：静态检查

- 搜索是否存在旧名称。
- 搜索是否出现硬编码密钥。
- 搜索新增配置键是否覆盖默认、读取、保存和示例。
- 搜索裸 `QDialog`，确认是否应改为 `AppDialog`。
- 检查新增弹窗是否进入常见问题编号系统。

### 第二级：编译

- qmake。
- Debug 编译。
- Release 编译。
- 注意运行中的 `vocekit.exe` 会锁住输出文件并导致链接失败。

### 第三级：界面检查

- 初始窗口。
- 缩小窗口。
- 最大化。
- 125% 和 150% Windows 缩放。
- 中文按钮和多行文字是否裁剪。
- 滚动区域是否只滚动预期部分。
- 弹窗是否存在无功能“？”。

### 第四级：核心流程

- 听写。
- 翻译选中文字。
- 问答。
- 自定义功能。
- 自动写入。
- 结果小框。
- 词库修正。
- 历史保存和详情。
- 日志刷新和滚动加载。
- 托盘和开机自启动。

### 第五级：错误流程

- 未填写语音密钥。
- 未填写模型密钥。
- 没有选中文字。
- 麦克风不可用。
- 网络超时。
- 代理或 TUN 导致连接被关闭。
- 历史目录不可写。
- 模型返回空内容或非法 JSON。

每个会弹出提示框的错误都应能在常见问题中通过编号、标题或错误原文搜到。

## AI 修改项目时的固定检查清单

### 新增功能时

检查：

- 左侧导航是否需要新入口。
- 主页是否需要新卡片。
- 功能自定义是否需要新卡片。
- 快捷键设置是否需要新卡片。
- 历史记录筛选是否需要新标签。
- 提示词是否需要新默认项。
- 常见问题是否需要新编号。
- 测试工具是否需要新测试项。
- 设置示例文件是否需要新字段。

### 新增设置项时

检查：

- `AppSettingsData` 默认值。
- `app_settings_json.*` 读取和保存映射。
- `AppSettingsStore::replaceAndSave()` 保存路径。
- 设置 UI。
- 当前状态区域。
- `config/settings.example.json`。
- 测试包默认配置。
- 关闭窗口是否自动保存。

### 新增接口时

检查：

- `SecretConfig`。
- `loadSecrets()` / `saveSecrets()`。
- 接口页 UI。
- `ProviderRegistry` 和对应 Provider。
- 接口自检。
- 网络诊断。
- 常见问题。
- `config/secrets.example.json`。
- `scripts/package-test.ps1`。

### 新增报错时

检查：

- 弹窗是否有清楚原因。
- 弹窗是否有常见问题编号。
- 常见问题页是否有同编号解决办法。
- 文案是否中文。
- 弹窗不要持续置顶。

### 修改 UI 时

检查：

- 普通弹窗必须使用项目统一的 `AppDialog`，不要直接创建 `QDialog`。
- `AppDialog` 会自动移除 Qt 标题栏中没有实际作用的上下文帮助“？”。
- 除非标题栏“？”已经绑定明确且可测试的帮助内容，否则禁止保留该按钮。
- 显式创建 `QMessageBox` 时仍需单独移除 `Qt::WindowContextHelpButtonHint`。
- 新增自定义弹窗后，必须检查标题栏是否出现无功能的“？”。
- 小窗口下是否裁剪文字。
- 全屏下是否浪费空间。
- 中文按钮高度是否足够。
- 长内容区域是否可滚动。
- 常用按钮是否固定可见。
- 是否还有英文菜单或英文按钮。

### 修改历史记录时

检查：

- 新结构是否保持一致。
- 总文本、总录音、总详细记录是否同步。
- 功能分类和日期目录是否正确。
- 备份、导入、导出是否都能用。
- 搜索、分页、加载更多是否不被破坏。

### 修改快捷键时

检查：

- 注册是否成功。
- 冲突检测是否正确。
- 自定义功能是否同步。
- `GlobalHotkeys::nativeEventFilter()` 不要同步执行重业务。
- `VoiceController` 是否正确防重复进入。

### 修改打包时

检查：

- 测试包能独立运行。
- 不包含真实 `config/secrets.json`。
- 不包含真实 `config/settings.json`。
- 不包含真实 `config/prompts.json`。
- 不包含真实 `config/lexicon/entries.json`。
- 不包含 `records/`。
- 不包含源码和中间文件。

## 常用开发命令

正式入口默认使用当前已验证的 Qt 6 和 MinGW 路径，换电脑时通过参数替换：

```powershell
& .\scripts\build.ps1 -Configuration debug `
  -QtBin "D:\Qt\6.11.1\mingw_64\bin" `
  -MingwBin "D:\Qt\Tools\mingw1310_64\bin"
```

Debug 编译：

```powershell
& .\scripts\build.ps1 -Configuration debug
```

Release 编译：

```powershell
& .\scripts\build.ps1 -Configuration release
```

启动调试版：

```powershell
Start-Process -FilePath '.\.qt6-build\debug\vocekit.exe' -WorkingDirectory '.\.qt6-build\debug'
```

如果 exe 正在运行导致链接失败：

```powershell
Get-Process -Name vocekit -ErrorAction SilentlyContinue | Stop-Process -Force
```

生成测试包：

```powershell
& .\scripts\deploy.ps1
& .\scripts\package-test.ps1
```

检查是否误提交隐私：

```powershell
git status --short
git status --ignored --short
rg -n "sk-[A-Za-z0-9_-]{20,}|config/secrets.json|records/|C:\\Users" .
```

## 当前最重要的架构约束

- 不能提交真实密钥。
- 不能提交真实录音和历史记录。
- 不能把旧 `1.txt` 密钥读取逻辑加回来。
- 不要查询余额等敏感接口信息。
- 不要在全局快捷键原生事件里跑重业务。
- 不要默认用 `Ctrl+C` 读取选区。
- 不要使用持续置顶提示框。
- 功能级配置统一放在“功能自定义”。
- 设置页按常用设置、词库、语音录音、网络、历史记录、快捷键和接口分类。
- 新错误必须进入常见问题编号系统。
- 新弹窗默认使用 `AppDialog`，禁止留下无功能的标题栏“？”。
- 历史和日志必须增量加载，不能一次性创建全部卡片。

## 按住说话与分段长录音

### 模块

- `src/input/hold_to_talk.h/.cpp`：Windows 低级键盘钩子和可测试的按键状态匹配器。
- `src/recording/segmented_recording.h/.cpp`：分段顺序、识别重试、成功/失败统计和文本合并。
- `src/recording/audio_recorder_legacy.h`：麦克风采集、PCM/WAV 保存和指定目录录音。
- `VoiceController`：定时切段、后台串行识别、完整 WAV 合并、模型处理和历史保存。

### 关键约束

- `WH_KEYBOARD_LL` 回调只能判断按键并投递 Qt 回调，不能直接录音、访问网络或更新复杂 UI。
- 长录音只有一个当前录音段和一个语音识别工作线程，禁止并发调用多个语音识别请求。
- 每段最多识别两次，第二次仍失败后必须结束重试，避免无限循环。
- 一次长录音最多 33 段；达到上限后停止采集，但必须继续处理已经排队的识别任务。
- 合并文字必须按段号排序，不能按网络返回顺序拼接。
- 全部分段失败时禁止调用大模型。
- 完整录音由原始 PCM 顺序拼接后重新生成 WAV 头，不能直接拼接多个 WAV 文件。
- 新任务开始时必须清理上一次录音的分段元数据，防止文本任务错误引用旧录音。
- 历史索引必须保留 `segments`，否则从索引加载后历史详情无法显示逐段结果。
- 历史详情重试失败段时，要从 WAV 的 `data` 分块提取 PCM，禁止把 WAV 头一起发送给语音接口。
- 历史重试成功后必须同步更新功能详细记录、总详细记录、功能文本记录、总文本记录和 `history_index.json`。

### 历史字段

```json
{
  "recordingTriggerMode": "hold",
  "longRecording": true,
  "segmentCount": 3,
  "failedSegmentCount": 1,
  "failedSegments": [2],
  "segments": [
    {
      "index": 1,
      "audio": "segment.wav",
      "text": "第一段文字",
      "error": "",
      "recognitionElapsedMs": 1200,
      "attempts": 1
    }
  ]
}
```

### 修改后验证

- 运行 `tests/recording/recording_core_tests.cpp`。
- 编译 Qt 6 MinGW 调试版。
- 人工测试切换录音、按住说话、70 秒长录音、单段失败和全部失败。
- 检查历史详情的中文按钮高度和小窗口滚动，避免再次出现文字裁剪。

## 截图 OCR 功能链

### 模块

- `src/capture/screenshot_types.h/.cpp`：截图触发模式、逻辑快捷键编号、选区归一化和译文行映射。
- `src/capture/screen_capture_overlay.h/.cpp`：多显示器桌面抓取、区域选择和截图回调，不负责 OCR。
- `src/capture/screenshot_launcher.h/.cpp`：桌面截图悬浮入口、功能菜单和位置记忆。
- `src/capture/screenshot_result_window.h/.cpp`：原图、译文覆盖、双语对照、透明度和结果操作。
- `src/ocr/ocr_types.h`：`OcrTextBlock`、图片尺寸和 OCR 结果元数据。
- `src/ocr/ocr_manager.h/.cpp`：后台调度 RapidOCR、Windows OCR 和自定义云 OCR，并为一次识别共享统一取消令牌。
- `src/controllers/screenshot_workflow_controller.h/.cpp`：截图状态机、OCR 调度、截图工具栏模型动作、临时文件和运行上下文。
- `src/ui/ocr_page_controller.h/.cpp`：图片识别页、批量队列以及识别结果的后续 AI 动作。

### 状态约束

- 截图快捷键只能投递 `screenshot:<functionId>`，不能在原生快捷键回调中截图、OCR 或访问网络。
- 同一时间只能存在一个截图 OCR 任务；录音、倒计时或模型处理期间不能开始新的截图。
- 显示截图层前必须保存原目标窗口，后续“写入”和“替换选中”都使用这个窗口。
- 截图取消后必须清理 `m_processing`、临时文件和截图上下文，且不能保存错误历史。
- OCR 的本地辅助进程、云端请求和截图后的模型请求必须共享调用方持有的 `CancellationToken`；禁止再用页面私有布尔值或独立原子变量伪装取消。
- 关闭截图工作流或在图片识别页点击取消后，底层请求必须真正停止；不能只让界面忽略异步返回。
- OCR 成功且功能启用语音时，截图文字保存为上下文，语音只作为补充问题或要求。
- OCR 临时图片只能放在系统临时目录，完成、取消或失败后都必须删除。
- 不能把截图图片写入历史、日志或仓库；云端 OCR 只有用户明确选择并确认时才能上传。
- `AI 图片识别` 当前不能走 `OcrManager` 的文本 OCR 助手协议；在实现多模态请求前不能静默伪装成 RapidOCR。

### 主语音模型取消约束

- 每次功能运行必须由 `VoiceRunLifecycleController` 创建新的 `CancellationSource`，禁止复用上一轮令牌。
- 取消令牌必须按 `VoiceRunLifecycleController -> VoiceRunExecutor -> VoiceModelProcessingTask -> ModelRequestTask -> IModelProvider` 传递，不能在中间层重新创建无关令牌。
- 开始新模型运行、关闭正在生成的结果小框或销毁运行控制器时，必须取消当前模型请求。
- 普通生成、流式生成、重新生成、换模型重试和继续追问必须使用相同的取消判断。
- `request.cancelled` 是正常终态，不是网络错误；取消后不能显示“处理失败”，也不能把迟到结果写入历史或当前窗口。
- `ResultChoicePopup` 的关闭回调只负责请求取消，不直接操作 Provider 或网络回复。

### 诊断任务取消约束

- 接口自检和网络诊断必须通过 `DiagnosticTaskRunner` 启动，页面类不能直接持有裸 `QFutureWatcher`。
- 再次开始同一种诊断时必须先取消上一轮；旧任务即使稍后结束，也不能调用当前页面的完成回调。
- 离开测试工具页时必须取消仍在运行的接口自检和网络诊断，避免后台 DNS、HTTP 或 Provider 请求继续占用资源。
- 诊断请求使用调用方传入的 `CancellationToken`；Provider 自检、OCR 自检、DNS 查询和 HTTP 探测不能在中途重新创建无关令牌。
- 用户取消是正常收尾，页面显示“测试已取消。”，不能弹出通用失败框，也不能把取消误记为接口故障。
- 新增诊断任务时优先复用 `DiagnosticTaskRunner`；只有真正同步且短时的本地计算才允许直接在界面线程执行。

### 结果窗约束

- “截图对照窗”只在本次输入确实来自截图时启用；普通文本触发时回退到普通结果小框。
- 译文覆盖要求模型输出非空行数量等于 OCR 文字块数量；不匹配时自动显示双语对照。
- 编辑后的结果关闭前要保存草稿，重新生成和换模型要复用原截图上下文。
- 窗口最小尺寸为 760 x 520，底部中文操作按钮最小高度为 40px，禁止再次出现裁剪。
- 位置、大小和透明度写入 `config/settings.json`，不能硬编码本机坐标。

### 历史字段

```json
{
  "inputSource": "screenshot",
  "ocrEngine": "RapidOCR",
  "ocrElapsedMs": 415,
  "ocrUsedFallback": false,
  "screenshotRect": {
    "x": 100,
    "y": 120,
    "width": 800,
    "height": 220
  }
}
```

### 修改后验证

- 运行 `tests/capture/screenshot_core_tests.cpp`。
- 运行 `tests/ocr/ocr_core_tests.cpp`，并保证 `fake_ocr_helper.exe` 位于测试程序同目录。
- 重新编译 RapidOCR 和 Windows OCR 助手，确认返回 `imageWidth`、`imageHeight` 和 `blocks`。
- 编译 Qt 6 MinGW 调试版与发布版。
- 人工验证截图取消、OCR 成功、截图翻译、译文覆盖回退、透明度、窗口记忆和悬浮入口拖动。
