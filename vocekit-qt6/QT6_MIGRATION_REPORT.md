# VoceKit Qt 6 迁移验证报告

验证日期：2026-08-17

## 结论

项目已经迁移到 Qt 6，并在隔离副本中完成了可编译、可测试、可部署、可启动的验证。整体难度属于中等：不需要推倒重写，但 Qt 5.9 到 Qt 6.11 跨度较大，音频、文本编码、屏幕坐标、正则表达式和部分事件 API 必须逐项替换。

原项目 `C:\Users\13736\Desktop\tts\vocekit` 未修改；正式 Qt 6 开发项目位于 `C:\Users\13736\Desktop\tts\vocekit-qt6`。

## 工具链

- Qt：6.11.1 MinGW 64-bit
- qmake：`D:\QT66666\6.11.1\mingw_64\bin\qmake.exe`
- 编译器：MinGW 13.1 64-bit
- make：`D:\QT66666\Tools\mingw1310_64\bin\mingw32-make.exe`
- 新增安装组件：Qt 6.11.1 WebSockets
- C++ 标准：所有 183 个 qmake 工程由 C++11 提升到 C++17

## 三遍代码遍历

每一遍都覆盖 958 个受 Git 跟踪的代码、工程和脚本文件，约 5.16 MB、160,640 行。

### 第一遍：结构和依赖

- 468 个 C++ 源文件、295 个头文件、183 个 `.pro` 工程。
- 识别出 182 个测试/程序目标。
- Qt 模块使用量：Core 181、Gui 93、Widgets 72、Network 26、Concurrent 18、WebSockets 10、Multimedia 3、TestLib 179。
- 找到 11 处 Qt 5/MinGW/部署路径硬编码。

### 第二遍：Qt 6 API 兼容性

重点发现并处理：

- `QRegExp` 到 `QRegularExpression`。
- `QTextCodec` 到 `QStringDecoder`，以及 `QTextStream::setCodec` 到 `setEncoding`。
- `QDesktopWidget` 到 `QScreen`。
- `QAudioInput`/默认音频设备接口到 `QAudioSource`、`QMediaDevices`、`QAudioDevice`。
- `QSound` 到 `QSoundEffect`。
- Qt 6 容器、鼠标坐标、滚轮事件、字体度量、原生事件过滤器等签名变化。
- `QLibraryInfo::location` 到 `QLibraryInfo::path`。

### 第三遍：构建、测试和发布假设

- 检查全部 182 个目标的 qmake 配置。
- 识别 Qt 5 DLL、OpenSSL 1.0、旧 Multimedia/Bearer 插件和旧部署目录结构。
- 确认运行期必须附带 OCR 助手、OCR 模型、Windows 语音助手和中文 Qt 翻译。

## 主要迁移改动

- 新增统一屏幕定位辅助代码，替换已删除的桌面屏幕 API。
- 完成 Qt 6 音频录制和提示音 API 适配。
- 修复 Qt 6 下拖放坐标、场景事件和上下文菜单生命周期问题。
- 修复 `QAbstractButton::click()` 信号期间同步删除父控件造成的生命周期崩溃，改为队列回调。
- 修复 Qt 6 `QProcess` 分块行为导致的超长助手输出判定差异。
- 修复中文界面在 200% 字体缩放下的字体传播和滚动显示。
- 补齐三个自定义控件的 `Q_OBJECT` 元对象声明和测试工程头文件依赖。
- 更新测试运行器，使 Qt 6 GUI 测试和 QtTest 输出聚合稳定。

## 验证结果

### 主程序

- Debug 构建：成功。
- Release 构建：成功。
- Release 可执行文件：`.qt6-build\release\vocekit.exe`。

### 全量自动化测试

```text
Projects               : 182
QtPrograms             : 179
StandalonePrograms     : 3
Passed                 : 1817
Failed                 : 0
Skipped                : 0
InfrastructureFailures : 0
```

额外验证包括 OCR 35 项测试、功能画布拖放测试、中文 200% 字体缩放，以及 Windows 语音设置在 100%/125%/150% 缩放下的显示。

### 独立发布和启动

