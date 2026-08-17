# 2026-06-29 架构拆分记录：UI 辅助类和托盘控制器

本轮目标是继续减少 `src/voiceassistant.cpp` 的职责，把已经相对独立的代码先迁出，避免直接大规模移动 `HubWindow` 或 `VoiceController` 造成回归。

## 已拆出的模块

1. `src/ui/app_dialogs.h/.cpp`
   - 承载 `AppDialog` 和 `HelpDialog`。
   - 作用是统一应用弹窗标题栏行为，并处理帮助弹窗说明。

2. `src/ui/mode_card_frame.h/.cpp`
   - 承载主页功能卡片的拖动排序和双击编辑交互。
   - 让主窗口不再直接包含鼠标拖拽和拖放细节。

3. `src/ui/chinese_text_context_menu.h/.cpp`
   - 承载文本框右键菜单汉化逻辑。
   - 让程序入口只负责安装事件过滤器。

4. `src/tasks/processing_guard.h/.cpp`
   - 承载处理状态作用域守卫。
   - 用于防止录音、识别和模型处理阶段重复进入。

5. `src/controllers/tray_controller.h/.cpp`
   - 承载托盘常驻菜单、语音服务切换、网络代理切换、浮动条开关和测试浮动条入口。
   - 新实现只依赖 `QWidget*` 和回调，不再直接包含旧 `AppSettings` 或 `FloatingBar` 头文件。

6. `src/ui/floating_bar.h`
   - 仍是头文件实现，但已改为使用 `FloatingBarPositionCallbacks` 读写位置。
   - 去掉对旧 `AppSettings` 指针的直接依赖，后续更容易搬成真正的 `.cpp` 模块。

## 暴露出的架构问题

拆 `TrayController` 时发现旧 `AppSettings` 和 `FloatingBar` 头文件还不是完全独立模块：

- `legacy_app_settings.h` 依赖 `voiceassistant.cpp` 中的若干旧全局函数。
- `floating_bar.h` 原本依赖 `tr8`、`appFont`、`clampedTopLeftToScreen` 等旧全局函数；本轮已去掉 `tr8` 和旧坐标函数依赖，保留 `appFont` 由 `ui_style.h` 提供。

因此托盘控制器没有继续直接包含这些旧头文件，而是改用回调作为接口。这说明后续应该优先把这些旧全局函数继续迁出到稳定模块中。

## 编译验证

使用 Qt 5.9 MinGW 重新运行：

```powershell
qmake.exe vocekit.pro
mingw32-make.exe -j2
```

结果：编译和链接通过。

## 下一步建议

1. 将 `FloatingBar` 从头文件实现继续迁移到 `floating_bar.cpp`。
2. 继续清理 `legacy_app_settings.h` 对旧全局函数的依赖，为后续替换成 `AppSettingsStore` 做准备。
3. 再拆 `VoiceController`，优先从输入收集、截图输入和结果输出这些已经有辅助模块的区域开始。
4. 暂时不要直接整体搬 `HubWindow`，页面方法仍然依赖大量私有成员，直接搬风险较高。
