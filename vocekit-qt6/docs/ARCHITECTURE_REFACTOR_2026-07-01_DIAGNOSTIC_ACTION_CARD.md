# 架构拆分记录：诊断测试卡片工具独立化

更新时间：2026-07-01

## 本轮完成

- 新增 `src/ui/diagnostic_action_card.h` 和 `src/ui/diagnostic_action_card.cpp`。
- 将测试工具页通用的诊断卡片创建逻辑从 `HubWindow` 移到独立 UI helper。
- 将诊断结果展示函数 `showDiagnosticResult()` 从 `HubWindow` 移到独立 UI helper。
- `HubWindow` 仍然保留具体测试动作，但不再持有通用测试卡片的实现细节。

## 对 11-20 架构任务的影响

- 11：继续缩小 `voiceassistant.cpp`。
- 12：测试工具页的 UI 组件进一步模块化。
- 17：通用 UI 细节从主窗口分离，减少后续拆页面时的耦合。

## 验证

- `qmake` + `mingw32-make -j2` 构建通过。
- release 下 20 个现有测试全部通过。

## 下一步建议

1. 继续拆 `InterfaceSelfCheckTask`。
2. 将麦克风测试、选中文字测试、写入测试和浮动条测试分别拆成任务或小组件。
3. 拆完测试工具页后，继续处理历史页和词库页的独立 QWidget。
