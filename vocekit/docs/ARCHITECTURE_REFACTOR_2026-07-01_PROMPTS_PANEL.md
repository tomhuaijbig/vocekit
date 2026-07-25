# 架构拆分记录：提示词页独立化

更新时间：2026-07-01

## 本轮完成

- 新增 `src/ui/prompts_panel.h` 和 `src/ui/prompts_panel.cpp`。
- 将提示词库页面 UI、提示词搜索、提示词锁定、新增、保存、复制和删除逻辑从 `HubWindow` 移到 `PromptsPanel`。
- `HubWindow` 现在只保留 `PromptsPanel *m_promptsPanel` 和 `refreshPromptSelector()` 兼容包装。
- 保留 `HubWindow::promptTargetForId()` 和 `HubWindow::promptTargets()`，因为功能编辑界面仍然复用这些提示词查询能力。

## 对 11-20 架构任务的影响

- 11：继续减少 `voiceassistant.cpp` 体积，本轮从约 229 KB / 4531 行降到约 211 KB / 4157 行。
- 12：提示词页已经变成独立 QWidget 类；历史页、词库页、测试页、图片识别页和 FAQ 页仍需继续拆分。
- 17：提示词页的 UI 状态和提示词编辑逻辑从主窗口分离，降低 `HubWindow` 的页面职责。
- 20：提示词保存后仍通过回调触发主窗口刷新，后续可以逐步改成事件总线。

## 验证

- `qmake` + `mingw32-make -j2` 构建通过。
- release 下 20 个现有测试全部通过。

## 下一步建议

1. 继续拆 `HubWindow` 中的测试工具页，把测试项 UI 和搜索逻辑先移到独立面板。
2. FAQ 页可以拆，但它和测试工具页、错误编号跳转有交叉，建议等测试页独立后再动。
3. 历史页和词库页体积更大，建议拆之前先定义页面接口，避免一次性移动过多状态。
