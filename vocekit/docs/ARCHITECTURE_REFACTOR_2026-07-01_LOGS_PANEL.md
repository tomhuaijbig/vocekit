# 架构拆分记录：日志页独立化

更新时间：2026-07-01

## 本轮完成

- 新增 `src/ui/logs_panel.h` 和 `src/ui/logs_panel.cpp`。
- 将日志页 UI、日志文件读取、搜索筛选、滚动加载和日志详情弹窗从 `HubWindow` 移到 `LogsPanel`。
- `HubWindow` 现在只保留 `LogsPanel *m_logsPanel`，并在进入“日志”页面时调用 `LogsPanel::reload(true)`。
- 补齐 `src/ui/history_row_frame.h` 自身依赖的 Qt 头文件，避免公共 UI 小组件继续依赖外部文件先包含 `QtWidgets`。
- 清理 `voiceassistant.cpp` 中不再需要的 `QDesktopServices` include。

## 对 11-20 架构任务的影响

- 11：继续减少 `voiceassistant.cpp` 体积，本轮从约 244 KB / 4842 行降到约 229 KB / 4532 行。
- 12：日志页已经变成独立 QWidget 类；历史页、词库页、测试页和 FAQ 页仍需继续拆分。
- 17：日志页的 UI 状态和刷新逻辑从主窗口分离，降低 `HubWindow` 的页面职责。
- 20：日志页仍由主窗口导航触发刷新，后续可以进一步接入事件总线做统一刷新。

## 验证

- `qmake` + `mingw32-make -j2` 构建通过。
- release 下 20 个现有测试全部通过。

## 下一步建议

1. 把 FAQ 页从 `HubWindow` 拆出，但要先整理它和测试工具页、错误编号跳转之间的关系。
2. 继续把历史页和词库页从 `src/pages/*_methods.h` 迁移成真正独立的 `QWidget` 类。
3. 后续拆页面时，公共可点击卡片继续使用 `HistoryRowFrame`，不要再依赖主文件包含顺序。
