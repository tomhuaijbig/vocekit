# 架构拆分记录：测试工具页独立化收尾

更新时间：2026-07-03

## 本轮完成

- 新增 `src/tasks/vocabulary_diagnostic_task.h/.cpp`。
- 新增 `src/ui/vocabulary_test_card.h/.cpp`，把“词库测试”的设置读取、诊断调用和结果展示从 `HubWindow` 迁出。
- 新增 `tests/tasks/vocabulary_diagnostic_task_tests.cpp` 和对应 `.pro`，覆盖词库文件缺失、示例替换和大模型注入数量说明。
- 新增 `src/ui/network_diagnostics_card.h/.cpp`，把“网络诊断”的按钮、后台执行和结果展示从 `HubWindow` 迁出。
- `HubWindow::diagnosticsPage()` 现在只负责把测试卡片组装进 `DiagnosticsPanel`，不再直接保存网络诊断和词库测试的按钮状态。

## 对 11-20 架构任务的影响

- 11：`voiceassistant.cpp` 继续缩小，本轮后约 2860 行。
- 12：测试工具页中的接口自检、网络诊断、麦克风测试、选中文字测试、写入测试、词库测试、浮动条测试和结果小框测试都已经是独立 QWidget 卡片。
- 17：测试工具页 UI 与诊断逻辑进一步解耦，主窗口不再直接执行测试工具页的具体诊断任务。
- 20：测试工具页仍通过页面组装刷新，后续可以继续接入统一事件刷新，但这轮不改变事件机制。

## 剩余工作

1. 继续把历史页、词库页、图片识别页从 `HubWindow` 的方法集合迁成真正的页面类。
2. 继续拆 `VoiceController` 的执行流程，把输入收集、识别、模型处理和结果输出拆成独立管线。
3. 逐步把旧 `ApiClient` 的具体请求迁移到真正的 provider 实现。
