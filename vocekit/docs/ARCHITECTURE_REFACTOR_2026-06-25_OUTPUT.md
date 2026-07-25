# 2026-06-25 架构拆分记录：结果写入模块

## 本轮完成

- 删除 `src/voiceassistant.cpp` 中残留的旧 UI Automation 选中文字实现。
- 选中文字读取统一由 `src/input/selected_text_reader.*` 负责，主流程不再保留第二套读取逻辑。
- 新增 `src/output/clipboard_writer.*`，集中处理结果写入、粘贴到目标窗口、替换选中和恢复剪贴板文本。
- `ResultChoicePopup`、写入测试页和 `VoiceController` 都改为调用 `ClipboardWriter`。
- 主文件不再内联 `ClipboardBridge`、`sendCtrlKey`、`sendSingleKey`，写入逻辑有了独立模块入口。
- 新增 `src/output/result_output_router.*`，集中判断结果应该走自动写入、截图结果窗还是结果小框。
- `VoiceController::finishMode()` 改为调用 `ResultOutputRouter`，减少主流程里的展现分支判断。
- `tests/domain/domain_types_tests.cpp` 增加输出路由测试，覆盖自动写入、截图结果窗和默认结果小框三种路径。

## 验证结果

- `qmake vocekit.pro` + `mingw32-make`：Release 主程序编译通过。
- `tests/domain/release/domain_types_tests.exe`：11 passed。
- 输出路由接入后，`tests/domain/release/domain_types_tests.exe`：14 passed。
- `tests/config/release/app_settings_json_tests.exe`：10 passed。
- `tests/config/release/app_settings_defaults_tests.exe`：6 passed。
- `cppcheck` 检查 `clipboard_writer`、选中文字读取、输入收集、结果格式化和语音识别任务模块：未发现新增问题。

## 后续建议

1. 继续拆 `VoiceController` 的结果输出阶段，把“展示结果小框、截图结果窗、自动写入、保存草稿”整理成独立输出管线。
2. 把 `ApiClient` 的具体接口请求逐个迁移到 `src/providers/`，优先从大模型请求开始。
3. 把 `HubWindow` 页面方法逐步改成独立 `QWidget` 类，先从历史页或词库页开始。
