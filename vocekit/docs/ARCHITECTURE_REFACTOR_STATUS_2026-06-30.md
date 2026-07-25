# 架构重构状态快照

更新时间：2026-06-30

## 本轮完成

- `VoiceController` 已经从 `voiceassistant.cpp` 搬到 `src/controllers/voice_controller.h` 和 `src/controllers/voice_controller.cpp`。
- 新增 `src/controllers/voice_controller_host.h`，把语音控制器对主窗口的依赖收窄成接口。
- `HubWindow` 现在只实现 `VoiceControllerHost`，负责提供主界面、词库刷新、设置刷新、词条编辑和历史保存通知。
- `voiceassistant.cpp` 从约 351 KB / 8008 行降到约 259 KB / 5613 行。

## 11-20 当前判断

| 编号 | 目标 | 当前状态 |
| --- | --- | --- |
| 11 | 拆 `voiceassistant.cpp` 为真正 `.h/.cpp` | 继续推进。核心语音控制器已搬出，主文件仍保留 `HubWindow` 和页面组合逻辑。 |
| 12 | 设置、历史、词库页面改独立类 | 未完成。仍有 `src/pages/*_methods.h` 依赖 `HubWindow`。 |
| 13 | 拆出语音任务控制器 | 基本完成外移。控制器已独立成文件，但内部流程还需要继续拆小。 |
| 14 | 拆出功能执行管线 | 部分完成。已有输入、识别、模型、输出相关模块，但还没有完整统一管线类。 |
| 15 | 统一接口提供商抽象 | 部分完成。Provider 抽象存在，旧 `ApiClient` 仍承担实际请求。 |
| 16 | 统一任务取消接口 | 部分完成。取消令牌存在，但还没有覆盖所有长任务。 |
| 17 | UI 与业务逻辑分离 | 继续推进。语音控制器已脱离 `HubWindow` 类型，但页面 UI 仍集中在主窗口。 |
| 18 | 配置项使用明确数据结构 | 部分完成。新结构存在，旧 `AppSettings` 仍是主入口。 |
| 19 | 历史存储独立为服务 | 基本完成。历史保存已主要走 `HistoryRecordService` / `HistoryStore`。 |
| 20 | 事件总线统一页面刷新 | 部分完成。`ApplicationEvents` 已有，但还没有替代所有直接刷新调用。 |

## 下一步优先级

1. 继续拆 `VoiceController::Impl`，优先拆截图、录音、识别、模型处理和结果输出。
2. 把历史页、词库页、图片识别页、测试页从 `HubWindow` 中拆成独立 `QWidget`。
3. 清理 `voiceassistant.cpp` 中不再需要的 include。
4. 逐步把旧 `ApiClient` 的真实请求迁移到 `src/providers/`。
