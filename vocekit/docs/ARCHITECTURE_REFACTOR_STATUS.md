# 架构重构进度

更新时间：2026-07-25

这份文件只记录“大架构拆分”进度，方便后续 AI 或接手开发者知道哪些已经能复用，哪些还不能当作完成。

## 11-20 进度

| 编号 | 目标 | 当前状态 |
| --- | --- | --- |
| 11 | 把 `voiceassistant.cpp` 拆为真正的 `.h/.cpp` | 已完成。`voiceassistant.cpp` 现为 8 行，只保留 `runVocekit()` 兼容入口；Qt、设置、主窗口、控制器、快捷键、托盘和截图入口的组装已迁入独立的 `src/app/vocekit_application_runtime.*`。 |
| 12 | 将设置、历史、词库页面从 `.inc` 改成独立类 | 基本完成。设置、历史、词库、OCR、测试、日志和常见问题均已有独立类及页面控制器，旧 `src/pages/*_methods.h` 已不存在。 |
| 13 | 拆出语音任务控制器 | 基本完成。录音、长录音分段、语音识别衔接、截图 OCR、选中文字、词库快捷加入、单次运行生命周期和全局功能命令分发已分别迁入独立控制器；`VoiceController` 只负责组装这些工作流。 |
| 14 | 拆出功能执行管线 | 完成度较高。`VoiceFunctionExecutionPipeline` 已统一请求组装、词库预修正、截图上下文、流式分流、模型完成以及成功或失败终态；`VoiceRunLifecycleController` 接管模型执行、输出后修正和历史收尾，`VoiceResultPresentationController` 接管结果展示和结果操作。 |
| 15 | 建立统一接口提供商抽象 | 已完成。DeepSeek、OpenAI、自定义 OpenAI 兼容接口、Claude、百度语音、讯飞语音和自定义语音均由独立 Provider 接管；HTTP、SSE 和 WebSocket 传输都通过可替换传输层执行。旧 `ApiClient` 源文件及迁移期适配器已经删除。 |
| 16 | 建立统一任务取消接口 | 接近收尾。模型请求、主语音模型流程、结果重试/追问/流式生成、HTTP/SSE、讯飞 WebSocket、长录音分段、RapidOCR/Windows OCR 辅助进程、自定义云 OCR、截图工具栏 AI 动作、图片识别页 AI 动作、接口自检和网络诊断均使用 `CancellationSource` / `CancellationToken`；后续只需随新增长任务继续执行同一约束。 |
| 17 | UI 与业务逻辑彻底分离 | 进行中。页面、历史存储、设置读写、截图、录音、选中文字、词库快捷加入、功能命令分发、模型运行收尾、结果展示和外部接口均已分离；剩余重点是事件刷新统一和少量协调器边界收口。 |
| 18 | 配置项使用明确的数据结构 | 已完成。主程序唯一持有 `AppSettingsStore`，页面和控制器只通过 `AppSettingsData` 快照及保存回调访问设置；旧 `AppSettings` 与 `legacy_app_settings.h` 已移除。 |
| 19 | 历史存储独立为服务 | 接近完成。`HistoryStore` 负责读取和底层文件实现，`HistoryRecordService` 是生产环境唯一修改入口；语音、OCR、收藏、删除、分段重试和导入后的索引重建均已迁入服务。 |
| 20 | 用事件总线统一页面状态刷新 | 已完成。设置、历史和词库三类跨页面变化分别发布 `SettingsChangeSet`、`HistoryChangeSet` 和 `VocabularyChangeSet`，由刷新协调器更新所有受影响页面。页面激活和页面内部控件状态仍由对应页面控制器管理，不再与全局变化广播混用。 |

## 最近完成

