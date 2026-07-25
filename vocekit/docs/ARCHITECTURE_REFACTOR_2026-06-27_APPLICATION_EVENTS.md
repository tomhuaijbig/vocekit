# 2026-06-27 ApplicationEvents 接入

## 本次目标

把已经存在但未使用的 `ApplicationEvents` 接入主窗口，开始减少页面之间和入口逻辑之间的直接刷新调用。

## 改动内容

- 在程序入口创建 `ApplicationEvents` 实例。
- `HubWindow` 增加 `setApplicationEvents()`，订阅设置、历史和词库变化事件。
- 设置变化后通过 `events.publishSettingsChanged()` 通知主窗口刷新，而不是入口处直接调用 `hub.refreshShortcuts()`。
- 历史记录保存后通过 `publishHistoryChanged()` 通知历史页失效缓存，并在历史页存在时刷新。
- 词库变化事件预留了主窗口刷新入口，会刷新词库页和功能页。

## 保守处理

- 没有移除旧的直接刷新逻辑；托盘、快捷键、截图启动器和运行状态仍按原路径执行。
- 这一步只建立事件总线的主路径，后续再逐步把页面内部的 `refreshHistoryTabs(true)`、`refreshVocabularyTabs()` 等手动调用迁移到事件发布。

## 验证结果

- `qmake vocekit.pro` 通过。
- `mingw32-make -j2` 通过。

## 后续

- 删除历史、导入历史、重建索引、词库导入导出等动作可以继续改为发布事件。
- 当历史页、词库页、设置页拆成独立 QWidget 后，页面可以直接订阅 `ApplicationEvents`，不再依赖 `HubWindow` 私有方法。
