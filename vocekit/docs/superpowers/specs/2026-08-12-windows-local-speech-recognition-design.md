# vocekit Windows 本地语音识别设计

## 目标

在现有讯飞、百度和自定义语音服务之外，增加可选择的“Windows 本地语音识别”。它使用 Windows 已安装的桌面语音识别器，在本机完成中文或英文听写，并把临时结果实时显示在现有 C 型悬浮窗中。

该能力不调用 `Win + H`，不争抢麦克风，也不依赖云端账号或密钥。Qt 继续负责录音；独立助手进程只接收同一份 16 kHz、单声道、16-bit PCM 副本并进行识别。

## 已确认的产品规则

- Windows 本地语音识别是第四个可选语音服务商，不替换讯飞、百度或自定义服务。
- 录音期间实时显示识别文字；临时文字只用于预览，不能提前运行 AI 或写入目标应用。
- 用户确认后等待最终文本，再执行一次现有 AI、翻译、问答和写入流程。
- 实时链路失败时，使用同一 Windows 本地识别器和本次完整 PCM 做一次整段识别兜底，不静默切换到任何云服务。
- 语言设置提供“跟随 Windows”（默认）、“简体中文”和“English”。
- 缺少对应语言识别器时明确提示安装或切换 Windows 语音语言，并提供打开系统语言设置的操作，不静默选用其他语言。
- 现有 B 型和 C 型悬浮窗都可使用该服务；C 型显示临时和已确认文字，B 型只显示录音、定稿、成功或失败状态。
- 取消录音立即丢弃本次预览和结果，不运行整段兜底、AI 或写入。

## 可行性复核结论

本方案没有发现系统级或工程级阻断。

- 当前录音器已经输出 16000 Hz、单声道、16-bit、小端有符号 PCM，可直接传给 `SpeechAudioFormatInfo`，无需转码。
- `System.Speech.Recognition.SpeechRecognitionEngine` 支持从指定格式的 `Stream` 接收音频、以 `RecognizeMode.Multiple` 连续异步识别，并提供 `SpeechHypothesized`、`SpeechRecognized` 和 `RecognizeCompleted` 事件。
- 输入流结束时，识别器会使用已经收到的音频完成当前识别，因此“关闭助手进程 stdin”可作为正常确认信号。
- Windows 11 自带 .NET Framework 4.8 或 4.8.1；本机已实际枚举并实例化 zh-CN 与 en-US 的 Microsoft Speech Recognizer 8.0，且两种识别器均能加载自由听写语法。
- 主程序是 Qt 5.9/MinGW 32 位，但独立 64 位 .NET Framework 助手进程可以通过标准输入输出通信，不产生 ABI 混用。现有 OCR 助手已经验证了同类独立进程打包方式。

需要接受的产品限制是：Windows 桌面本地识别器属于传统离线识别能力，识别质量通常低于当前云端大模型语音服务；`SpeechHypothesized` 是不稳定的候选结果，可能频繁改写。因此候选文字必须有明确的临时样式，只有最终结果才能进入业务流程。

## 非目标

- 不模拟或自动触发 `Win + H`。
- 不让 Windows 助手进程自己占用麦克风。
- 不把 `Windows.Media.SpeechRecognition` 引入当前便携式程序；该 API 的当前桌面使用要求 MSIX 身份，会改变现有部署模型。
- 不在本次加入语言自动检测、说话人分离、时间戳或自定义声学模型。
- 不把 Windows 临时结果写入历史记录或当前输入框。
- 不在 Windows 识别失败时自动上传音频到讯飞或百度。

## 总体架构

新增独立助手 `vocekit-windows-speech.exe`，基于 .NET Framework 4.8 的 `System.Speech`。主程序通过一个新的 Windows 流式会话适配器管理助手进程，并继续复用现有录音、流式文字、悬浮窗和工作流边界。

数据流为：

1. Qt 录音器采集 PCM，并照常保存本次完整录音。
2. 当前 Windows 流式会话把 PCM 副本写入助手进程 stdin。
3. 助手内部的有界生产者/消费者音频流把持续输入交给 `SpeechRecognitionEngine`。
4. 助手把候选、已确认、最终或错误事件以逐行 JSON 写到 stdout。
5. Qt 会话解析事件，通过已有流式回调更新悬浮窗。
6. 用户确认时 Qt 关闭 stdin；助手在输入流结束后输出最终事件并退出。
7. 控制器只把唯一最终文本交给现有后续流程一次。

每次录音启动一个独立助手进程，不复用跨录音进程。这样可以天然隔离识别器状态、语言、音频缓冲和失败，不让上一次录音的事件进入下一次录音。

## 进程协议

