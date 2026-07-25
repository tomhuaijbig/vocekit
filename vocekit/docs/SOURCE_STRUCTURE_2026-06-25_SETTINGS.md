# 设置模块迁移说明

## 当前边界

- `src/config/app_settings_defaults.*` 是设置默认值和稳定 ID 的公共模块。旧 `AppSettings`、新 `AppSettingsStore`、配置 JSON 转换层和 UI 都应该复用这里，不要在 `voiceassistant.cpp` 里再复制一份默认值。
- `src/config/app_settings_data.h` 是新的明确设置结构。新功能如果需要持久化，优先扩展这里和 `app_settings_json.*`。
- `src/config/app_settings_json.*` 负责 `AppSettingsData` 与 `config/settings.json` 的互转，同时保留未知字段，避免迁移时丢配置。
- `src/config/app_settings_store.*` 是新的设置读写服务，使用 `QSaveFile` 原子保存，不直接操作 UI，也不弹窗。
- `src/config/legacy_app_settings.h` 仍然是旧运行入口。它现在通过 `toData()` 导出 `AppSettingsData`，通过 `applyData()` 从 `AppSettingsData` 灌回旧字段。

## 迁移约束

- 不要再给 `AppSettings::save()` 增加新的手写 JSON 字段，应改 `AppSettingsData` 和 `app_settings_json.*`。
- 不要在页面代码里直接解析 `settings.json`。
- 后续继续迁移时，优先清理 `AppSettings::load()` 里已经不再执行的旧 JSON 解析代码。
- 如果继续拆 `VoiceController`，先抽输入收集阶段的数据结构，不要同时改快捷键注册、模型请求和结果写入。
