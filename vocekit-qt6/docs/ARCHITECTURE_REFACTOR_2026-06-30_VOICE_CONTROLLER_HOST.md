# VoiceController 主窗口依赖拆分

本次调整的目标是把 `VoiceController` 从具体的 `HubWindow` 中解耦出来，并把语音控制器从 `voiceassistant.cpp` 搬到独立 `src/controllers/voice_controller.h/.cpp`。

## 已完成

- 新增 `src/controllers/voice_controller_host.h`。
- 新增 `src/controllers/voice_controller.h` 和 `src/controllers/voice_controller.cpp`。
- `VoiceController` 不再保存 `HubWindow *m_hub`，改为保存 `VoiceControllerHost *m_host`。
- 主程序仍然使用 `VoiceController` 这个稳定外壳，实际大实现先放在 `VoiceController::Impl` 中，降低一次性重构风险。
- `VoiceController` 需要主窗口协助的动作都通过接口完成：
  - 打开主界面。
  - 获取弹窗父窗口。
  - 刷新词库页面。
  - 应用设置变更。
  - 打开词条编辑窗口。
  - 通知历史记录已保存。
- 弹窗父窗口统一通过 `hostWidget()` 获取，避免控制器直接知道主窗口类型。
- `HubWindow` 实现 `VoiceControllerHost`，继续承接现有 UI 行为。
- `voiceassistant.cpp` 从约 351 KB / 8008 行降到约 259 KB / 5613 行。

## 为什么这样做

之前 `VoiceController` 直接调用 `HubWindow` 的方法，导致语音流程、词库、历史、设置刷新和主界面强绑定。只要要移动 `VoiceController`，就会牵动整个主窗口类。

现在先建立一个小接口，把控制器真正需要的 UI 能力列出来，再用 `Impl` 方式搬走控制器主体。这样主程序和页面代码不需要跟着大改，后续可以继续逐步拆小语音流程。

## 下一步

1. 把 `VoiceController::Impl` 内部的截图、录音、识别、模型处理再逐步拆成更小任务。
2. 继续减少 `HubWindow` 私有页面方法，让历史、词库、图片识别和测试页变成独立 `QWidget`。
3. 清理 `voiceassistant.cpp` 中已经不再需要的 include，让主文件继续变薄。

## 验证

- `qmake` 通过。
- `mingw32-make -j2` 通过。
- 已跑过核心测试：
  - `model_request_task_tests`
  - `vocabulary_store_tests`
  - `screenshot_core_tests`
  - `app_settings_json_tests`
  - `ocr_core_tests`
  - `recording_core_tests`
