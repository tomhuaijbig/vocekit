# 2026-06-26 全局快捷键拆分记录

## 本次完成

- 新增 `src/input/global_hotkeys.h` 和 `src/input/global_hotkeys.cpp`。
- `GlobalHotkeys` 从 `voiceassistant.cpp` 移出，主文件不再直接实现 Windows `RegisterHotKey` 细节。
- 新增 `GlobalHotkeySettingsSnapshot` 和 `GlobalHotkeyFunction`，让快捷键模块只接收需要注册的数据，不直接依赖完整 `AppSettings`。
- 主程序新增 `globalHotkeySnapshotFromSettings()`，负责把当前设置转换为快捷键快照。
- 全局快捷键注册继续支持普通功能快捷键、独立截图快捷键和按住说话模式。

## 为什么这样拆

- 快捷键注册是输入层能力，不应该长期留在主窗口文件里。
- 快捷键模块不直接读取设置对象，后续可以更容易做快捷键冲突提示、单独测试和替换输入实现。
- 复用已有 `parseNativeHotkey()`，避免主文件和输入模块各维护一套快捷键解析逻辑。

## 验证结果

- `qmake + mingw32-make` 构建通过。
- `domain_types_tests.exe` 通过。
- `app_settings_json_tests.exe` 通过。
- `app_settings_defaults_tests.exe` 通过。
- `hotkey_parser_tests.exe` 通过。
- `hotkey_definitions_tests.exe` 通过。
- `screenshot_core_tests.exe` 通过。
- `cppcheck` 针对本次触碰文件检查通过。

## 后续建议

- 继续把 `VoiceController` 按“输入收集、语音识别、模型处理、结果输出”拆开。
- 给 `GlobalHotkeys` 增加更细的注册结果结构，后续提示用户具体哪个快捷键冲突。
- 将主文件中的 `globalHotkeySnapshotFromSettings()` 继续下沉到设置适配模块。
