# 2026-06-26 AppSettings 拆分记录

## 本次完成

- `voiceassistant.cpp` 不再直接定义 `AppSettings` 大类。
- 原有设置读写逻辑集中到 `src/config/legacy_app_settings.h`。
- `HotkeyDef`、`CustomFunctionDef`、`PromptLibraryItem`、词库相关结构和 `ModelOption` 改为统一使用 `src/domain/app_legacy_types.h`。
- `CustomFunctionDef` 保留 `resultActions` 字段，让自定义功能的结果按钮顺序可以跟随设置读写。
- 主程序仍然沿用原来的 `AppSettings` 调用方式，避免一次性改动 UI 和业务流程。

## 验证结果

- `qmake + mingw32-make` 构建通过。
- `domain_types_tests.exe` 通过。
- `app_settings_json_tests.exe` 通过。
- `app_settings_defaults_tests.exe` 通过。
- `cppcheck` 针对本次触碰文件检查通过。

## 后续建议

- 下一步可以继续把 `AppSettings` 的调用逐步迁移到 `AppSettingsStore`。
- 等调用侧稳定后，再把 `legacy_app_settings.h` 拆成 `.h/.cpp`，减少头文件体积。
- 功能页面解耦时，优先从只读配置入口开始，避免同时改动保存逻辑和 UI 结构。
