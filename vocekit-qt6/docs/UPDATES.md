# VoceKit 在线更新与发布

本项目当前采用“便携版自更新”方案：应用从 GitHub Releases 检查新版本，下载完整 ZIP，校验 SHA-256，退出主程序后由独立 PowerShell 更新程序替换运行文件。以后如果改用 Qt Installer Framework，版本号、更新检查界面和发布清单仍可继续使用。

## 已实现的范围

- `APP_VERSION` 是唯一应用版本源，qmake、应用界面和 Windows 文件版本都从这里读取。
- “设置 → 更新”显示当前版本、稳定通道、检查状态、发布说明、下载进度和发布页入口。
- 默认更新源为 GitHub 仓库 `tomhuaijbig/vocekit` 的最新正式 Release API。
- 支持 GitHub Release JSON，也支持独立 `update-manifest.json` 格式。
- 只接受 HTTPS 下载地址。
- 必须获得 64 位十六进制 SHA-256。优先使用 GitHub Asset 的 `digest`，没有时读取同名 `.sha256` 资源。
- 只允许语义版本高于当前版本时安装；草稿版本不会被安装。
- 更新程序再次校验 ZIP，防止校验后文件被替换。
- ZIP 解压前检查绝对路径和目录穿越。
- 主程序退出后才替换运行文件。
- 替换前逐文件备份；失败时删除已写入文件并恢复旧文件。
- `config`、`prompts`、`records`、`logs`、`userdata`、`user-data` 等目录永不由更新包覆盖。
- 更新结果与备份写入 `%LOCALAPPDATA%\VoceKit\updates`，不混入便携版用户配置。

## 当前安全边界

SHA-256 能发现下载损坏或资源被替换，但如果发布账户和校验值同时被攻击，它不能替代发布者身份认证。因此正式对外发布前仍应购买或申请受信任的 Windows 代码签名证书，并稳定使用同一证书签名。仓库提供了 `scripts/sign-release.ps1`，但不会生成、提交或托管私钥。

当前 `vocekit.exe` 尚未签名。没有证书时可用于本机和受控测试，不应把它描述为已经完成生产级签名。

截至 2026-08-19 的发布验证，当前 GitHub 源仓库对未登录请求返回 404，因此它还不能充当公众更新源。对外发布前必须把 Release 放在公众可访问的仓库，或提供自己的公开 HTTPS 清单地址。构建时可用下面的参数覆盖更新源，无需修改 C++：

```powershell
& .\scripts\build.ps1 -Configuration release `
  -UpdateFeedUrl "https://api.github.com/repos/你的公开账户/你的公开发布仓库/releases/latest"
```

该地址会编译进应用；不要使用需要把私有访问令牌分发给用户的更新源。

如果改用独立 JSON 清单，生成清单时还可同时指定下载地址和发布页地址：

```powershell
& .\scripts\create-update-manifest.ps1 `
  -ArchivePath .\dist\vocekit-qt6-portable.zip `
  -ReleaseBaseUrl "https://downloads.example.com/vocekit" `
  -ReleasePageBaseUrl "https://example.com/vocekit/releases"
```

## 版本规则

`APP_VERSION` 使用语义版本：

```text
主版本.次版本.补丁版本
```

例如：

- `0.1.0`：当前内部基线。
- `0.1.1`：兼容的修复版本。
- `0.2.0`：兼容的新功能版本。
- `1.0.0`：首个稳定公开版本。

Git 标签必须与版本一致并带 `v` 前缀，例如 `v0.1.1`。

## 生成发布包

先运行完整测试，再构建、部署和打包：

```powershell
& .\scripts\run-all-tests.ps1 -Configuration release
& .\scripts\build.ps1 -Configuration release
& .\scripts\deploy.ps1
& .\scripts\sign-release.ps1 -CertificateThumbprint "你的证书指纹"
& .\scripts\package-test.ps1 -PackageName vocekit-qt6-portable -RequireSignedBinaries
& .\tests\scripts\update-helper-tests.ps1
```

内部测试阶段如果还没有证书，可以暂时跳过签名命令，并去掉 `-RequireSignedBinaries`；这种包只能视为未签名测试包。

会生成：

```text
dist/vocekit-qt6-portable/
dist/vocekit-qt6-portable.zip
dist/vocekit-qt6-portable.zip.sha256
dist/update-manifest.json
```

`update-manifest.json` 包含版本、下载地址、SHA-256、通道和发布页。当前客户端通过 GitHub Release API 检查，因此发布时最关键的是 ZIP 和同名 `.sha256` 两个资源；清单也应保留，供以后切换到自建更新源。

## 代码签名

证书已安装到 Windows 当前用户或本机证书库后执行：

```powershell
& .\scripts\sign-release.ps1 `
  -CertificateThumbprint "你的证书指纹"
```

签名必须发生在打包之前。脚本会递归签名部署目录内的 EXE/DLL，并使用 `signtool verify` 复核；正式打包时的 `-RequireSignedBinaries` 会再次拒绝未签名文件。私钥和证书密码不得放入 Git、脚本参数默认值或普通日志。

## 创建 GitHub Release

1. 修改 `APP_VERSION`，例如从 `0.1.0` 改为 `0.1.1`。
2. 完成测试、Release 构建、部署、签名和打包。
3. 提交代码并创建对应标签，例如 `v0.1.1`。
4. 在 GitHub 创建正式 Release；不要勾选 draft 或 prerelease（稳定通道）。
5. 上传且保持下面两个文件名完全一致：
   - `vocekit-qt6-portable.zip`
   - `vocekit-qt6-portable.zip.sha256`
6. 填写发布说明。客户端会原样作为纯文本显示，不执行其中的 HTML。
7. 发布后用旧版本点击“设置 → 更新 → 检查更新”，完成一次 `N-1 → N` 实机升级。

## 发布门槛

每个公开版本至少验证：

- 当前完整自动化测试全部通过。
- Release 构建、部署和运行库验证通过。
- 无 Qt/MinGW 环境变量的干净 PATH 能启动部署版。
- 更新页在普通窗口、最大化、Windows 缩放和长中文发布说明下无裁切。
- 正确 ZIP 可以更新并重启。
- 错误 SHA-256 被拒绝。
- 异常中断后旧运行文件可恢复。
- 更新前后的 API Key、设置、自定义提示词、词库、历史、录音和日志均保持不变。
- EXE/DLL 的 Authenticode 签名有效且时间戳有效。

## 后续阶段

用户量扩大后再增加：

- Qt Installer Framework 安装器与 Maintenance Tool。
- stable / preview 多通道。
- 灰度发布、暂停发布和服务端回滚开关。
- 增量更新。
- 用户可点击的一键回退到上一版本。
- 默认提示词和配置结构的显式迁移器，而不是直接覆盖用户文件。

这些功能不影响当前便携版完整包更新，可以在实际发布规模需要时继续接入。
