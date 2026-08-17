# vocekit OCR 工作流设计

## 目标

为 vocekit 增加单张普通截图 OCR。识别出的文字可以直接复制，也可以继续交给翻译、润色、总结、问答和自定义功能处理。

第一版不处理批量图片、实时视频、复杂 PDF、表格结构恢复和版面重建。

## 隐私原则

- RapidOCR 和 Windows OCR 全程在本机运行，不上传图片。
- 自定义云端 OCR 和 AI 图片识别默认关闭。
- 用户主动选择云端 OCR 或 AI 图片识别时，界面必须明确显示“图片会发送到所配置的服务”。
- 日志只保存引擎、图片尺寸、耗时、结果字数和错误摘要，不保存图片内容、Base64、密钥或完整识别文字。
- OCR 临时图片在任务结束后删除；用户选择的原始图片不修改。

## 页面和交互

左侧导航在“词库”下方增加“OCR”。

OCR 页面包含：

1. 选择图片。
2. 图片预览。
3. 当前识别引擎和语言。
4. “开始识别”按钮。
5. 状态：等待识别、正在识别、识别完成、识别失败、已切换备用引擎。
6. 可编辑的识别结果。
7. 结果操作：复制、智能整理、翻译、润色、总结、问答。
8. “发送到功能”菜单，列出所有自定义功能。

同一时间只运行一个 OCR 任务。识别中再次点击时显示“已有图片正在识别”，不创建第二个任务。

OCR 完成后允许用户修改文字，再将修改后的内容交给 AI。

## 引擎顺序

设置中的 OCR 模式：

- 自动：RapidOCR 失败后切换 Windows OCR。
- RapidOCR：只用 RapidOCR，失败后给出原因。
- Windows OCR：只用 Windows OCR。
- 自定义云端 OCR：上传到用户配置的接口。
- AI 图片识别：把图片发送给用户选择的视觉模型。

默认模式为“自动”。

自动模式中的失败顺序：

1. 检查 RapidOCR 辅助程序和模型文件。
2. 首次使用时启动 RapidOCR 辅助程序并加载模型。
3. RapidOCR 模型缺失、进程启动失败、超时、返回格式错误或识别异常时，记录原因并尝试 Windows OCR。
4. Windows OCR 不可用、语言包缺失或识别失败时，显示两个引擎各自的失败原因。
5. 任一辅助进程退出或返回异常都不能终止 vocekit 主进程。

## RapidOCR 架构

RapidOCR 运行在独立的 `vocekit-rapidocr.exe` 辅助进程中，不在 Qt 主进程加载 ONNX Runtime。

目录结构：

```text
ocr/
  rapidocr/
    vocekit-rapidocr.exe
    onnxruntime.dll
    models/
      det.onnx
      cls.onnx
      rec.onnx
      keys.txt
```

主程序和辅助程序通过标准输入输出传递一行 JSON：

请求：

```json
{
  "requestId": "ocr-时间戳",
  "action": "recognize",
  "imagePath": "临时图片绝对路径",
  "languages": ["zh-Hans", "en"]
}
```

成功响应：

```json
{
  "requestId": "ocr-时间戳",
  "ok": true,
  "text": "识别结果",
  "lines": [
    {
      "text": "一行文字",
      "confidence": 0.98
    }
  ],
  "elapsedMs": 320
}
```

失败响应：

```json
{
  "requestId": "ocr-时间戳",
  "ok": false,
  "errorCode": "MODEL_MISSING",
  "message": "缺少识别模型"
}
```

辅助进程在首次识别时启动。任务结束后保留 5 分钟，期间复用已加载模型；连续 5 分钟无任务后正常退出并释放内存。主程序退出时也要求辅助进程退出。

单次识别默认超时 45 秒。超时后终止本次辅助进程，自动模式继续尝试 Windows OCR。

## Windows OCR 架构

Windows OCR 使用独立的 `vocekit-windows-ocr.exe` C++/WinRT 辅助程序，不执行 PowerShell。

协议和 RapidOCR 相同，便于主程序统一处理。辅助程序使用 Windows 已安装语言包，第一版优先：

- 简体中文 `zh-Hans`。
- 英文 `en`。
- 中英混合时优先使用简体中文识别器。

系统缺少对应 OCR 语言包时返回 `LANGUAGE_NOT_INSTALLED`，并在常见问题中说明如何通过 Windows 语言设置安装，不由软件自动修改系统设置。

## 自定义云端 OCR

设置的“接口”页增加“OCR 接口”分区，包含：

- OCR 模式。
- 自定义 OCR 地址。
- API Key。
- 模型名称，可空。
- 请求超时。
- “测试当前接口”。

