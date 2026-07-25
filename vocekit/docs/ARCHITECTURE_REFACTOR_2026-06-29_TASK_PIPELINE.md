# 2026-06-29 架构拆分记录：任务管线继续下沉

本轮继续减少 `src/voiceassistant.cpp` 的控制器职责，重点是把录音、模型和词库 AI 生成这些可独立执行的任务从主流程里下沉。

## 本轮改动

- `src/recording/segmented_recording.*`
  - 新增 `wavFromPcm()`，统一 WAV 封装逻辑。
  - `voiceassistant.cpp` 删除重复的静态 WAV 封装函数。

- `src/tasks/vocabulary_suggestion_task.*`
  - 新增词库 AI 生成任务。
  - 负责检查 DeepSeek 配置、拼接词库生成提示、调用模型、解析词条。
  - `VoiceController::suggestVocabularyEntry()` 保留为入口，但只委托任务模块执行。

- `src/domain/voice_run_executor.*`
  - `VoiceController::runContext()` 改为调用 `VoiceRunExecutor::run()`。
  - 听写、翻译、问答和自定义功能的分派规则进一步集中到执行器模块。

- `src/tasks/speech_recognition_task.*`
  - 新增 `runSpeechRecognitionProviderTask()`。
  - 普通录音识别和长录音分段识别都改为走统一任务入口。
  - 语音 Provider 创建、取消令牌、耗时和错误文本集中到任务模块。

- `src/tasks/model_request_task.*`
  - 新增 `runModelProviderRequestTask()`。
  - 大模型 Provider 创建从 `VoiceController::runModelRequest()` 下沉到任务模块。

- `src/ui/tab_bar_wheel_filter.*`
  - 从设置页方法头文件中拆出标签栏滚轮切换过滤器。
  - 历史页和设置页共享这个 UI 工具，不再由设置页隐式提供。

- `src/voiceassistant.cpp`
  - 删除无调用的旧 `showResultDialog()`。
  - 文件缩小到约 358 KB / 7522 行。

## 验证

- `qmake` 重新生成 Makefile。
- `mingw32-make -j2` 构建通过。
- `tests/tasks/release/model_request_task_tests.exe -txt` 通过：4 passed。
- `tests/storage/release/vocabulary_store_tests.exe -txt` 通过：6 passed。
- `tests/config/release/app_settings_json_tests.exe -txt` 通过：10 passed。

## 对架构目标的影响

- 目标 13：语音任务控制器继续变薄，语音识别 Provider 调用已移到任务模块。
- 目标 14：功能执行管线继续成形，`VoiceRunExecutor` 开始接管 `runContext()` 分派。
- 目标 15：Provider 抽象接入更深，语音和模型 Provider 创建不再散落在控制器主要流程里。
- 目标 17：UI 与业务分离继续推进，词库 AI 生成、录音识别和模型请求都更接近独立业务任务。

## 后续建议

1. 继续拆 `VoiceController`，优先把 `processDictate()`、`processTranslate()`、`processAsk()`、`processCustom()` 中的提示词构造和模型请求合并为独立 `VoiceRunModelProcessor`。
2. 把测试工具页中直接使用 `ProviderRegistry` 的接口自检逻辑搬到独立诊断服务，之后 `voiceassistant.cpp` 可以移除 `api_client_provider_adapters.h`。
3. 逐步把 `HubWindow` 页面方法头文件改成真正的 `QWidget` 页面类，先从测试工具页或词库页开始。
