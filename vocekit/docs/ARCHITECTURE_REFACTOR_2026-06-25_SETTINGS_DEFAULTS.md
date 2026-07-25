# 2026-06-25 架构拆分记录：设置默认值统一

## 本次改动

- `src/voiceassistant.cpp` 引入 `src/config/app_settings_defaults.h`。
- 删除主文件里重复定义的默认模型、输出方式、结果模板、词库加入方式、功能默认输入方式、语音服务和 OCR 引擎函数。
- 这些默认值现在统一由 `src/config/app_settings_defaults.cpp` 提供。

## 目的

- 避免同一个配置含义在多个文件里各写一份。
- 后续修改模型名称、输出方式、语音识别服务、OCR 引擎或默认开关时，只需要改配置默认值模块。
- 降低 `voiceassistant.cpp` 的体积，让主流程更专注于界面和业务串联。

## 验证

- `qmake vocekit.pro`
- `mingw32-make`
- 构建通过后会生成 `release/vocekit.exe`。