### 启动参数

主程序以参数传入：

- 协议版本；
- 本次运行 ID；
- 工作模式 `stream` 或 `batch`；
- 解析后的语言标签；
- 采样率 16000、声道 1、位深 16。

助手启动后先校验参数、识别器和听写语法，再输出 `ready`。在收到 `ready` 前，Qt 最多暂存约两秒 PCM；超时、启动失败或缓存溢出均让流式会话进入降级状态，但录音本身继续。

### stdin

stdin 只传原始 PCM 字节，不夹带长度帧、结束命令或取消命令：

- 正常确认：Qt 停止继续写入并关闭 write channel；EOF 代表本次音频结束。
- 取消：Qt 立即停止写入并终止本次助手进程；即使助手随后输出内容也忽略。
- 异常退出：Qt 记录本地识别错误并进入既定兜底或失败路径。

二进制音频与控制消息不复用同一通道，可避免半包、转义、误解析和结束命令混入音频的问题。

### stdout 与 stderr

stdout 只允许 UTF-8、单行 JSON 事件：

- `ready`：助手和语言识别器可用；
- `hypothesis`：可反复改写的当前候选文字；
- `recognized`：已经确认的一个文本片段；
- `final`：本次唯一完整最终文本；
- `error`：稳定错误码和可显示的简短消息。

所有事件携带协议版本和运行 ID。Qt 还以当前控制器运行 ID 和会话存活状态二次门禁，忽略旧进程或已取消会话的迟到事件。

stderr 只用于诊断，不作为机器协议。日志不得包含完整音频、密钥或完整识别文本；必要时只记录文本长度、语言、阶段和错误类别。

## 助手内部识别行为

- 使用与所选语言精确匹配的 `RecognizerInfo` 创建 `SpeechRecognitionEngine`。
- 加载 `DictationGrammar`，然后以 `RecognizeMode.Multiple` 启动连续异步识别。
- `SpeechHypothesized` 更新当前候选片段，不把候选追加为永久文本。
- `SpeechRecognized` 把该片段追加到已确认文本，并清空当前候选。
- 输入 EOF 后等待 `RecognizeCompleted`，拼接所有已确认片段并输出一次 `final`。
- 空结果不是成功；助手返回稳定的“未识别到语音”错误。
- 候选事件允许节流或同值去重，避免 UI 被高频重复消息淹没。
- 助手内部音频流使用有界缓冲和条件等待；读取方阻塞时不得无限占用内存。

## 语言解析

配置值为：

- `follow-windows`；
- `zh-CN`；
- `en-US`。

显式语言只接受对应的已安装识别器；没有精确匹配则报错。

“跟随 Windows”按以下顺序解析：

1. 读取当前 Windows UI culture；
2. 查找语言标签完全匹配的已安装识别器；
3. 查找相同中性语言的已安装识别器；
4. 仍无匹配则返回缺少语言组件错误，并列出当前可用语言。

它不因为设备上存在 zh-CN 就强制选择中文，也不自动改用 en-US。解析后的实际语言在设置详情和测试结果中显示，便于用户确认。

## Qt 侧模块边界

### Provider 与配置

新增稳定 provider ID `windows-local`，接入以下现有统一入口：

- `supportedSpeechProviderIds`、规范化、标题和默认值；
- 内置 Provider 工厂与配置校验；
- 语音接口设置、功能设置、画布节点和托盘选择器；
- 接口自检与运行时可用性判断。

需要清理当前功能页和托盘内硬编码的服务商列表，让所有入口都使用同一 provider 目录，避免只在部分界面出现 Windows 选项。

Windows Provider 不需要 API Key。它的配置有效性由助手文件存在、可启动及选定语言识别器存在共同决定。

### 流式会话

在现有 `StreamingSpeechSession` 工厂中增加 Windows 会话。会话使用 `QProcess`：

- 异步启动与读写，不阻塞录音线程或 Qt 主线程；
- 在 `bytesWritten` 驱动下继续排空有界 PCM 队列；
- 逐行解析 stdout JSON，并限制单行和总缓冲大小；
- 进程异常、协议错误、写入积压或启动超时只上报一次降级；
- `finish` 关闭 stdin 并等待最终结果；
- `cancel` 先断开业务回调，再 terminate，短暂超时后 kill。

### 整段兜底

Windows 流式会话启动失败或中途失败后，控制器继续保存完整 PCM。用户确认时再启动一个 `batch` 助手，把完整 PCM 写入后关闭 stdin：

- 仍使用同一语言和 Windows 本地识别器；
- 不连接讯飞、百度或自定义服务；
- 成功后用整段最终文本替换临时预览；
- 失败后保留录音文件，显示具体原因和“打开 Windows 语言设置”操作；
- 整段兜底最多执行一次，最终工作流最多执行一次。

