# 2026-07-03 词库候选推荐弹窗独立化

## 本轮目标

继续拆分词库页，把“词库候选推荐”弹窗从 `HubWindow` 方法集合中移出，减少主窗口直接构建候选列表和候选卡片的 UI 代码。

## 改动内容

- 新增 `src/ui/vocabulary_candidates_dialog.h` 和 `src/ui/vocabulary_candidates_dialog.cpp`。
- `VocabularyCandidatesDialog` 负责：
  - 展示词库候选推荐弹窗。
  - 渲染候选卡片、候选范围、出现次数和推荐原因。
  - 处理“编辑”“加入”“关闭”按钮的界面状态。
- `HubWindow::showVocabularyCandidatesDialog()` 现在只负责：
  - 收集候选词条。
  - 创建候选弹窗。
  - 提供“编辑候选”和“加入候选”的回调。

## 架构影响

- 对任务 11 有推进：主窗口和词库方法头文件继续减少 UI 细节。
- 对任务 12 有推进：词库候选推荐成为独立 `QDialog` 类。
- 对任务 17 有推进：候选列表展示与词条保存动作通过回调连接，UI 与业务逻辑进一步分离。

## 尚未完成

- 候选生成算法 `collectVocabularyCandidates()` 仍在 `hub_vocabulary_page_methods.h` 中。
- 词库导入、导出、删除动作还由 `HubWindow` 承担。
- 后续可以把候选生成拆到领域模块，把导入导出保存拆到词库应用服务。

## 验证

- `qmake` 重新生成 Makefile。
- `mingw32-make -j2` 编译通过。
- release 测试共 25 个，全部通过。
