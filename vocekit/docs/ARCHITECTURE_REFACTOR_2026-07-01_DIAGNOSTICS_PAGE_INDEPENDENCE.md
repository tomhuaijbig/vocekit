# 架构拆分记录：测试工具页继续独立化
更新时间：2026-07-01

## 本轮完成

- 新增 `src/ui/write_input_test_card.h` 和 `src/ui/write_input_test_card.cpp`。
- 将“写入测试”的卡片 UI、内置文本框、写入光标、替换选中和恢复内容逻辑从 `HubWindow` 移到独立 QWidget。
- 新增 `src/ui/selection_input_test_card.h` 和 `src/ui/selection_input_test_card.cpp`。
- 将“选中文字测试”的普通读取、强力读取、3 秒倒计时、浮动条提示、读取完成后回到主窗口和结果展示逻辑从 `HubWindow` 移到独立 QWidget。
- 新增 `src/ui/microphone_input_test_card.h` 和 `src/ui/microphone_input_test_card.cpp`。
- 将“麦克风测试”的 3 秒录音、可选保存样本、诊断文案和失败提示从 `HubWindow` 移到独立 QWidget。
- 新增 `src/ui/floating_bar_test_card.h` 和 `src/ui/floating_bar_test_card.cpp`。
- 将“浮动条测试”的状态选择、波形预览、识别/模型/写入计时预览和结果提示从 `HubWindow` 移到独立 QWidget。
- 新增 `src/ui/interface_self_check_card.h` 和 `src/ui/interface_self_check_card.cpp`。
- 将“接口自检”的目标接口下拉框、后台自检调用和结果展示从 `HubWindow` 移到独立 QWidget。
- 新增 `src/ui/result_popup_test_card.h` 和 `src/ui/result_popup_test_card.cpp`。
- 将“结果小框测试”的预览弹窗入口从 `HubWindow` 移到独立 QWidget。
- 删除旧的 `src/pages/hub_input_tests_methods.h`，并移除 `HubWindow` 中对应的测试成员变量和方法。

## 对 11-20 架构任务的影响

- 11：继续缩小 `voiceassistant.cpp`，测试工具页里 6 个测试入口已从主文件迁出。
- 12：测试工具页内的写入、选中、麦克风、浮动条、接口自检和结果小框测试已经变成真实 QWidget 类。
- 17：测试页 UI 细节不再全部集中在主窗口，主窗口只负责组装测试页和保留仍未迁出的少数测试入口。

## 验证

- `qmake` + `mingw32-make -j2` 构建通过。
- release 测试集通过。

## 剩余工作

1. 将“词库测试”卡片从 `HubWindow` 迁移为独立 QWidget。
2. 最后让 `HubWindow::diagnosticsPage()` 只负责创建 `DiagnosticsPanel`，由独立卡片类承接具体测试逻辑。