## 设置与界面

在“设置 > 接口 > 语音识别”中增加“Windows 本地”：

- 服务说明：本机离线识别，无需密钥；
- 语言下拉：跟随 Windows、简体中文、English；
- 当前状态：助手存在、实际识别语言、已安装识别器；
- “测试”按钮：启动助手自检，验证识别器和听写语法可加载，不占用麦克风；
- 缺失组件时显示明确原因和打开 Windows 语言设置的按钮。

功能页、画布节点和托盘菜单均可选择 Windows 本地。语言是全局接口设置，不为每个功能复制一套语言配置。

旧配置没有 Windows 字段时保持原服务商和行为不变。新增语言字段缺失时采用 `follow-windows`。

## 悬浮窗与工作流

- C 型悬浮窗把“已确认文字”以普通前景色显示，把当前候选文字以蓝色显示；新的候选快照原位替换旧候选，不重复追加。
- B 型悬浮窗不显示正文，只显示“录音中”“正在定稿”“正在重新识别”等简洁状态。
- 点击对号：停止采集、关闭 helper stdin、显示“正在定稿”，等待最终结果。
- 点击叉或按 Esc：取消采集、终止 helper、清空预览，不进入任何后续流程。
- 流式失败：C 型保留最后预览并提示“实时识别已中断，确认后将本地重新识别”；B 型只显示降级状态。
- 最终成功：替换全部候选文本，然后沿用现有 AI、写入和写入失败弹出结果框规则。
- 最终失败：不运行 AI 或写入，保留录音，显示诊断和修复入口。

## 状态机

主要成功路径：

`StartingHelper -> RecordingStreaming -> Finalizing -> Completed`

流式失败后的本地兜底：

`StartingHelper/RecordingStreaming -> StreamingDegraded -> RecognizingLocalBatch -> Completed/Failed`

缺少语言组件：

`StartingHelper -> ConfigurationFailed`

取消：

`StartingHelper/RecordingStreaming/Finalizing -> Cancelled`

每条路径都由运行 ID、显式状态和“最终结果已交付”标记约束。只有从有效状态进入 `Completed` 的第一个非空最终结果才能触发后续工作流。

## 超时与资源限制

- 助手启动并返回 `ready`：目标 2 秒，硬超时 5 秒。
- PCM 待写队列：最多约 2 秒音频；溢出即降级，不阻塞采集。
- 用户确认后的最终结果：目标 2 秒，硬超时 8 秒；超时后终止助手并进入本地整段兜底。
- 整段兜底：按音频时长设置有上限的超时，最少 15 秒，最长 120 秒。
- stdout 单行和累计缓冲必须设上限；非法 UTF-8、非法 JSON、运行 ID 不匹配或未知必要事件均视为协议错误。
- 应用退出时先取消活动会话，再回收助手；不能遗留 `vocekit-windows-speech.exe` 进程。

## 错误分类和用户提示

至少区分：

- 助手缺失或无法启动；
- .NET Framework/System.Speech 不可用；
- 所选语言识别器未安装；
- 听写语法加载失败；
- PCM 格式或协议版本不匹配；
- 写入队列溢出；
- 助手异常退出或最终结果超时；
- 未识别到语音；
- 用户取消。

配置错误在录音开始前尽量拦截。用户取消不是错误。提示不得建议输入 API Key；缺语言组件时直接指向 Windows 的“时间和语言 > 语言和区域/语音”设置。

## 构建、部署和兼容

- 新助手源码位于独立 helper 目录，目标为 .NET Framework 4.8、x64、Release。
- 交付路径固定为 `speech/windows/vocekit-windows-speech.exe`，运行时通过应用目录解析，不依赖工作目录或 PATH。
- 构建脚本先生成助手，再执行 Qt 部署；部署和运行时验证脚本都必须检查 helper 存在并能完成 `--probe` 自检。
- Windows 11 自带所需 .NET Framework 版本，不随应用复制系统 `System.Speech.dll`；若运行时探测失败，显示可诊断错误。
- 主程序仍保持 Qt 5.9、MinGW 和 C++11 兼容，不在 Qt 进程中链接 .NET 或 Windows Runtime。
- 打包测试必须在清空开发工具 PATH 的环境中启动交付目录，验证 Qt DLL、平台插件和语音助手均能被找到。

## 测试策略

### 助手单元和协议测试

