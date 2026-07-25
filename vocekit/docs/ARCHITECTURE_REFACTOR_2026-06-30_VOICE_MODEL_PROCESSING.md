# 2026-06-30 语音模型处理拆分

## 本轮目标

继续把 `voiceassistant.cpp` 里的业务流程拆到独立模块中，优先处理语音功能执行管线里的“大模型处理阶段”。

## 改动

- 新增 `src/tasks/voice_model_processing_task.h/.cpp`。
  - 集中处理听写、翻译、问答和自定义功能的大模型提示词拼装。
  - 统一调用 `runModelProviderRequestTask`。
  - 统一返回输出文本、错误、耗时和提示词版本。
- `VoiceController` 保留原来的 `processDictate`、`processTranslate`、`processAsk`、`processCustom` 入口。
  - 这些函数现在只负责组装请求并调用 `processVoiceModelRequest`。
  - 词库提示词注入和模型可用性检查通过回调传给任务模块。
- 新增 `src/tasks/screenshot_text_action_task.h/.cpp`。
  - 集中处理截图工具栏里的智能整理、翻译、润色和总结。
  - `VoiceController` 只负责截图界面状态、日志和回调，不再直接创建 `ApiClient`。
- `VocabularyStore` 新增 `appendEntry`。
  - 词条保存、生成 ID、校验无修正效果、写入 JSON 都回到存储层。
  - `VoiceController` 不再直接操作词库 JSON 列表。
- 新增 `src/domain/function_catalog.h/.cpp`。
  - 功能标题和自定义功能查找集中到领域层。
  - `HubWindow` 和 `VoiceController` 不再各自扫描内置功能和自定义功能列表。
- `vocekit.pro` 已加入新任务模块。
- `voiceassistant.cpp` 不再直接引用 `ModelRequestTaskRequest` 和 `ModelRequestTaskResult`。

## 架构收益

- 大模型阶段从 UI 控制器中分离出来，后续迁移 `VoiceController` 时阻力更小。
- 提示词拼装逻辑集中到任务模块，后续要改翻译、问答、自定义功能的模型请求，不需要继续扩大主窗口文件。
- 任务模块没有直接依赖窗口、浮动条、历史页和结果小框，便于继续测试和替换。
- 截图工具栏的 AI 操作也开始走任务层，后续做 OCR/截图工作流重构时可以直接复用。
- 词库写入逻辑更集中，后续历史、结果小框、快捷键加入词库都可以走同一个存储入口。
- 功能名称和自定义功能查找有了统一入口，后续拆 `HubWindow` 和 `VoiceController` 时可以少带重复逻辑。

## 验证

- `qmake` 通过。
- `mingw32-make -j2` 通过。
- `tests/tasks/release/model_request_task_tests.exe -txt` 通过，4 项。
- `tests/storage/release/vocabulary_store_tests.exe -txt` 通过，7 项。
- `tests/config/release/app_settings_json_tests.exe -txt` 通过，10 项。
- `tests/capture/release/screenshot_core_tests.exe -txt` 通过，12 项。

## 未完成

- `VoiceController` 类本体仍在 `voiceassistant.cpp`。
- `HubWindow` 仍在 `voiceassistant.cpp`。
- 页面类仍有一部分是 `src/pages/*_methods.h` 方法集合，还不是独立 QWidget。
- 模型任务已经走 Provider 入口，但旧 `ApiClient` 仍然承担底层实际请求。

## 2026-06-30 继续拆分补充

- 新增 `src/ocr/screenshot_ocr_config.h/.cpp`。
  - 负责把 OCR 引擎设置、RapidOCR/Windows OCR 辅助程序路径、云端 OCR 密钥和超时设置转换成 `OcrManagerConfig`。
  - `VoiceController` 不再直接拼接 OCR 程序路径，也不再直接把 OCR 密钥字段塞进配置。
- 新增 `src/domain/vocabulary_runtime.h/.cpp`。
  - 负责判断本次运行是否启用词库。
  - 负责把相关词库条目注入大模型提示词。
  - 负责输入预修正和输出后修正。
  - `VoiceController` 保留日志和 UI 状态，但不再直接组合词库运行策略。
- `vocekit.pro` 已加入这两个新模块。

## 2026-06-30 继续验证

- `qmake` 通过。
- `mingw32-make -j2` 通过。
- `tests/tasks/release/model_request_task_tests.exe -txt` 通过：4 项。
- `tests/storage/release/vocabulary_store_tests.exe -txt` 通过：7 项。
- `tests/capture/release/screenshot_core_tests.exe -txt` 通过：12 项。
- `tests/config/release/app_settings_json_tests.exe -txt` 通过：10 项。

## 2026-06-30 补充验证

- `tests/ocr/release/ocr_core_tests.exe -txt` 通过：29 项。
- `tests/recording/release/recording_core_tests.exe -txt` 通过：13 项。
