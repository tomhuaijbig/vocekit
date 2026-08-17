# 2026-07-03 历史详情文本拆分

## 本次改动

- `history_text` 新增耗时格式化、识别文本提取、详情纯文本生成和详细导出 JSON 生成。
- 历史页里的 `historyElapsedText()`、`historyRecognizedText()`、`historyDetailPlainText()` 和 `historyEntryToJson()` 改为薄包装。
- 扩展 `history_text_tests`，覆盖详情文本、导出 JSON、识别文本提取和耗时显示。

## 为什么拆分

历史详情页和导出功能使用同一批字段。之前这些字段拼接规则写在页面方法里，后续拆历史页 UI 时会把导出规则、显示规则和页面布局一起搬，风险较高。

拆分后：

- 详情页、文本导出和 JSON 导出复用同一套领域文本规则。
- 页面只保留模型显示名映射和文件是否存在这类 UI/环境信息。
- 后续把历史页变成独立 QWidget 时，导出内容不需要再重写。
