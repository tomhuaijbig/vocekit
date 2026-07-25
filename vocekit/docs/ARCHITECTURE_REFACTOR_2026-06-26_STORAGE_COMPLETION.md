# 2026-06-26 存储模块收口

本轮目标是先把已经“基本完成”的存储模块继续做实，减少 `voiceassistant.cpp` 里的重复实现。

## 已完成

1. 新增 `src/storage/vocabulary_store.h/.cpp`
   - 负责词库 JSON 读写。
   - 负责词库作用范围、匹配方式、CSV 导入导出辅助。
   - 负责文本替换。
   - 负责按上下文生成最多 N 条相关词条的模型提示块。
   - 主程序和页面仍可通过原有函数名调用，避免一次性重写页面代码。

2. 主文件移除词库重复实现
   - `loadVocabularyEntries`
   - `saveVocabularyEntries`
   - `applyVocabularyEntries`
   - `vocabularyPromptBlock`
   - 词库 CSV、匹配、唯一键等辅助逻辑

3. 主文件移除百度示例代码解析重复实现
   - 改为直接使用已有 `src/config/baidu_sample_parser.*`。

4. 历史目录兼容函数改为委托 `HistoryStore`
   - 目录名、安全文件名、路径归一化、历史可读文本生成等规则集中到 `HistoryStore`。
   - 旧页面仍使用原函数名，降低改动风险。

5. 新增 `tests/storage/vocabulary_store_tests.*`
   - 覆盖词库保存过滤、按作用范围替换、相关词条提示块限制、CSV 解析和转义。

## 当前状态

- `HistoryStore`：存储规则已经集中，页面层仍保留 UI 包装逻辑。
- `VocabularyStore`：词库存储和规则已经独立，词库页面仍挂在 `HubWindow` 方法头文件里。
- `voiceassistant.cpp`：本轮继续减小，但 `HubWindow`、`VoiceController` 和模型调用流程仍在里面。

## 后续建议

1. 继续把 `ApiClient` 的实际语音和模型请求迁移到 `src/providers/`。
2. 把词库页面改成独立 `QWidget` 后，直接持有 `VocabularyStore`，不再依赖兼容函数。
3. 把历史页面改成独立 `QWidget` 后，直接持有 `HistoryStore`，删除剩余兼容包装。
