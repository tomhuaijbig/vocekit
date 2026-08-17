# 2026-06-25 架构拆分继续记录

## 本轮完成

- 恢复并修正了 `src/voiceassistant.cpp` 在一次整文件重写后产生的编译损坏；后续禁止再用会重写整文件编码的命令处理这个文件。
- 选中文字读取已经抽到 `src/input/selected_text_reader.*`，主流程和词库快捷键都改为调用这个模块。
- 输入收集基础逻辑已经抽到 `src/input/voice_input_collector.*`，选中文字读取和词库预修正从 `VoiceController` 中移出一部分。
- 结果小框和历史输入文本格式化已经抽到 `src/domain/voice_run_formatter.*`，并在 `tests/domain/domain_types_tests.cpp` 中加入覆盖。
- 长录音分段识别任务外壳已经抽到 `src/tasks/speech_recognition_task.*`，主文件只负责排队、日志和接收结果。
- 历史记录存储继续使用 `src/storage/history_store.*`，主窗口删除了重复的 `HistoryEntry` 结构体，改用 domain 层 `HistoryEntry`。
- 密钥配置继续使用 `src/config/secret_config.*` 和 `src/config/secret_store.*`，主文件删除了旧 `SecretConfig`、旧 `loadSecrets()` 和旧 `saveSecrets()`，避免重复定义和链接冲突。
- 旧 `src/modules/*.inc` 引用已经替换为当前的 `src/ui`、`src/pages`、`src/api`、`src/recording` 等模块路径。

## 验证结果

- `qmake vocekit.pro` + `mingw32-make`：Release 主程序编译通过。
- `tests/domain/release/domain_types_tests.exe`：11 passed。
- `tests/config/release/app_settings_json_tests.exe`：10 passed。
- `tests/config/release/app_settings_defaults_tests.exe`：6 passed。
- `cppcheck` 检查新拆分模块：未发现新增问题。

## 下一步建议

1. 继续把 `VoiceController` 拆成“输入收集、语音识别、模型处理、结果输出”四个阶段，其中输入收集已经开始。
2. 把 `ApiClient` 里的百度、讯飞、自定义语音、大模型请求逐个迁移到 `src/providers/`，每次迁移一个 provider 并编译验证。
3. 继续把 `HubWindow` 页面方法改成真正的独立 `QWidget` 类，优先选择历史页或词库页。
