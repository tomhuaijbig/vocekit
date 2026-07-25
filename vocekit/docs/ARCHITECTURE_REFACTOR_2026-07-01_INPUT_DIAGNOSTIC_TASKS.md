# 架构拆分记录：输入诊断任务独立化

更新时间：2026-07-01

## 本轮完成

- 新增 `src/tasks/microphone_diagnostic_task.h` 和 `src/tasks/microphone_diagnostic_task.cpp`。
- 将麦克风测试中“录音结束后的 PCM 分析、峰值/平均音量/削波统计、失败/低音量/通过判断、警告文案生成”从主窗口移到独立任务。
- 主窗口仍负责启动和停止真实麦克风录音，避免 Qt 音频设备跨线程风险。
- 新增 `src/tasks/selected_text_diagnostic_task.h` 和 `src/tasks/selected_text_diagnostic_task.cpp`。
- 将选中文字测试中“普通读取/强力读取、未识别到/通过、字符数和摘要展示”的结果生成从测试页面方法里移到独立任务。
- 新增两个单元测试：
  - `tests/tasks/microphone_diagnostic_task_tests.cpp`
  - `tests/tasks/selected_text_diagnostic_task_tests.cpp`

## 对 11-20 架构任务的影响

- 11：继续缩小 `voiceassistant.cpp` 和页面方法里的业务判断。
- 13、14：输入诊断开始从 UI 类迁移到可测试任务，为后续拆完整输入管线做准备。
- 17：UI 层只保留录音、倒计时、窗口切换和展示，具体诊断判断移入任务模块。

## 验证

- `qmake` + `mingw32-make -j2` 构建通过。
- release 下 23 个测试全部通过。

## 下一步建议

1. 拆写入测试的结果和操作判断。
2. 拆浮动条测试状态构造。
3. 继续把 `HubWindow` 的测试工具逻辑迁移到独立页面类。
