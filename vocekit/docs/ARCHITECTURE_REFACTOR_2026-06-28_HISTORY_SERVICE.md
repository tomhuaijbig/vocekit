# 2026-06-28 历史记录保存服务

## 本次调整

- 新增 `HistoryRecordService`。
- `VoiceController::saveHistory()` 不再直接调用 `HistoryRecordBuilder` 生成详情 JSON，也不再直接调用 `HistoryStore::appendRecord()`。
- 历史保存流程现在分成三层：
  - `VoiceController`：收集本次功能执行的输入、输出、模型、耗时和录音信息。
  - `HistoryRecordService`：把业务数据转换为历史详情并发起保存。
  - `HistoryStore`：负责目录、文本文件、详情 JSON、总记录和索引落盘。

## 为什么这样拆

- 历史记录字段会继续扩展，例如模型耗时、识别耗时、提示词版本、截图 OCR 信息和录音分段信息。
- 如果这些规则继续留在 `VoiceController`，主流程会越来越难测试。
- 现在历史保存规则可以通过存储层测试直接覆盖，不需要启动主窗口。

## 已验证

- `history_store_tests` 新增 `recordServiceBuildsMetadataAndAppendsRecord()`。
- 测试覆盖：
  - 详情 JSON 字段生成。
  - 结构化历史文件写入。
  - 历史索引追加。
