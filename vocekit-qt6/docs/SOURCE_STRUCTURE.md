# 源码结构说明

## 当前拆分状态

项目保留 `src/voiceassistant.cpp` 作为应用启动和少量兼容组装入口。主窗口、页面、设置、历史、截图、录音和部分任务流程已经迁入独立 `.h/.cpp` 模块，不再通过旧 `.inc` 文件拼接实现。

页面、输入收集、全局功能命令、语音识别、模型处理、运行收尾、历史持久化、配置读写、外部接口和结果输出已经拥有独立入口。模型、语音、OCR、接口自检和网络诊断的主要长任务均已接入统一取消令牌；历史修改已经统一经过记录服务，修改完成后再通过 `ApplicationEvents` 发布变化，由刷新协调器同步历史页和首页最近记录。功能流程画布现已建立独立的图模型、草稿/发布事务、编译计划、运行调度和结果收尾边界；经典流程仍作为没有可用发布画像时的兼容兜底。

## 主要目录

- `src/api/`：提供商共用的签名、错误转换和请求辅助函数。这里不再包含接口客户端；新的语音或大模型实现必须放在 `src/providers/`。
- `src/app/`：应用级事件中心，用于减少页面之间直接互相调用。
- `src/capture/`：截图选择、截图悬浮入口和截图结果窗口。
- `src/config/`：设置文件结构、JSON 转换、设置读写服务、接口密钥存储和百度示例代码解析。
- `src/controllers/`：应用流程协调器。这里负责把输入、任务、页面和输出模块组织成完整流程，但不重复实现底层识别、存储或网络请求。
- `src/domain/`：可复用的数据结构和纯逻辑，如功能配置、历史记录类型、执行耗时、错误结构和旧界面共享类型。
- `src/input/`：全局输入相关能力，例如内置快捷键定义和按住说话键盘钩子。
- `src/ocr/`：本地 OCR、云端 OCR、OCR 辅助进程和批量队列。
- `src/ui/`：主窗口、独立页面、设置分区、历史列表、词库、OCR、测试工具、悬浮条和结果小框等界面模块。
- `src/providers/`：统一接口提供商抽象、网络请求执行器、HTTP/SSE/WebSocket 可替换传输层和提供商注册表。DeepSeek、OpenAI、自定义 OpenAI 兼容接口、Claude、百度语音、讯飞语音和自定义语音均直接拥有请求与响应逻辑。
- `src/recording/`：录音分段、长录音合并和旧录音器拆分头文件。
- `src/runtime/`：功能流程运行日志等不依赖界面的运行期基础设施；只记录白名单元数据，不保存提示词、用户正文、图片、音频或密钥。
- `src/storage/`：本地持久化服务。当前包含历史记录目录、索引、详情 JSON 扫描、可读文本生成及统一历史修改服务。
- `src/tasks/`：可复用任务节点和任务控制基础设施。当前包含取消令牌、语音识别任务、诊断辅助和大模型请求任务。

## 当前仍需注意的文件

