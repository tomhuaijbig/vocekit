# RapidOCR 本地识别说明

## 架构

vocekit 主程序是 Qt 5.9、32 位 MinGW 程序。RapidOCR 使用 64 位
MSVC 辅助进程运行，主程序不会直接加载 OpenCV 或 ONNX Runtime。

通信协议：

- 主程序向辅助进程标准输入写入一行 JSON。
- 辅助进程向标准输出返回一行 JSON。
- 第三方库的诊断文字只能写入标准错误，不能污染 JSON。
- 辅助进程崩溃、超时或模型损坏时，主程序仍保持运行。

## 本地依赖

下载 RapidOcrOnnx 1.2.2 完整工程：

https://github.com/RapidAI/RapidOcrOnnx/releases/tag/1.2.2

文件名：

`Project_RapidOcrOnnx-1.2.2.7z`

解压到：

`vocekit/Project_RapidOcrOnnx-1.2.2`

该目录约 810 MB，已被 Git 忽略，不进入源码仓库和测试包。

## 构建

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-ocr-helpers.ps1
```

生成内容：

```text
helpers/bin/vocekit-rapidocr.exe
helpers/bin/vocekit-windows-ocr.exe
helpers/bin/models/
helpers/bin/LICENSE-RapidOcrOnnx.txt
```

脚本会校验四个 RapidOCR 模型的 SHA-256，防止模型版本或文件内容错误。

## 部署结构

```text
ocr/
  rapidocr/
    vocekit-rapidocr.exe
    LICENSE-RapidOcrOnnx.txt
    models/
      ch_PP-OCRv3_det_infer.onnx
      ch_PP-OCRv3_rec_infer.onnx
      ch_ppocr_mobile_v2.0_cls_infer.onnx
      ppocr_keys_v1.txt
  windows/
    vocekit-windows-ocr.exe
```

## 错误约定

- `MODEL_MISSING`：模型文件不完整。
- `MODEL_LOAD_FAILED`：模型存在，但 ONNX Runtime 无法加载。
- `IMAGE_DECODE_FAILED`：图片无法读取、无法解码或超过 25 MB。
- `RECOGNITION_FAILED`：识别过程中发生异常。
- `EMPTY_TEXT`：图片可读取，但没有识别到文字。

自动模式下，RapidOCR 的可恢复错误会切换到 Windows OCR。

## 验证

```powershell
.\build-tests\ocr-red\debug\ocr_core_tests.exe -txt
```

当前测试覆盖：

- RapidOCR 真实中英文图片识别。
- 模型缺失时返回错误码。
- Windows OCR 备用识别。
- 自动降级。
- 超时和取消。
- 非法图片、超大图片和无效响应。
