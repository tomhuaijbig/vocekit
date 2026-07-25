# 架构拆分记录：接口自检任务独立化

更新时间：2026-07-01

## 本轮完成

- 新增 `src/tasks/interface_self_check_task.h` 和 `src/tasks/interface_self_check_task.cpp`。
- 新增 `tests/tasks/interface_self_check_task_tests.cpp` 和 `tests/tasks/interface_self_check_task_tests.pro`。
- 将 `HubWindow::runInterfaceSelfCheck()` 中的接口自检业务迁移到 `runInterfaceSelfCheckTask()`。
- 主窗口现在只负责：
  - 禁用/恢复“开始测试”按钮；
  - 收集当前自检目标、OCR 设置、程序路径、代理策略和接口密钥；
  - 启动后台任务；
  - 展示自检结果。
- 独立任务现在负责：
  - 百度、讯飞、自定义语音接口自检；
  - DeepSeek、OpenAI、Claude 和多个自定义大模型自检；
  - 图片识别接口自检；
  - RapidOCR / Windows OCR / 自定义云 OCR 的测试图片识别流程；
  - 自动 OCR 失败回退逻辑。

## 对 11-20 架构任务的影响

- 11：`voiceassistant.cpp` 继续缩小，当前约 141 KB / 3074 行。
- 13、14：接口自检从 UI 类迁移到后台任务模块，后续拆“功能执行管线”时可以复用这种请求/结果结构。
- 15：自检仍使用 `ProviderRegistry`，但调用位置已经从主窗口移到任务模块。
- 17：主窗口不再直接承载 Provider 和 OCR 自检实现细节。

## 验证

- `qmake` + `mingw32-make -j2` 构建通过。
- release 下 21 个测试全部通过。
- 新增测试覆盖 DeepSeek、讯飞和自定义大模型在未填写密钥时的无网络跳过路径。

## 下一步建议

1. 给 `InterfaceSelfCheckTask` 增加独立单元测试，优先覆盖“未填写密钥时跳过”的无网络路径。
2. 将接口自检接入统一取消令牌，避免 OCR helper 或网络自检耗时较长时无法取消。
3. 继续拆麦克风测试、选中文字测试、写入测试和浮动条测试。