- `src/config/app_settings_data.h`：新的明确设置数据结构，后续功能设置应优先加到这里，而不是继续平铺到旧类成员里。
- `src/config/app_settings_json.*`：`AppSettingsData` 与 `config/settings.json` 的兼容转换层。这里负责保留旧字段和未知字段，避免迁移时丢配置。
- `src/config/app_settings_store.*`：设置读写的唯一生产入口，使用 `QSaveFile` 原子保存，不直接操作界面、不弹窗。`replaceAndSave()` 在写入失败时回滚内存快照，调用方不要自行组合“替换快照 + 保存”。
- `src/config/secret_config.*`：接口密钥和自定义大模型列表的数据结构与兼容入口。
- `src/config/secret_store.*`：`config/secrets.json` 的实际读写逻辑。新增接口密钥字段时优先改这里。
- `src/config/baidu_sample_parser.*`：解析百度智能云 AccessToken 示例代码里的 `client_id` 和 `client_secret`，供接口设置页复用。
- `src/input/hotkey_definitions.*`：内置快捷键、核心功能快捷键和截图快捷键默认值。旧设置中心、设置页和快捷键注册都应复用这里。
- `src/input/hotkey_parser.*`：把 `QKeySequence` 转换成 Windows `RegisterHotKey` 需要的修饰键和虚拟键码。不要在原生事件类里重复写解析逻辑。
- `src/domain/voice_function_execution_pipeline.*`：功能执行总入口。它把功能输入转换成统一输入请求，并集中处理词库预修正、上下文补充、流式接管、模型完成、失败和成功终态。后续输入输出画布应组合这条管线的节点，不要重新在界面里编写执行顺序。
- `src/domain/voice_run_executor.*`：把 `VoiceRunContext` 规划结果转换为统一的 `VoiceModelProcessingRequest`，集中传递模型、提示词、词库注入、流式回调并记录模型耗时。`VoiceController` 不再为听写、翻译、问答和自定义功能各维护一套模型中转方法。
- `src/tasks/model_request_task.*`：执行单次大模型请求，统一处理 `ModelRequest` 组装、调用方取消令牌、提示词版本、耗时和错误文本。未提供令牌时任务会创建本次执行令牌；需要支持界面取消的调用方必须传入自己持有的令牌。它由模型处理任务调用，不应在界面代码中直接调用 provider。
- `src/tasks/diagnostic_task_runner.*`：测试工具页后台任务的统一启动和取消入口。重复启动会取消旧任务并使用执行代次屏蔽迟到结果，页面隐藏或销毁时也必须取消。
- `src/tasks/interface_self_check_task.*`：接口自检任务。语音、大模型和 OCR 检查共享同一个调用方取消令牌，并通过 `ProviderRegistry` 获取具体接口。
- `src/tasks/network_diagnostics_task.*`：网络诊断任务。系统代理信息、DNS 查询和 HTTP 探测按顺序执行，每一步都检查同一个取消令牌。
- `src/tasks/diagnostic_helpers.*`：诊断共用的结果格式、可取消 DNS 查询和可取消网络探测；界面不要重复实现阻塞式 DNS 或网络事件循环。
- `src/domain/voice_screenshot_session.*`：统一维护单次截图工作流的代次、待识别选区、云端授权、OCR 结果和一次性运行上下文。它只保存状态，不持有界面对象，也不执行文件系统副作用。
- `src/controllers/screenshot_workflow_controller.*`：完整管理截图选区、OCR 调度、截图工具栏模型动作、临时文件和运行上下文注入。关闭或重置工作流时会通过统一取消令牌终止 OCR 和模型网络请求；`VoiceController` 只负责把快捷键请求和外层任务回调接到这个控制器。
- `src/controllers/voice_recording_workflow_controller.*`：完整管理录音准备、倒计时和提示音、录音开始与停止、按住说话、长录音分段、语音识别衔接以及悬浮条录音状态。`VoiceController` 只发起录音请求并接收最终识别文本。
- `src/controllers/selected_text_workflow_controller.*`：集中管理选中文字读取、强力选中参数、词库预修正、读取状态和纯文字功能的缺失输入阻断；默认平台读取适配器位于同目录的 `selected_text_workflow_adapters.cpp`。
- `src/controllers/vocabulary_quick_add_controller.*`：集中管理词库快捷键读取、AI/手动/每次询问策略、AI 失败回退、词条保存和词库刷新；询问弹窗位于 `src/ui/vocabulary_quick_add_dialog.*`。
- `src/controllers/voice_run_lifecycle_controller.*`：集中管理模型执行、提示词和词库注入、输出后修正、运行耗时记录和历史持久化。每次模型运行都会创建新的 `CancellationSource`，开始下一次运行或销毁控制器前必须先取消旧请求。默认生产适配位于 `voice_run_lifecycle_adapters.cpp`，`VoiceController` 不再直接组装历史请求或调用模型执行器。
- `src/controllers/voice_result_presentation_controller.*`：集中管理普通结果小框、截图结果窗、流式内容、自动写入、重新生成、换模型重试、继续追问、窗口位置和透明度、草稿及结果历史。结果小框在生成中关闭时通过访问接口取消当前模型任务，取消不会进入通用失败弹窗。它通过访问接口调用模型与存储，不直接依赖主窗口。
- `src/controllers/function_command_controller.*`：统一处理功能快捷键按下与释放、目标窗口、词库快捷加入、截图入口、选中文字、语音配置检查和录音启动。Windows 前台窗口读取位于独立适配器，`VoiceController` 不再保存命令运行状态。
- `src/domain/function_flow_graph.*`、`function_flow_ports.*` 与 `function_flow_runtime_types.*`：九种节点、类型化端口、触发画像、不可变运行计划、节点/运行事件及取消状态等纯数据结构。图层不得依赖 QWidget、Provider 或文件系统。
- `src/domain/function_flow_validation.*`、`function_flow_compiler.*` 与 `function_flow_scheduler.*`：依次负责结构和端口校验、按触发入口编译不可变 DAG 计划，以及按依赖顺序串行调度、汇合、失败和有界取消。运行时不得直接解释可编辑草稿。
- `src/config/function_flow_json.*`：流程 schema、草稿、编辑器视口和发布快照的兼容 JSON 边界。未知字段需要保留；较新 schema 和损坏数据必须安全只读或回退，不能静默覆盖。
- `src/controllers/function_flow_editor_controller.*` 与 `function_flow_publication_service.*`：编辑命令、撤销/重做、自动保存、版本冲突和发布事务。草稿保存只发布草稿事件；校验、编译、完整哈希和原子设置保存全部成功后才替换发布版。
- `src/controllers/function_flow_plan_cache.*`：按功能缓存已启用发布画像。设置事件带有 `functionIds` 时只重建对应功能；正在运行的 `QSharedPointer` 继续持有启动时冻结的 revision/hash。
- `src/controllers/function_flow_execution_controller.*` 与 `function_flow_runtime_adapters.*`：冻结目标窗口、模型、提示词、语音、OCR、历史目录等依赖，协调九种节点并屏蔽取消后的迟到结果。模型/提示词或服务配置失效属于配置错误，不能转入经典流程重复执行。
- `src/controllers/function_flow_result_controller.*`：统一处理结果小框、截图对照窗、自动写入、重新生成、换模型、继续追问和编辑后的最终文本。一次流程无论包含多少输出动作都只保存一条历史。
- `src/tasks/function_flow_model_task_runner.*` 与 `src/domain/function_flow_model_message.*`：把上游输入按确定顺序转换为单次模型请求，传递取消令牌、模型 ID、提示词版本和流式结果；模型节点不直接访问 UI。
- `src/ui/function_canvas_*.*`：画布视图、场景、节点、边、节点库、Inspector 和编辑器壳层。UI 只提交编辑命令和显示窄运行事件，不负责持久化、发布校验或运行调度。
- `src/domain/function_flow_errors.*` 与 `src/runtime/function_flow_runtime_log.*`：集中维护稳定错误码、用户安全提示、FAQ 映射和白名单 JSONL 日志，避免 UI、历史和日志各自拼接 Provider 详情。
- `src/domain/history_types.*`：历史记录详情、查询、摘要和分段重试结果等纯数据结构。新增历史字段时先改这里，再改读写逻辑。
- `src/storage/history_store.*`：历史记录读取和底层文件实现，负责目录结构、`history_index.json`、详情 JSON 扫描及可读文本生成。追加、删除、收藏、重试和索引写入接口是私有实现，生产调用方不得绕过记录服务。
- `src/storage/history_record_service.*`：生产环境唯一历史修改入口。语音和 OCR 保存、收藏更新、单条或批量删除、分段重试及导入后的索引重建都必须经过这里；服务会校验详情文件位于受管历史目录内。
- `src/ui/history_page_controller.*`：协调历史页面和存储服务。收藏、删除、导入、分段重试及收藏夹新增完成后发布 `HistoryChangeSet`，由 `HubRefreshCoordinatorBundle` 交给 `ApplicationEvents`，不要只刷新当前页面。
- `src/app/application_events.*`：设置、历史和词库变化的应用级通知入口。修改操作发布事件，页面刷新协调器负责决定刷新哪些页面；仅独立组件测试或没有事件中心的降级环境可以使用局部刷新回退。
- `src/ui/hub_application_event_coordinator.*` 与 `src/ui/hub_refresh_coordinator_bundle.*`：统一发布并接收 `SettingsChangeSet`、`HistoryChangeSet` 和 `VocabularyChangeSet`。设置保存成功后只发一次通知；运行时设置应用完成后，由事件订阅统一刷新状态、快捷键、功能页和相关缓存。
- `src/providers/provider_network_transport.*`：Provider 使用的最小网络传输接口。生产实现复用 `NetworkRequestExecutor`，测试可以注入假传输，不访问真实接口。
- `src/providers/deepseek_model_provider.*`：DeepSeek 的独立 Provider，负责请求构造、普通和 SSE 流式响应解析、取消、代理、错误转换和接口自检，不依赖旧 `ApiClient`。
- `src/providers/openai_compatible_model_provider.*`：OpenAI 与多个自定义兼容接口的独立 Provider，负责地址归一化、模型和鉴权选择、普通及分片 SSE 响应、取消、代理、错误转换和接口自检。
- `src/providers/claude_model_provider.*`：Claude 的独立 Provider，负责 Anthropic 请求头、模型名归一化、普通及分片 SSE 响应、取消、代理、错误转换和接口自检。
- `src/providers/baidu_speech_provider.*`：百度短语音的独立 Provider，负责 AccessToken 缓存、音频请求、响应解析、取消、代理、错误转换和接口自检。
- `src/providers/xfyun_speech_provider.*`：讯飞语音听写的独立 Provider，负责鉴权地址、PCM 分片、结果解析、取消、代理、错误转换和接口自检。
- `src/providers/custom_speech_provider.*`：自定义语音接口的独立 Provider，负责 JSON 音频请求、可选鉴权、自定义模型名、多种响应字段、取消、代理、错误转换和接口自检。
- `src/providers/provider_websocket_transport.*`：Provider 使用的通用 WebSocket 传输，集中处理连接、分片发送、超时、代理、远端关闭和取消。
- `src/providers/built_in_provider_factory.*`：内置语音和大模型 Provider 的生产工厂；任务层不直接构造具体提供商。

## 下一步建议

1. 使用真实测试账号继续复核语音、模型、云 OCR、快捷键冲突和跨应用自动写入；自动测试不得替代这些外部环境验收。
2. 后续增加节点类型时先扩展 schema、端口表、校验器、编译器和纯测试，再接 UI 与运行适配器，不能只在画布中添加一个可见卡片。
3. 保持草稿、发布版和正在运行的计划互相隔离；任何兼容迁移都要验证未知字段保留、发布失败不回滚旧版本以及经典流程只在 `NotAvailable` 时兜底。

每完成一段拆分，都要至少运行：

```powershell
qmake vocekit.pro -spec win32-g++ CONFIG+=release
mingw32-make -j2 release
```

如果改到已有测试覆盖的模块，也要运行对应测试。改网络、配置、录音、OCR、历史结构时，不能只靠编译通过。

历史存储相关改动还要运行：

```powershell
cd tests\storage
qmake history_store_tests.pro -spec win32-g++ CONFIG+=release
mingw32-make -j2 release
.\release\history_store_tests.exe
```
