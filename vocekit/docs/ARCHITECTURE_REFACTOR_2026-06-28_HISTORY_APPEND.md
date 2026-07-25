# 2026-06-28 历史保存下沉

## 本次改动

- `HistoryStore` 新增 `HistoryAppendRequest`、`HistoryAppendResult` 和 `appendRecord()`。
- 历史记录的文件名、目录创建、总文本、总详情、总录音复制、详情 JSON 和索引写入，统一由 `HistoryStore` 处理。
- `VoiceController::saveHistory()` 保留运行元数据组装，但不再直接写文本、JSON、总录音和索引文件。
- OCR 页的历史保存也改为调用 `HistoryStore::appendRecord()`。
- `history_store_tests` 新增“保存一条带录音的历史记录”测试，覆盖模式目录、总目录、文本、详情、录音和索引。

## 还没完成

- `VoiceController` 仍在 `voiceassistant.cpp` 里，后续还要继续拆成独立控制器。
- 历史页面仍负责缓存刷新和 UI 通知，所以保存后会调用 `HubWindow::notifyHistoryRecordSaved()`。
- 目前是过渡状态：文件落盘已经进入存储层，但业务元数据仍由语音流程生成。

## 验证

- `tests/storage/history_store_tests.exe`：通过。
- 主程序 `vocekit.exe`：重新编译通过。
