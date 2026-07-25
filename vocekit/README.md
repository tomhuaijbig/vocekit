# vocekit

vocekit 是一个基于 Qt 5.9 的 Windows 桌面语音助手。它以托盘常驻和全局快捷键为核心，支持听写、翻译、问答、自定义功能、词库、提示词配置、结果小框、录音保存、历史记录和测试工具。

语音识别支持百度语音识别和讯飞语音听写。大模型接口支持 DeepSeek、OpenAI 和 Claude。仓库只包含源码、示例配置和文档，不包含任何真实接口密钥、录音、历史记录或本机配置。

## 目录结构

- `vocekit.pro`：Qt 项目文件。
- `src/`：程序入口、界面、托盘、快捷键、录音、接口调用和历史记录逻辑。
- `config/`：示例配置。复制示例文件后在软件里填写自己的接口密钥。
- `prompts/`：听写、翻译、问答的默认提示词。
- `scripts/`：部署运行库和生成测试包的脚本。
- `docs/`：测试说明、开发日志、迭代复盘和给 AI 看的项目解释文档。

## 给接手开发者或 AI

- `docs/DEVELOPMENT_LOG.md`：按功能和时间记录主要改动。
- `docs/AI_PROJECT_GUIDE.md`：解释项目文件、关键类、核心流程和修改检查清单。
- `docs/ITERATION_REVIEW.md`：记录开发过程中的理解偏差、返工点和经验教训。

## 本地配置

以下文件只在本地使用，不应提交到 GitHub：

- `config/settings.json`
- `config/secrets.json`
- `records/`
- `debug/`
- `release/`
- `dist/`
- `Makefile*`

首次运行前可以复制示例配置：

```powershell
Copy-Item config/settings.example.json config/settings.json
Copy-Item config/secrets.example.json config/secrets.json
```

也可以直接启动软件，在“设置 -> 接口”中填写自己的 DeepSeek、OpenAI、Claude、百度或讯飞接口密钥。

## 构建

先把 Qt 和 MinGW 加入当前终端的 `PATH`。下面是通用写法，请把路径替换为你自己的安装位置：

```powershell
$env:QT_BIN="D:\Qt\5.9\mingw53_32\bin"
$env:MINGW_BIN="D:\Qt\Tools\mingw530_32\bin"
$env:PATH="$env:MINGW_BIN;$env:QT_BIN;$env:PATH"

& "$env:QT_BIN\qmake.exe" vocekit.pro
& "$env:MINGW_BIN\mingw32-make.exe" -j2
```

构建后的程序位于：

```text
debug/vocekit.exe
```

## 部署运行库

构建完成后运行部署脚本。脚本不会写死开发者电脑路径，需要通过参数或环境变量传入 Qt、MinGW 和 OpenSSL 运行库目录：

```powershell
$env:QT_BIN="D:\Qt\5.9\mingw53_32\bin"
$env:MINGW_BIN="D:\Qt\Tools\mingw530_32\bin"
$env:OPENSSL_BIN="D:\Qt\Tools\mingw530_32\opt\bin"

powershell -ExecutionPolicy Bypass -File .\scripts\deploy.ps1 -Configuration release
```

部署脚本最后会自动运行 `scripts/verify-runtime.ps1`，检查 SSL、Qt 网络与录音插件、
MinGW 运行库、中文翻译和 OCR 文件。任何关键文件缺失或 32/64 位架构不一致时，
部署会直接失败，不会生成看似可运行但无法联网的残缺版本。

需要确认 Qt 能实际加载 OpenSSL 时，可以构建并运行
`tests/runtime/ssl_runtime_smoke.pro`。输出中的 `SSL supported` 必须为 `yes`。

也可以直接传参：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\deploy.ps1 `
  -Configuration release `
  -QtBin "D:\Qt\5.9\mingw53_32\bin" `
  -MingwBin "D:\Qt\Tools\mingw530_32\bin" `
  -OpenSslBin "D:\Qt\Tools\mingw530_32\opt\bin"
```

## 生成测试包

先构建并部署发布版，再生成不含本地密钥、录音、历史记录和编译中间文件的便携测试包：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\deploy.ps1 -Configuration release
powershell -ExecutionPolicy Bypass -File .\scripts\package-test.ps1
```

生成结果：

- `dist/vocekit-test/`：可直接运行的测试目录。
- `dist/vocekit-test.zip`：可发送给测试人员的压缩包。

测试人员使用说明位于测试包内的 `TESTING.md`。打包脚本会使用空白接口配置，不会复制本地 `config/secrets.json`、`config/settings.json` 和 `records/`。

## 上传前检查

```powershell
git status --short
git status --ignored --short
rg -n "sk-[A-Za-z0-9_-]{20,}|C:\\Users|config/secrets.json|records/" .
```

确认真实密钥、录音、历史记录、构建产物和本机路径只出现在忽略列表或说明文字中。
