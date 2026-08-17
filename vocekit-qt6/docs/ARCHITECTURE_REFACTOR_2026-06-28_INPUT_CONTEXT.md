# 2026-06-28 输入上下文生成统一

## 本次调整

- `processRecognizedSpeech()` 不再手写 `VoiceRunContext` 字段。
- `processTextOnly()` 不再手写 `VoiceRunContext` 字段。
- 两条路径都改用 `VoiceInputCollector`：
  - `makeVoiceContext()`：语音输入路径。
  - `makeTextOnlyContext()`：纯文本输入路径。
- 语音输入成功和失败时的历史输入文本，都改为通过 `VoiceRunFormatter::historyInput()` 生成。

## 为什么这样拆

- 后续“输入收集 -> 语音识别 -> 模型处理 -> 结果输出”的管线需要一个统一上下文。
- 以前语音路径和文本路径各自拼字段，容易漏掉选中文字、截图 OCR 或独立网络策略。
- 现在上下文生成集中在输入模块，历史输入格式集中在领域格式化模块。

## 行为说明

- 用户可见功能不变。
- 有选中文字 + 语音输入时，历史记录仍显示选中文字和语音输入。
- 纯文本路径仍只保存处理文本。
- 如果大模型失败，历史记录也会保留完整输入上下文，便于排查问题。

## 继续调整

- `processRecognizedSpeech()` 和 `processTextOnly()` 都改为调用同一个 `runContext()`。
- 非流式、小结果框、截图结果窗口三条路径都先拿到模型原始输出，再通过 `finalOutputForContext()` 做最终词库修正。
- `runContext()` 只负责根据 `VoiceRunContext` 调用听写、翻译、问答或自定义功能，不再负责最终输出修正。
- 这样后续拆独立执行管线时，可以把“模型运行”和“结果落地”拆成两个更清晰的模块。
