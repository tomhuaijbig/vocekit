# 2026-07-03 词条编辑弹窗独立化

## 本轮目标

继续压缩 `HubWindow` 里的词库页面代码，把“新增词条 / 编辑词条”的表单弹窗拆成独立 UI 类。

## 改动内容

- 新增 `src/ui/vocabulary_entry_dialog.h` 和 `src/ui/vocabulary_entry_dialog.cpp`。
- `VocabularyEntryDialog` 负责：
  - 构建词条编辑表单。
  - 展示标题栏问号说明。
  - 执行 AI 填充。
  - 校验原词、标准写法和修正效果。
  - 返回用户确认后的 `VocabularyEntry`。
- `HubWindow::showVocabularyEntryDialog()` 现在只负责：
  - 传入已有词条、作用范围和 AI 回调。
  - 接收弹窗返回的词条。
  - 写入 `config/lexicon/entries.json`。
  - 发布词库刷新事件。

## 架构影响

- 对任务 11 有推进：`voiceassistant.cpp` 和词库方法头文件继续减少 UI 实现细节。
- 对任务 12 有推进：词库相关 UI 正在从 `HubWindow` 方法集合迁移到独立 `QWidget/QDialog` 类。
- 对任务 17 有推进：词条编辑表单和词库文件保存逻辑分离，后续可以继续把保存动作拆到词库服务。

## 尚未完成

- 候选推荐弹窗还在 `hub_vocabulary_page_methods.h` 里。
- 词库导入、导出、删除动作还由 `HubWindow` 直接承担。
- 词库页面相关方法头文件仍然存在，后续还可以继续拆成更小的页面控制器或服务。

## 验证

- `mingw32-make -j2` 编译通过。
- release 测试共 25 个，全部通过。