请求使用 HTTPS POST JSON：

```json
{
  "image": "图片 Base64",
  "mimeType": "image/png",
  "languages": ["zh-Hans", "en"],
  "model": "用户填写的模型名"
}
```

填写 API Key 时发送：

```http
Authorization: Bearer 用户密钥
Content-Type: application/json
```

响应按以下顺序读取：

- `text`
- `result`
- `content`
- `data.text`
- `data.result`

接口测试使用程序生成的小型本地测试图片，测试前明确提示该图片会上传。测试不查询余额。

## AI 图片识别

AI 图片识别不是 OCR 的自动降级路径，只有用户主动选择时使用。

功能自定义中的模型下拉仅显示标记为“支持图片”的模型。内置和自定义模型配置增加“支持图片输入”开关。

发送内容包括：

- 图片。
- 固定任务说明：“提取图片中的全部可见文字，保持阅读顺序，只输出文字”。
- 用户选择的 OCR 语言。

模型返回文字后进入和本地 OCR 相同的结果页。视觉接口不支持图片、返回空结果或认证失败时，显示对应常见问题编号。

## AI 后续处理

OCR 结果不直接复制到剪贴板。用户点击后续操作时：

- 智能整理：使用听写整理提示词的独立 OCR 版本。
- 翻译：使用翻译功能当前模型和提示词。
- 润色、总结：使用内置 OCR 操作提示词。
- 问答：OCR 文字作为上下文，弹出问题输入框。
- 自定义功能：把 OCR 文字作为“选中文字”输入，不触发录音，除非该自定义功能明确要求追加语音。

所有 AI 结果继续使用现有结果小框和历史记录机制。

## 历史记录

历史记录新增 `ocr` 类型，保存：

- 图片文件名，不保存图片副本。
- OCR 引擎。
- OCR 语言。
- OCR 文字。
- OCR 耗时。
- 是否发生自动降级。
- 后续处理功能和模型。
- 错误码和错误摘要。

如果用户只完成 OCR、没有调用 AI，也保存一条 OCR 历史记录。

## 模块边界

新增文件：

- `src/modules/ocr_types.h`：OCR 请求、结果、错误码和配置类型。
- `src/modules/ocr_manager.h/.cpp`：单任务调度、懒加载、超时和自动降级。
- `src/modules/ocr_helper_process.h/.cpp`：辅助进程 JSON 协议。
- `src/modules/ocr_cloud_client.h/.cpp`：自定义云端 OCR。
- `src/modules/hub_ocr_page.inc`：OCR 页面。

现有文件调整：

- `src/voiceassistant.cpp`：导航、历史类型和把 OCR 文字交给现有功能。
- `src/modules/settings_panel.inc`：OCR 设置和自定义接口。
- `src/modules/api_client.inc`：视觉模型请求入口。
- `src/modules/hub_history_page.inc`：OCR 历史筛选和详情。
- `vocekit.pro`：新源文件和 WinRT 辅助程序打包说明。

OCR 引擎实现不能继续堆进 `voiceassistant.cpp`。

## 错误和常见问题

至少新增：

1. RapidOCR 辅助程序缺失。
2. RapidOCR 模型缺失。
3. RapidOCR 加载或识别失败。
4. Windows OCR 不可用。
5. Windows OCR 语言包缺失。
6. 两个本地 OCR 引擎均失败。
7. 图片格式不支持或图片过大。
8. 自定义 OCR 地址或返回格式错误。
9. 自定义 OCR 超时或认证失败。
10. 视觉模型不支持图片输入。
11. OCR 正在运行，不能重复启动。

每个错误弹窗使用数字编号，并提供“查看解决办法”。

## 限制

- 支持 PNG、JPG、JPEG、BMP、WebP。
- 输入图片最大 25 MB。
- 解码后最长边超过 8000 像素时等比例缩小后识别。
- 第一版只识别平面截图，不纠正复杂透视、弯曲文档和密集表格。
- 第一版不提供区域框选截图，只选择已有截图文件。

## 验收

- RapidOCR 正常时可以识别中文、英文和中英混合截图。
- 删除 RapidOCR 模型后，自动模式能切到 Windows OCR。
- 两个本地引擎都不可用时，主程序保持运行并显示两个失败原因。
- OCR 期间主界面可以切换页面和移动窗口。
- 同时触发两次 OCR 时只执行一个任务。
- 本地模式抓包不产生图片网络上传。
- 云端模式和 AI 模式在发送前有明确上传提示。
- OCR 结果可以编辑、复制和发送到所有后续 AI 功能。
