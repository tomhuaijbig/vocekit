# vocekit 界面设计语言探索（三）

本轮不是直接照搬品牌界面，而是提取成熟生产力工具的设计原则，再适配到 Windows 语音与文本助手。三个方案都遵循以下共同约束：

- 左上角不显示软件名、标志或品牌字样。
- 只使用黑、白、冷灰和克制的蓝色。
- 以中文文字和实际任务为核心，不做营销页或装饰性插画。
- 不使用大量悬浮卡片，不使用渐变、光斑、圆球和高饱和装饰。
- 中文采用清晰的 Windows 无衬线字体，字距为 0，所有文字完整显示。
- 展示的是可实际开发的 Qt 桌面应用，不是网页套壳或概念海报。

## 方案一：精密任务工作台

**参考原则：** Linear 式高密度任务界面。提取严格网格、细分隔线、紧凑层级、克制蓝色焦点和快捷键优先，不复制其品牌或资产。

### 生成提示词

```text
Use case: ui-mockup.
Create a high-fidelity 16:10 Windows desktop productivity application UI concept for a Chinese voice and text assistant. Apply the principles of a precision task workbench: strict grid, compact typography, cool gray surfaces, hairline 1px dividers, restrained cobalt-blue selection, almost no shadow, keyboard-first controls. Do not copy any logo, brand asset, or proprietary interface.

The top-left title-bar area is intentionally blank: no app name, no logo, no wordmark. Use only black, white, cool gray and cobalt blue. Use crisp Microsoft YaHei UI-style Chinese text with zero letter spacing. Every label must fit completely.

Structure: no conventional dark sidebar. Use a thin full-width command bar at the top with four text modes: 听写, 翻译, 问答, 自定义 1. The active mode is 听写 and shows shortcut Alt + X beside it. The right side of the same bar contains compact text commands: 历史, 词库, 提示词, 测试, 日志, 设置.

Below the command bar, create one continuous work surface, not a dashboard. Add a slim task-state strip with four sequential stages: 输入, 识别, AI 处理, 交付. The stage 输入 is active in blue; each stage shows a small elapsed time.

Main workspace: a two-column editor with a 42/58 split. Left side is an editable live transcript titled 原始内容, showing a few lines of Chinese speech text and small inline indicators for 麦克风, 讯飞语音听写, 词库命中 6. Right side is an editable polished result titled 最终结果, with clear text hierarchy and version indicator 版本 2. A narrow inline action row under the result contains 写入, 复制, 替换选中, 重新生成, but only 写入 is blue.

At the bottom, use three compact expandable rows instead of cards: 输入来源, 模型与提示词, 输出方式. Each row has a chevron, current value and keyboard-accessible text control. Keep the overall experience dense, calm and professional. Use square or 4px-radius controls, 1px separators, no floating cards, no illustrations, no gradients, no oversized empty areas. Render as a single real application window centered on a neutral desktop background, not a marketing page and not a website inside a browser.
```
## 方案二：命令入口工作区

**参考原则：** Raycast 式命令入口。提取统一搜索入口、键盘操作、即时结果、暗色工具外壳和上下文操作，不复制其图标、标志或品牌资源。

### 生成提示词

```text
Use case: ui-mockup.
Create a high-fidelity 16:10 Windows desktop application UI concept for a Chinese voice and text assistant. The design language is a keyboard-first command surface: a near-black graphite shell, a single powerful command/search entry, precise 1px borders, compact 6px-radius controls, bright white text and one restrained blue accent. Do not copy any existing product logo or proprietary assets.

No app name, no logo and no wordmark anywhere. The upper-left title-bar area must be empty. Use only black, white, cool gray and blue. Chinese typography must be crisp, fully visible and set with zero letter spacing.

Structure: a very narrow left mode rail with text only, no brand area. It contains 听写, 翻译, 问答, 自定义 1 and a small 新增功能 action at the bottom. The active 听写 item is indicated by a 2px blue line and a darker surface, not by a large card.

Across the top of the content area place one prominent command field reading 搜索功能、执行命令或打开设置, with shortcut Ctrl + K. To its right show only three quiet text actions: 历史, 设置, 帮助. Under it, show the current task header 听写 with Alt + X and a compact status label 等待输入.

The center is a command-driven session surface. The upper half contains a large editable transcript area with subtle line numbers and a live waveform reduced to a thin blue line. The lower half contains generated result text. Between them place a horizontal command palette row with suggestions: 开始录音, 读取选中文字, 截图识别, 打开最近结果. The selected suggestion has a blue outline.

When the user selects a command, show a contextual detail pane sliding from the right edge only 320px wide, displaying relevant settings such as 麦克风, 语音服务, 提示词 and 输出方式. It is not permanently visible. At the bottom of the main surface show a compact keyboard hint strip: Enter 执行, Tab 切换, Esc 关闭.

The interface should feel fast and focused, with very little decorative chrome. Avoid dashboards, decorative cards, gradients, glowing effects, huge headings, marketing copy and excessive rounded corners. Render one realistic Windows application window on a neutral background, not a browser page.
```

## 方案三：文档式处理流

**参考原则：** Notion 式内容优先工作流。提取连续文档、块级编辑、渐进式披露和少量工具栏，不使用其品牌插画或彩色属性体系。

### 生成提示词

```text
Use case: ui-mockup.
Create a high-fidelity 16:10 Windows desktop productivity UI for a Chinese voice and text assistant. Use a document-first design language: continuous white editing canvas, minimal chrome, block-based content, progressive disclosure, quiet cool-gray background and restrained blue focus. Do not copy logos, illustrations or proprietary assets from any existing product.

No product name, no logo and no wordmark. Leave the upper-left title-bar area blank. Use only black, white, cool gray and blue. Use clear Microsoft YaHei UI-style Chinese text, zero letter spacing and generous but efficient line height. All text must remain fully visible.

Structure: no sidebar. Use a slim top breadcrumb row: 今天 / 听写记录, followed by text-only navigation on the right: 历史, 词库, 提示词, 测试, 日志, 设置. Below it is a document title 听写 and a small shortcut label Alt + X.

The document begins with a compact property table, not cards: 输入方式 = 语音, 语音服务 = 百度语音识别, 模型 = deepseek-v4-flash, 输出 = 结果小框. Each value is clickable text with a subtle blue underline. Under the properties, place a single inline blue action 开始听写 and quiet secondary text actions 读取选中 and 截图识别.

Create three vertically stacked document blocks separated only by whitespace and thin rules. Block 1 is 原始输入 with editable Chinese transcript and a small timestamp in the margin. Block 2 is AI 处理 with collapsible rows for 提示词, 词库命中 and 模型参数. Block 3 is 输出结果 with an editable polished paragraph, a blue insertion cursor and a small state label 已修改 · 自动保存草稿.

At the bottom of the result block, place a restrained text toolbar: 写入, 复制, 替换选中, 继续追问, 版本记录. The active action uses a solid blue rectangle with 4px radius; all others are text buttons. Show a very narrow revision rail on the far right with three timestamp markers, not a settings panel.

The result should feel like writing in a calm professional document rather than operating a control panel. Avoid dashboards, repeated boxed cards, permanent inspectors, gradients, shadows, illustrations and large empty decorative areas. Render one realistic Windows desktop application window, not a web browser or marketing presentation.
```
