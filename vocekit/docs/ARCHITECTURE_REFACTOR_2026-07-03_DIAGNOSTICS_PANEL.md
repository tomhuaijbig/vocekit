# 2026-07-03 测试工具页装配收敛

## 本次改动

- `DiagnosticsPanel` 新增 `addDefaultCards()`，由面板内部统一装配接口自检、网络诊断、麦克风测试、选中文字测试、写入测试、词库测试、浮动条测试和结果小框测试。
- `HubWindow::diagnosticsPage()` 不再逐个创建测试卡片，只传入设置、浮动条、程序目录和密钥读取回调。
- 主窗口移除了对测试卡片头文件的直接依赖；进入测试页时只调用 `DiagnosticsPanel::refreshRuntimeTargets()`。

## 为什么拆分

测试工具页已经有独立面板类，但页面内容仍由 `HubWindow` 拼装。这样主窗口需要知道每张测试卡片的构造方式，后续新增测试项会继续污染主窗口。

装配逻辑进入 `DiagnosticsPanel` 后：

- 新增或调整测试项优先修改测试页自身。
- 主窗口只负责页面切换和依赖注入。
- 后续把测试工具页完全迁出 `HubWindow` 会更容易。

## 后续衔接

- 可以继续把历史页或 OCR 页拆成独立页面类。
- 后续如果新增测试项，优先在 `DiagnosticsPanel::addDefaultCards()` 内处理，避免把测试页细节重新散落回主窗口。
