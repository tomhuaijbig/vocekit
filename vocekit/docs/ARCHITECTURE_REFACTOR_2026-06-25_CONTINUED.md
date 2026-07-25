# 2026-06-25 架构拆分续作

## 本次完成

- `AppSettings::load()` 已经只通过 `AppSettingsStore` 读取 `config/settings.json`，不再保留同函数内那段已经不会执行的旧手写 JSON 解析代码。
- 新增 `src/domain/voice_run_context.*`，把一次功能执行中的选中文字、语音文本、截图 OCR 结果、网络策略等运行上下文从 `VoiceController` 里抽出来。
- `VoiceController` 现在使用 `VoiceRunContext` 传递模型生成、结果小框、截图结果窗、历史输入和现场恢复所需数据。
- `VoiceRunContext` 增加了 `hasSelectedText()`、`hasVoiceText()`、`hasTextOnlyInput()`、`hasScreenshotText()`，后续拆输入收集、语音识别、模型处理、结果输出时可以复用这些判断。
- `tests/domain/domain_types_tests` 增加 `VoiceRunContext` 覆盖，避免后续继续拆分时把基础输入状态判断改坏。

## 验证

- `tests/domain/domain_types_tests.exe`：8 passed。
- `tests/config/app_settings_json_tests.exe`：10 passed。
- `tests/config/app_settings_defaults_tests.exe`：6 passed。
- `qmake vocekit.pro` + `mingw32-make`：主程序 Release 编译通过。
- `cppcheck` 检查新上下文文件通过；对 `legacy_app_settings.h` 的提示为既有手写循环风格建议，不是本轮新增错误。

## 后续建议

1. 继续拆 `VoiceController` 时，优先围绕 `VoiceRunContext` 抽出“输入收集”阶段。
2. `ApiClient` 现在仍依赖主 cpp 内较多静态工具函数，迁移到 `src/providers/` 前应先把模型 ID、网络错误整理和请求辅助函数抽成可独立包含的模块。
3. 页面拆分仍应按“历史页、词库页、设置页”逐个从 `HubWindow` 私有成员里移出，避免一次性改动过大。