- `windeployqt` 成功部署 Qt 6 Core、Gui、Widgets、Network、WebSockets、Multimedia、Svg、平台插件、图片插件、TLS 和 MinGW 运行库。
- OCR、模型文件、Windows OCR/语音助手和 `qt_zh_CN.qm` 已打包。
- 干净发布包生成时为 40 个文件、122,950,013 字节。
- 随后把所有 `D:\QT66666` 路径从 PATH 中移除，启动发布版 5 秒；进程未退出且 `Responding=True`。
- 冒烟启动在发布目录中生成了默认设置、历史索引和日志，因此当前目录比初始干净发布包多 4 个状态文件。

### 正式开发基线收口

- 迁移目录已正式命名为 `vocekit-qt6`，原 Qt 5 目录继续保留。
- 正式入口统一为 `scripts/build.ps1`、`scripts/run-all-tests.ps1`、`scripts/deploy.ps1` 和 `scripts/package-test.ps1`。
- Debug、Release 在新目录重新生成 qmake 文件并构建成功。
- Qt 6 运行库验证检查 22 个必需文件、x64 架构和 Windows Speech 助手协议，全部通过。
- 便携测试包和 ZIP 生成成功，目录大小 122,973,712 字节，ZIP 大小 57,713,062 字节。
- 在新正式目录再次移除 Qt/MinGW PATH 后启动 5 秒，`Responding=True`，随后精确结束测试进程。
- 全量 182 个工程在正式目录重新运行，加入 Qt 6 构建目录路径回归测试后，最终为 1817 项通过、0 项失败、0 个基础设施错误。

### 旧 Qt 5 目录独立性审计

- 对原项目 1,091 个 Git 跟踪文件逐个映射到 Qt 6 项目重新核对，对应文件缺失数为 0；其中代码、工程、脚本、配置和文档均已覆盖。
- RapidOcrOnnx 完整本地 SDK 已迁入 Qt 6 项目：两边均为 1,445 个文件，缺失和长度不一致均为 0；OCR 助手可以只使用新目录重新编译。
- Windows OCR、RapidOCR 和 Windows Speech 三个运行时助手已从 Qt 6 新目录重编译并完成自检。二进制中旧项目绝对路径命中数为 0。
- 原项目的本机设置、密钥、提示词、词库、621 个历史文件和 49 个日志文件已复制到 Qt 6 目录；旧目录没有删除、移动或覆盖。
- 开发版路径规则已调整：`.qt6-build\debug` 和 `.qt6-build\release` 中的程序读取 Qt 6 项目根目录下的 `config`、`records` 等数据，不再读取构建输出目录，更不会回退到 Qt 5 目录。
- 活跃源码、脚本、测试和配置对旧项目绝对路径的引用为 0；Release 导入表只包含 Qt 6、MinGW 和 Windows 系统运行库，不包含 Qt 5 DLL。
- 最新便携 ZIP 已解压到系统临时目录，并从 PATH 中移除 Qt、MinGW、旧项目和早期原型目录后启动 8 秒：进程未退出、`Responding=True`，包内旧路径字符串命中数为 0。
- 便携包仍通过隐私检查：只带空白示例配置，不包含真实密钥、历史、录音或日志。

因此，从源码、构建依赖和运行依赖角度，`vocekit-qt6` 已不依赖旧 `vocekit` 目录。正式删除旧目录之前仍建议先把新目录加入版本控制或做一次完整归档；这是防止误删未提交成果，不是运行依赖要求。

## 使用方式

构建 Release：

```powershell
& .\scripts\build.ps1 -Configuration release
```

生成独立发布包：

```powershell
& .\scripts\deploy.ps1
```

运行全量测试：

```powershell
& .\scripts\run-all-tests.ps1
```

## 尚需人工验收

自动化迁移已经通过，但把 Qt 6 版本用于日常工作之前仍建议在真实使用环境手工检查：

- 实际麦克风录音和设备切换。
- 系统托盘、全局热键、选中文字和截图交互。
- 各云语音/模型服务的真实网络调用。
- 多显示器和不同 DPI 混用。
- Windows OCR、RapidOCR 和 Windows Speech 的完整业务流程。

当前剩余的编译提示主要是 Qt 6 弃用警告，例如 `QWebSocket::error` 和旧式快捷键整数转换，不影响本次构建、测试和启动验证，但后续可以继续清理。
