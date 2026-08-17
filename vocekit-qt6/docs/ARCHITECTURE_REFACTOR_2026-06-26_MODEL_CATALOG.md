# 2026-06-26 大模型目录拆分

## 改动

- 新增 `src/providers/model_catalog.h/.cpp`。
- 把内置大模型列表、自定义大模型列表、模型标题显示、旧模型名兼容转换从 `voiceassistant.cpp` 拆出。
- 自定义大模型现在复用 `SecretConfig::effectiveCustomModels()`，不再由主窗口直接解析 `config/secrets.json`。

## 目的

- 主窗口只负责界面和功能编排，不再关心大模型列表的存储细节。
- 后续继续增加自定义大模型、模型测试、模型分组时，只需要改模型目录模块。
- `AppSettings` 仍然可以继续使用 `normalizeModelId()`，保持旧配置迁移逻辑不变。

## 验证重点

- 设置页、功能自定义页、结果小框里的模型下拉仍能显示内置模型和自定义模型。
- 旧配置里保存的 `gpt-*`、`claude-*`、`custom:*` 模型名仍能正常归一化。