- 将新的设置服务命名为 `AppSettingsStore`，避免与旧 `AppSettings` 冲突。
- 将 `AppSettingsStore` 加入主工程编译列表。
- 更新配置测试，让 `AppSettingsStore` 参与读写测试。
- 拆出 `src/input/hotkey_definitions.*`，把内置快捷键定义和截图快捷键默认值从 `voiceassistant.cpp` 移出。
- 新增 `tests/input/hotkey_definitions_tests.*`，保护默认快捷键列表不被后续改坏。
- 拆出 `src/input/hotkey_parser.*`，把 `QKeySequence` 到 Windows `RegisterHotKey` 参数的转换从 `GlobalHotkeys` 中移出。
- 新增 `tests/input/hotkey_parser_tests.*`，覆盖常用快捷键、特殊键和无效快捷键。
- 更新结构文档，说明新旧设置中心的边界。
- 拆出 `src/tasks/model_request_task.*`，把大模型请求组装、取消令牌、提示词版本、耗时和错误返回从 `VoiceController::runModelRequest()` 迁出。
- 新增 `tests/tasks/model_request_task_tests.*`，覆盖模型请求任务的参数传递、流式增量、取消令牌和错误返回。
- 录音工作流完整迁入 `VoiceRecordingWorkflowController`。
- `VoiceRunExecutor` 直接组装统一模型任务，控制器删除四种功能的模型中转方法。
- 新增 `VoiceFunctionExecutionPipeline`，控制器删除 `processInputThroughPipeline()` 和终态判断。
- 新增 `VoiceResultPresentationController`，集中管理普通结果小框、截图结果窗、流式展示、自动写入、重新生成、换模型重试、继续追问、草稿和结果历史。
- 新增 `SelectedTextWorkflowController`，集中管理选中文字读取、词库预修正、读取状态和缺少选中文字时的阻断规则。
- 新增 `VocabularyQuickAddController`，集中管理词库快捷键、AI/手动/每次询问策略、AI 失败回退、词条保存及页面刷新。
- 新增 `VoiceRunLifecycleController`，集中管理模型执行、词库提示注入、输出后修正、运行耗时和提示词版本记录，以及完整历史请求的持久化和通知。
- 新增 `FunctionCommandController`，统一处理快捷键按下与释放、主界面命令、目标窗口记录、词库快捷加入、截图入口、选中文字读取、语音配置校验和录音启动。
- `VoiceController` 已删除全局快捷键分类、截图逻辑编号解析、目标窗口和选中文字运行状态；当前为 904 行、29217 字节，比上一批减少 47 行和 1919 字节。
- `voiceassistant.cpp` 当前约 340 行，只承担应用组装和兼容入口。
- 主程序配置入口已完全迁移到 `AppSettingsStore`，启动时由存储服务加载或创建默认配置，不再直接调用 JSON 转换函数。
- 新增事务式 `replaceAndSave()`：设置文件写入失败时恢复原内存快照，避免当前运行状态与磁盘配置不一致。
- 新增配置回滚测试和启动架构防回退测试；`voiceassistant.cpp` 当前为 333 行、12760 字节。
- 新增 `IProviderNetworkTransport`，Provider 的请求构造与响应解析可以脱离真实网络独立测试，生产环境继续统一复用 `NetworkRequestExecutor`。
- 新增独立 `DeepSeekModelProvider`，接管普通响应、分片 SSE 流式响应、代理策略、取消令牌、错误转换、运行日志和接口自检。
- DeepSeek 工厂与注册中心已改用新 Provider；旧 `ApiClient::deepseekChat()`、`ApiClient::deepseekChatStream()` 及 DeepSeek 接口地址已经删除。
- 本轮第 15 项估算由 75% 提升到 85%，11-20 整体估算由 92% 提升到 93%。
- 新增独立 `OpenAiCompatibleModelProvider`，统一接管官方 OpenAI 和多个自定义兼容接口，支持普通响应、分片 SSE 流式响应、可选密钥、自定义地址、代理、取消、错误转换和接口自检。
- 设置页的自定义大模型“测试”按钮直接把当前未保存的表单快照交给独立 Provider，不再把测试配置注入旧 `ApiClient`。
- Provider 工厂与注册中心已把 `openai`、`custom` 和 `custom:<id>` 路由到新实现；旧 `ApiClient` 中对应请求、流式解析和 OpenAI 接口地址已删除。
- 增加 OpenAI 自检模型名回归测试，防止把提供商编号 `openai` 错当成实际模型名发送。
- 本轮第 15 项估算由 85% 提升到 91%，11-20 整体估算由 93% 提升到 94%。
- 新增独立 `ClaudeModelProvider`，接管 Anthropic 请求头、模型名归一化、普通响应、跨数据块 SSE 流式响应、代理、取消、错误转换、运行日志和接口自检。
- Provider 工厂与注册中心已把 `claude` 路由到新实现；旧 `ApiClient` 中通用模型请求、Claude 请求、Anthropic 地址和旧事件流解析均已删除。
- `ApiClient` 现在只承担三种语音服务的迁移期兼容工作，已缩减到 664 行、24445 字节。
- 全量 116 个测试程序、748 项测试全部通过，失败和跳过均为 0。
- 本轮第 15 项估算由 91% 提升到 96%，11-20 整体估算由 94% 提升到 95%。
- 新增独立 `CustomSpeechProvider`，接管自定义语音接口的 JSON 请求、可选鉴权、自定义模型名、代理、取消、错误转换、运行日志和接口自检。
- Provider 工厂与注册中心已把自定义语音路由到新实现；旧 `ApiClient` 中的自定义语音配置检查、自检和识别分支已删除。
- `ApiClient` 现在只保留百度和讯飞两种语音服务，`api_client.cpp` 已缩减到 539 行。
- 全量 117 个测试程序、764 项测试全部通过，失败和跳过均为 0；发布版、运行库核验、SSL 冒烟、`cppcheck` 和 `git diff --check` 均通过。
- 本轮第 15 项估算由 96% 提升到 97%，11-20 整体估算由 95% 提升到 96%。
- 新增独立 `BaiduSpeechProvider`，接管 AccessToken 获取与缓存、短语音请求、响应解析、代理、取消、错误转换、运行日志和接口自检。
- Provider 工厂与注册中心已把百度语音路由到新实现；旧 `ApiClient` 中的百度配置检查、令牌缓存、接口自检、识别请求和两个百度接口地址已全部删除。
- `IProviderNetworkTransport` 新增可替换 `GET` 请求，使令牌请求也能脱离真实网络测试；`ApiClient` 现在只保留讯飞 WebSocket 兼容实现，`api_client.cpp` 缩减到 291 行。
- 全量 118 个测试程序、781 项测试全部通过，失败和跳过均为 0；发布版、运行库核验、SSL 冒烟、`cppcheck` 和 `git diff --check` 均通过。
- 本轮第 15 项估算由 97% 提升到 98%，11-20 整体估算由 96% 提升到 97%。
- 新增独立 `XfyunSpeechProvider` 和通用 `IProviderWebSocketTransport`，讯飞鉴权地址、40 毫秒分片发送、响应解析、代理、超时、取消、错误转换和接口自检均已迁入 Provider。
- 讯飞工厂、注册中心、普通录音、长录音与历史分段重试统一通过 `ISpeechProvider` 执行；旧 `ApiClient`、旧适配器及其工程引用已经删除。
- 讯飞 Provider 新增 12 项单元测试；全量 119 个测试程序、790 项测试全部通过，其中非界面 496 项、界面 294 项，失败和跳过均为 0。
- 第 15 项由 98% 提升到 100%，第 16 项由约 70% 提升到约 74%，11-20 整体估算由 97% 提升到 98%。
- OCR 管理器删除独立的 `QAtomicInt` 取消状态，RapidOCR、Windows OCR 和自定义云 OCR 改为共享统一 `CancellationToken`；新增执行中取消与云端预取消测试。
- `ModelRequestTaskRequest` 支持由调用方提供取消令牌，截图工作流关闭、重置或图片识别页点击取消时，会真正中止截图后的大模型网络请求，而不是只忽略返回结果。
- 全量 119 个测试程序、794 项测试全部通过，其中非界面 500 项、界面 294 项，失败和跳过均为 0；发布版主程序完整链接成功，运行库、SSL、后台启动、`cppcheck` 和 `git diff --check` 均通过。
- 本轮第 16 项估算由约 74% 提升到约 84%，11-20 整体估算由约 98% 提升到约 98.5%。
- `VoiceRunLifecycleController` 为每次主语音模型执行创建独立取消令牌，并在新运行开始或控制器销毁前取消旧请求，避免旧流式回调污染下一次运行。
- 取消令牌已沿 `VoiceRunExecutor -> VoiceModelProcessingTask -> ModelRequestTask -> Provider` 完整传递，普通返回、流式返回、重新生成、换模型重试和继续追问使用同一条取消语义。
- 结果小框在“正在生成”状态关闭时会中止底层模型请求；取消结果只记录“已取消”，不再误弹“处理失败”。
- 新增生命周期、执行器、结果流程、模型任务和结果小框界面级取消测试；全量 119 个测试程序、799 项测试全部通过，其中非界面 504 项、界面 295 项，失败和跳过均为 0。
- 发布版主程序完整链接成功，`vocekit.exe` 为 3289088 字节；26 项运行库、x86 架构、SSL 和 `--autostart` 后台启动检查通过，`cppcheck` 零输出。
- 当前工作树的 `git diff --check` 通过。两个旧 PowerShell 界面契约仍引用已经迁移的主文件或 `.inc` 路径，需要改写为独立组件测试，不能再作为当前架构的有效验证。
- 本轮第 16 项估算由约 84% 提升到约 92%，11-20 整体估算由约 98.5% 提升到约 99%。
- 新增 `DiagnosticTaskRunner`，接口自检和网络诊断统一支持重新开始时取消旧任务、执行代次隔离和页面离开时取消。
- Provider 配置检查、OCR 自检、DNS 查询和 HTTP 探测接收同一调用方取消令牌；取消后不发布迟到结果，也不误报为接口失败。
- 全量 121 个测试工程完成验证：804 项 QtTest 全部通过，另有 2 个独立辅助/冒烟程序成功，失败和跳过均为 0。
- Qt 5.9 MinGW 发布版构建、26 项运行库及 x86 架构检查、SSL 冒烟、`--autostart` 后台启动、`cppcheck` 和 `git diff --check` 均通过。
- 本轮第 16 项估算由约 92% 提升到约 97%，11-20 整体估算由约 99% 提升到约 99.3%。
- `HistoryRecordService` 新增 OCR 保存、收藏更新、单条/批量删除、分段重试和索引重建接口，统一承担历史修改事务。
- OCR 页面、历史页面、历史操作控制器和历史备份导入均已迁移到服务；生产源码扫描未发现绕过服务的历史写入。
- `HistoryStore` 的追加、删除、收藏同步、重试更新和索引写入接口已改为私有，仅向 `HistoryRecordService` 和受控测试开放。
- 历史服务新增受管目录边界检查，拒绝修改历史根目录之外的详情文件；批量删除去重并只重建一次索引。
- 历史存储定向测试 17 项、历史归档测试 6 项通过；全量 121 个测试工程、808 项 QtTest 和 2 个独立程序通过。
- Debug、Release、部署运行库、x86、SSL、`cppcheck`、`git diff --check` 和历史写入绕过扫描均通过。
- 本轮第 19 项估算由约 85% 提升到约 98%，11-20 整体估算由约 99.3% 提升到约 99.5%。
- 历史页内部修改不再直接调用页面刷新：收藏切换、收藏夹调整、单条或批量删除、备份导入、分段重试和新增收藏夹统一发布历史变化事件。
- `HubContentPagesController` 通过 `HubRefreshCoordinatorBundle` 把历史变化交给 `ApplicationEvents`，首页最近记录、历史页缓存和其他订阅者使用同一刷新路径；没有事件中心的组件测试仍保留局部回退。
- 新增历史操作控制器事件测试，并更新历史页面访问工厂和内容页面控制器契约测试。
- 全量 122 个测试工程完成验证：120 个 QtTest 程序共 812 项通过，另有 2 个独立构建工程；发布版、26 项运行库、x86、SSL、`--autostart` 后台启动、全量 `cppcheck` 和 `git diff --check` 均通过。
- 本轮第 20 项估算由约 60% 提升到约 75%，11-20 整体估算由约 99.5% 提升到约 99.6%。
- `HubApplicationEventCoordinator` 和 `HubRefreshCoordinatorBundle` 新增设置变化发布入口；存在事件中心时发布 `SettingsChangeSet`，独立组件环境使用同一回调回退。
- 设置页删除“直接完整刷新后再通知”的重复路径，保存成功只通知一次；保存失败立即返回，不再刷新成成功状态。
- 主页功能设置和 `VoiceController` 的设置修改统一使用通知语义，运行时设置应用完成后再由事件中心刷新页面。
- 设置事件定向测试 35 项通过；全量 122 个测试工程完成验证，120 个 QtTest 程序共 815 项通过，另有 2 个独立构建工程。
- Debug、Release、26 项部署运行文件、x86、SSL、`--autostart` 后台启动、`cppcheck`、格式与 UTF-8 检查通过。
- 本轮第 20 项估算由约 75% 提升到约 84%，11-20 整体估算由约 99.6% 提升到约 99.7%。
- 提示词保存、内置功能编辑、自定义功能编辑、新增、删除和命令页编辑已删除保存后的直接页面刷新回调；保存操作只负责持久化，再由 `ApplicationEvents` 统一刷新所有受影响页面。
- `FunctionEditorDialog`、`FunctionManagementPage`、`FunctionCommandPage`、`FunctionWorkspaceController`、`FunctionPagesAccessFactory` 和 `HubUtilityPagesController` 不再各自维护重复的提示词、功能、导航和主页刷新链路。
- 新增或更新 8 个协调器和访问工厂测试程序，49 项定向测试覆盖“只保存一次、只发布一次、没有附属直刷”的约束。
- 全量 122 个测试工程完成验证：120 个 QtTest 程序共 815 项通过，另有 2 个独立程序成功；OCR 集成测试改用仓库固定样例并规范化 Windows 路径，连续运行 3 次均通过。
- Qt 5.9 MinGW Debug 和 Release 主程序构建通过；全量 `cppcheck` 零输出，`git diff --check` 通过，638 个测试生成的临时 `Makefile.codex*` 已清理。
- 本轮第 20 项估算由约 84% 提升到约 92%，11-20 整体估算由约 99.7% 提升到约 99.8%。
- 提示词保存、新增、复制和删除不再在发布设置事件后自行重复刷新；有应用事件回调时只通知一次，没有事件中心的独立组件环境才执行本地刷新回退。
- 词库快捷加入只发布 `VocabularyChangeSet`，删除附带的无关 `SettingsChangeSet`；宿主接口改名为 `notifyVocabularyChangedForVoiceController()`，明确它是变化通知而不是页面直刷。
- 新增提示词单刷新路径和词库事件分类回归约束，两个定向测试程序共 12 项通过。
- 新增 `scripts/run-all-tests.ps1`，严格按 `.pro` 扩展名枚举测试工程，汇总结果并自动清理临时测试 Makefile。
- 全量 122 个测试工程完成验证：120 个 QtTest 程序共 816 项通过，2 个独立程序成功，失败和跳过均为 0。
- Qt 5.9 MinGW Debug 和 Release 主程序构建通过；全量 `cppcheck`、PowerShell 脚本解析和 `git diff --check` 通过，临时 `Makefile.codex*` 剩余数量为 0。
- 本轮第 20 项由约 92% 提升到 100%，11-20 整体估算由约 99.8% 提升到约 99.9%。
- 应用运行组装迁入 `src/app/vocekit_application_runtime.*`，`voiceassistant.cpp` 从 334 行缩减到 8 行。
- 启动架构测试新增兼容入口行数和职责约束，禁止入口直接创建 Qt 应用、设置存储或主窗口。
- 全量 122 个测试工程完成验证：120 个 QtTest 程序共 817 项通过，2 个独立程序成功，失败和跳过均为 0。
- 第 11 项由基本完成提升到 100%；11-20 的代码拆分目标已全部达到，剩余工作是旧契约测试迁移和人工核心流程验收。
- 删除两份仍扫描旧主文件和已删除 `.inc` 的 PowerShell 契约；对应行为迁入 `HubSettingsState`、结果流程和 `ResultChoicePopup` 的可执行 Qt 组件测试。
- 新增逐功能录音与结果设置持久化测试，以及结果按钮显隐、透明度约束和单次完成回调测试。
- 全量首次运行发现本地 HTTP 假服务器偶尔在 POST 请求体收完前关闭连接；测试服务器改为读取完整请求后再响应，完整网络执行器测试连续 10 次通过。
- 全量 122 个测试工程最终验证通过：120 个 QtTest 程序共 821 项通过，另有 2 个独立程序成功，失败和跳过均为 0；`cppcheck`、`git diff --check` 和生成文件残留检查通过。

## 后续优先顺序

1. 按最终验收清单复核第 11-20 项，并执行一次人工核心流程回归。
2. 完成发布测试包和隐私检查。
3. 为下一阶段画布式输入输出编排整理稳定扩展点，不提前改变现有功能行为。
