# VoceKit Qt 6

VoceKit 是一个基于 Qt 6 Widgets 的 Windows 桌面语音助手。项目以系统托盘和全局快捷键为核心，支持语音输入、翻译、问答、自定义功能流、选中文字操作、截图 OCR、词库、历史记录和结果小框。

从 2026-08-17 起，本目录是后续开发使用的 Qt 6 主项目。旧目录 `C:\Users\13736\Desktop\tts\vocekit` 仅作为 Qt 5 历史版本保留，不再作为新功能开发基线。

## 开发基线

- Windows 10/11 64 位
- Qt 6.11.1 MinGW 64-bit
- MinGW 13.1 64-bit
- C++17
- qmake + mingw32-make
- Qt 模块：Core、Gui、Widgets、Network、WebSockets、Multimedia、Concurrent、TestLib

当前机器已经验证的工具路径：

```text
D:\QT66666\6.11.1\mingw_64\bin
D:\QT66666\Tools\mingw1310_64\bin
```

Qt 安装必须包含 Qt WebSockets 组件。

## 目录结构

- `vocekit.pro`：Qt 6 主工程。
- `src/`：界面、控制器、领域模型、录音、网络、OCR 和持久化代码。
- `tests/`：QtTest 与独立测试工程。
- `helpers/`：Windows OCR、RapidOCR 和 Windows Speech 辅助进程源码或本机构建结果。
- `config/`：示例配置；真实密钥和本机设置不提交。
- `prompts/`：默认提示词。
- `scripts/`：正式构建、测试、部署和打包入口。
- `updater/`：便携版独立更新程序和用户数据保护策略。
- `docs/`：架构、测试和历史开发文档。

## 首次配置

```powershell
Copy-Item config/settings.example.json config/settings.json
Copy-Item config/secrets.example.json config/secrets.json
```

也可以直接启动软件，在设置页填写自己的服务配置。不要提交 `config/secrets.json`、录音、历史记录或日志。

## 构建

Debug：

```powershell
& .\scripts\build.ps1 -Configuration debug
```

Release：

```powershell
& .\scripts\build.ps1 -Configuration release
```

构建结果位于：

```text
.qt6-build/debug/vocekit.exe
.qt6-build/release/vocekit.exe
```

上述文件是供开发和链接检查使用的裸构建产物，不包含 Qt DLL 与插件。不要把其中的
`vocekit.exe` 单独复制或直接当作便携程序分发；普通运行请使用下文生成的
`.qt6-deploy/vocekit.exe`，对外发送请使用 `dist/` 中的完整目录或 ZIP。

脚本已有当前机器的默认路径。换电脑时可以传入自己的 Qt 6 和 MinGW 目录：

```powershell
& .\scripts\build.ps1 -Configuration debug `
  -QtBin "D:\Qt\6.11.1\mingw_64\bin" `
  -MingwBin "D:\Qt\Tools\mingw1310_64\bin"
```

## 测试

运行全部 185 个测试/程序目标：

```powershell
& .\scripts\run-all-tests.ps1
```

最近一次完整 Qt 6 验证结果为 1846 项通过、0 项失败、0 项跳过、0 个基础设施错误。

修改界面时，除测试外还必须检查普通窗口、最大化、Windows 字体缩放和较长中文文本，不能只以编译成功为准。

## 高级模型 API

普通聊天流程保持简洁；需要自定义完整请求或调试服务端响应时，可从“设置 → 接口 → 高级 API 自定义”打开独立控制台。它支持可视化参数、未来未知字段的 Raw JSON 最终覆盖、System Prompt 预设、模型列表、密钥和连接检测、默认仅记录元数据的请求日志、原始响应、Token/停止原因/延迟/费用估算，以及 Markdown、代码、公式、链接和结构化来源显示。只有用户主动开启后，请求日志才保存脱敏后的问题、系统提示词、工具定义和模型回答。

详细的优先级、安全边界和使用方法见 `docs/ADVANCED_MODEL_API.md`。

## 部署

先构建 Release，再生成独立运行目录：

```powershell
& .\scripts\build.ps1 -Configuration release
& .\scripts\deploy.ps1
```

