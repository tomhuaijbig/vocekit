# 2026-06-25 架构拆分进度

## 本轮完成

- 新增 `src/config/app_settings_defaults.*`，把模型默认值、输出方式、结果模板、词库加入方式、语音服务、OCR 引擎和内置功能默认输入方式从 `voiceassistant.cpp` 迁出。
- `src/config/app_settings_json.cpp`、`src/config/legacy_app_settings.h` 和主程序现在共用同一套设置默认值，减少旧 `AppSettings` 与新 `AppSettingsStore` 行为不一致的风险。
- `AppSettingsStore` 新增 `replaceSnapshot()`，可以一次接收完整 `AppSettingsData`。
- 旧 `AppSettings` 新增 `toData()`，把当前旧设置对象导出为新的 `AppSettingsData`。
- 旧 `AppSettings::save()` 已切到 `AppSettingsStore` 写入，不再手写 `settings.json` 的大块 JSON 组装逻辑。
- 旧 `AppSettings::load()` 已优先通过 `AppSettingsStore::load()` 读取，再用 `applyData()` 灌回旧运行时字段；主程序仍能沿用原来的 `AppSettings` 调用方式。
- `resetDefaults()` 现在会显式重置提示词锁定状态，避免同一个设置对象重复加载时保留旧状态。
- 新增 `tests/config/app_settings_defaults_tests.*`，并在 `app_settings_json_tests` 中补充完整快照保存测试。

## 验证结果

- `tests/config/app_settings_defaults_tests.exe`：6 passed。
- `tests/config/app_settings_json_tests.exe`：10 passed。
- `cppcheck` 检查 `app_settings_defaults.cpp`、`app_settings_json.cpp`、`app_settings_store.cpp`：通过。
- `qmake vocekit.pro` + `mingw32-make`：主程序 Release 编译通过。第一次链接失败是因为 `release/vocekit.exe` 正在运行，结束进程后重新链接通过。

## 下一步建议

1. 清理 `AppSettings::load()` 里已经不再走的旧 JSON 手写解析代码，让旧设置中心只保留迁移桥和兼容访问函数。
2. 开始拆 `VoiceController` 的四个阶段，建议先抽“输入收集”阶段，因为它和模型请求、结果写入耦合相对较少。
3. 拆输入收集时先只移动纯数据结构和选中文字、截图、语音输入的上下文对象，不要同时改快捷键触发和 UI。
