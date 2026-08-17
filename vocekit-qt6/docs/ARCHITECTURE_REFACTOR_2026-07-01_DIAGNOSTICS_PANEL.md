# 架构拆分记录：测试工具页壳层独立化

更新时间：2026-07-01

## 本轮完成

- 新增 `src/ui/diagnostics_panel.h` 和 `src/ui/diagnostics_panel.cpp`。
- 将测试工具页的标题、搜索框、滚动列表、空状态和“常见问题匹配”按钮从 `HubWindow` 移到 `DiagnosticsPanel`。
- `DiagnosticsPanel` 通过 `addTestCard()` 接收测试卡片，通过 `setSearchText()` 支持外部跳转搜索。
- 测试工具页搜索常见问题时，通过 `FaqPanel::matchCount()` 统计匹配数量；点击匹配按钮仍会跳转到常见问题页。
- 具体诊断动作暂时仍保留在 `HubWindow`，下一步再按接口自检、网络诊断、麦克风测试等拆成独立任务模块。

## 对 11-20 架构任务的影响

- 11：继续减少 `voiceassistant.cpp` 的页面壳层代码。
- 12：测试工具页开始独立化，当前先完成页面壳层，业务动作仍待拆分。
- 17：搜索 UI 与页面容器从主窗口分离，`HubWindow` 的 UI 职责进一步降低。
- 20：测试工具页与 FAQ 页的联动已经通过 `DiagnosticsPanel` 回调和 `FaqPanel` 公开方法完成。

## 验证

- `qmake` + `mingw32-make -j2` 构建通过。
- release 下 20 个现有测试全部通过。

## 下一步建议

1. 继续把接口自检和网络诊断动作从 `HubWindow` 移到独立诊断任务模块。
2. 再拆麦克风测试、选中文字测试、写入测试和浮动条测试。
3. 测试工具页业务动作拆完后，再处理图片识别页、词库页和历史页。