部署结果位于 `.qt6-deploy/`，可双击其中的 `vocekit.exe`。脚本会调用
`windeployqt`，并加入：

- Qt 6 与 MinGW 运行库及插件。
- Qt 中文翻译。
- Windows OCR 与 RapidOCR 助手和模型。
- Windows Speech 助手。
- Qt 6 运行库架构和语音助手协议验证。

## 生成便携测试包

```powershell
& .\scripts\package-test.ps1
```

结果位于：

```text
dist/vocekit-test/
dist/vocekit-test.zip
```

打包脚本只使用空白示例配置，不复制真实密钥、设置、历史、录音或日志。

## 在线更新

正式发布构建的“设置 → 更新”可以检查公开更新源、显示发布说明、下载完整更新包、校验 SHA-256，并在主程序退出后由独立更新程序完成替换和重启。开发和内部测试构建默认关闭联网更新，只有显式注入公开 HTTPS 更新源才会启用。更新会保护本机的配置、API Key、自定义提示词、记录和日志，替换失败时自动回滚运行文件。

公开发布必须通过 `scripts/create-release-package.ps1`：版本/标签、Git 状态、真实应用验收、公开更新源和 Authenticode 签名任一不满足都会拒绝打包。未签名的 `package-test.ps1` 产物带 `UNSIGNED_TEST_BUILD` 标记，只能用于受控测试。

## 崩溃诊断与安全模式

每次启动都会生成会话编号，并将会话编号写入运行日志和 `logs/session-last.json`。Windows 未处理异常会在 `logs/crashes` 生成 `.dmp` 与不含用户正文/API Key 的 JSON 元数据；`logs/last_action.txt` 保留该会话最后动作。最近 10 分钟连续出现两次崩溃时，下次启动会自动进入安全模式，临时关闭全局快捷键、选中文字监控和悬浮条；安全模式正常退出后，下次启动自动恢复。也可用 `vocekit.exe --safe-mode` 手动进入。

对外发布时使用固定包名：

```powershell
& .\scripts\package-test.ps1 -PackageName vocekit-qt6-portable
```

脚本会同时生成 ZIP、`.sha256` 和 `update-manifest.json`。正式公开分发前还必须配置受信任的 Windows 代码签名证书；仓库提供签名入口，但不包含任何私钥。完整发布流程、GitHub Release 文件名和升级测试门槛见 `docs/UPDATES.md`。

## 与旧 Qt 5 目录的关系

- 本项目仍然是 C++ 桌面程序，只是开发基线从 Qt 5.9/C++11 升级为 Qt 6.11/C++17。
- 所有受 Git 跟踪的代码、头文件、工程文件、脚本、配置和文档均已迁入本目录。
- RapidOCR 完整 SDK、OCR/Windows Speech 助手源码与本机构建结果均在本目录内；可执行文件已从本目录重新编译。
- `.qt6-build/debug` 和 `.qt6-build/release` 会读取本项目根目录的 `config/`、`records/` 等数据。
- `.qt6-deploy/` 和 `dist/vocekit-test.zip` 带有独立运行所需的 Qt 6、MinGW、OCR 和语音运行组件。
- 自动化审计和外部目录启动均未发现对旧 `vocekit` 目录或 Qt 5 DLL 的依赖。

详细证据见 `QT6_MIGRATION_REPORT.md`。删除旧目录前先归档或提交本目录，避免把尚未纳入版本控制的新迁移成果一起丢失。

## 开发约束

- UI 不直接访问网络、录音、OCR 或存储实现，应通过现有 controller/provider/storage 边界接入。
- 功能画布编辑 draft；校验成功后再发布 published；没有有效发布流程时保留经典流程兜底。
- 新代码以 Qt 6 和 C++17 为基线，不再为了 Qt 5.9 或 C++11 增加兼容分支。
- 新增 Qt 模块时同步更新 `vocekit.pro`、部署验证和便携包白名单。
- 修改配置字段时同步检查默认值、读写、示例配置、UI 和回归测试。

迁移细节和验证证据见 `QT6_MIGRATION_REPORT.md`。
