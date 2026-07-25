# 2026-06-28 历史详情字段构建拆分

## 本次改动

- 新增 `HistoryRecordBuilder`，集中生成历史详情 JSON 的业务字段。
- `VoiceController::saveHistory()` 不再直接拼接截图 OCR 字段、录音分段字段、输入来源字段和耗时字段。
- `LastRunContext` 到 `VoiceRunContext` 的转换集中到一个方法，减少重复赋值。
- `HistoryRecordBuilder::ocrEngineName()` 统一 OCR 引擎显示名称，避免截图流程和历史保存各自维护一套名称。

## 当前边界

- `HistoryRecordBuilder` 只负责业务字段。
- `HistoryStore` 仍负责文件路径、可读文本、JSON 落盘、总目录和索引。
- `VoiceController` 暂时仍负责决定当前功能是否使用模型、模型名、提示词版本和录音源路径。

## 验证

- `tests/domain/domain_types_tests.exe`：通过。
- 主程序 `vocekit.exe`：重新编译通过。
