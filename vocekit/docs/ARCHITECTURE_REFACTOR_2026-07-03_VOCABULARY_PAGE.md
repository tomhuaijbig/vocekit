# 2026-07-03 词库页面独立化

## 本轮目标

把词库页从 `HubWindow` 的方法集合里拆出为独立页面，减少主窗口同时负责页面布局、词条筛选、卡片渲染和业务动作的情况。

## 改动内容

- 新增 `src/ui/vocabulary_page.h` 和 `src/ui/vocabulary_page.cpp`。
- `VocabularyPage` 负责词库页标题、搜索框、分类标签、词条卡片、空状态和当前筛选结果。
- `HubWindow` 不再保存词库搜索框、词库标签页和搜索文字状态。
- `HubWindow` 通过 `VocabularyPageCallbacks` 继续提供新增词条、候选推荐、导入、导出、打开目录、编辑和删除动作。
- 词库导出继续按当前标签和搜索条件导出，筛选结果由 `VocabularyPage::currentFilteredEntries()` 提供。

## 架构影响

- 对任务 11 有推进：`voiceassistant.cpp` 继续减少主窗口内的 UI 状态。
- 对任务 12 有推进：词库页开始变成真正独立的 `QWidget` 类，不再由 `HubWindow` 直接拼装页面。
- 对任务 17 有推进：词库页展示逻辑和词库业务动作之间通过回调连接，后续可以继续把词库编辑弹窗、候选推荐弹窗拆出去。

## 尚未完成

- 词库新增/编辑弹窗仍在 `hub_vocabulary_page_methods.h` 里。
- 候选推荐弹窗仍在 `hub_vocabulary_page_methods.h` 里。
- 词库导入导出动作还由 `HubWindow` 承担，后续可继续拆到词库页面控制器或词库应用服务。

## 验证

- `qmake` 重新生成 Makefile。
- `mingw32-make -j2` 编译通过。
- release 测试共 25 个，全部通过。