- 参数、协议版本、PCM 格式和语言解析。
- 已安装识别器精确匹配、中性语言匹配和缺失错误。
- hypothesis 替换、recognized 追加、final 唯一交付和空结果。
- stdin EOF 正常完成、异常 stdin、取消和超时。
- stdout 始终为逐行 UTF-8 JSON，stderr 不污染协议。
- 有界音频流在生产快于消费、EOF 和取消时不会死锁或无限增长。

### Qt 适配器测试

使用假的助手进程或可控测试 helper，不依赖真实麦克风：

- 正确启动参数和原始 PCM 字节完整性；
- ready 前缓冲、异步排空和队列溢出；
- 拆分 JSON 行、连续多行、非法 JSON、超长行和异常退出；
- finish 只关闭 stdin 并等待 final；cancel 不交付结果；
- 旧运行 ID 和取消后的迟到事件被忽略；
- 流式失败只触发一次 Windows batch 兜底，绝不调用云 Provider；
- 最终工作流和写入最多调用一次。

### 设置和选择器测试

- Windows provider 在接口、功能、画布和托盘四处都出现，且使用同一 ID/标题。
- 语言设置可保存、刷新和兼容旧配置。
- 无密钥卡片不会误显示 API Key 输入框。
- 自检成功、缺助手、缺中文、缺英文和打开系统设置操作。

### 真实系统冒烟测试

在 Windows 11 真实环境分别用固定 zh-CN 和 en-US PCM 样本验证：

- `--probe` 能列出并加载两种识别器；
- 流式模式产生候选或已确认事件并最终结束；
- batch 模式返回最终结果或稳定的“未识别”错误；
- 不连接网络，不使用真实云密钥。

真实识别文本可能因机器语言包和声学结果存在差异，冒烟测试只检查进程、事件、语言和非崩溃行为；确定性文字拼接由假事件测试负责。

### UI、回归和交付验证

- B/C 悬浮窗覆盖录音、候选修正、定稿、降级、成功、失败和取消。
- 中文长文本、英文长文本、100%/125%/150% 缩放和放大字体下无裁切；叉、对号和状态始终可操作。
- 全部现有语音、录音、工作流、写入、设置、画布和托盘测试通过。
- Qt Release 主程序和 .NET Release 助手均从干净目录构建。
- 完整测试套件、真实 widget 截图、部署验证和干净 PATH 启动全部通过。

## 验收标准

- 用户能在所有语音服务商选择入口选择“Windows 本地”。
- 选择 C 型悬浮窗时，录音期间能看到可被原位修正的临时文字；B 型保持简洁状态。
- 确认后只使用最终文本执行一次 AI/写入；取消不产生任何后续操作。
- 流式助手失败时，使用相同 Windows 引擎对完整 PCM 兜底，不上传云端。
- 缺少语言识别器时给出明确语言和修复入口，不静默换语言或服务商。
- 旧配置和原三个服务商行为不变。
- 无录音线程阻塞、无限缓冲、遗留助手进程、迟到事件串入下一次录音或重复执行。
- 自动化测试、真实中英文识别器冒烟、Qt Release、helper Release、缩放视觉检查和可运行包验证全部通过。

## 官方依据

- `SpeechRecognitionEngine.SetInputToAudioStream` 与输入流结束语义：<https://learn.microsoft.com/en-us/dotnet/api/system.speech.recognition.speechrecognitionengine.setinputtoaudiostream?view=netframework-4.8.1>
- 连续异步识别及识别事件：<https://learn.microsoft.com/en-us/dotnet/api/system.speech.recognition.speechrecognitionengine.recognizeasync?view=netframework-4.8.1>
- 临时候选事件的性质：<https://learn.microsoft.com/en-us/dotnet/api/system.speech.recognition.speechrecognitionengine.speechhypothesized?view=netframework-4.8.1>
- 取消异步识别：<https://learn.microsoft.com/en-us/dotnet/api/system.speech.recognition.speechrecognitionengine.recognizeasynccancel?view=netframework-4.8.1>
- 自由听写语法：<https://learn.microsoft.com/en-us/dotnet/api/system.speech.recognition.dictationgrammar?view=netframework-4.8.1>
- 枚举已安装识别器：<https://learn.microsoft.com/en-us/dotnet/api/system.speech.recognition.speechrecognitionengine.installedrecognizers?view=netframework-4.8.1>
- Windows 11 包含 .NET Framework 4.8/4.8.1：<https://learn.microsoft.com/en-us/dotnet/framework/install/>
- 当前 `Windows.Media.SpeechRecognition` 桌面应用打包要求：<https://learn.microsoft.com/en-us/windows/apps/develop/input/speech-recognition>
- Windows 语音键入的焦点与联网行为：<https://support.microsoft.com/en-us/windows/use-voice-typing-to-talk-instead-of-type-on-your-pc>
