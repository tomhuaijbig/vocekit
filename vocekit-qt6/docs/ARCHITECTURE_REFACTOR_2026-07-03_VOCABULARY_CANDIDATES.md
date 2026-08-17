# 2026-07-03 词库候选生成拆分

## 本次改动

- 新增 `src/domain/vocabulary_candidates.h/.cpp`，集中处理“从历史记录生成词库候选”的规则。
- `HubWindow::collectVocabularyCandidates()` 现在只组装历史记录、已有词条和自定义功能列表，然后调用领域模块。
- 新增 `tests/domain/vocabulary_candidates_tests.cpp`，覆盖重复计分、已有词条去重、自定义功能作用范围和数量限制。

## 为什么拆分

词库候选推荐原来写在 `hub_vocabulary_page_methods.h` 里，页面层同时负责 UI、历史读取、词条读取和推荐算法。拆出后：

- 推荐规则可以独立测试。
- 词库页面后续拆成独立 `QWidget` 时，不需要再移动这段业务逻辑。
- 历史页和词库页之间减少直接依赖，候选生成只依赖明确的数据输入。

## 后续衔接

- 继续把历史页、测试工具页从 `HubWindow` 的方法头文件拆成独立页面类。
- 后续如果要改候选规则，比如引入中文专名或词频权重，只改 `vocabulary_candidates` 模块和对应测试。
