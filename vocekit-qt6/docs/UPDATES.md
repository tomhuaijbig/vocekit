# VoceKit 在线更新与正式发布

VoceKit 当前采用“便携版完整包更新”：应用从公开 HTTPS 更新源读取版本信息，下载完整 ZIP，校验 SHA-256，退出主程序后由独立更新程序替换运行文件。普通开发构建和 GitHub Actions 产物只用于预检；公开更新必须走本文的两阶段正式发布流程。

## 已实现的更新边界

- `APP_VERSION` 是唯一应用版本源；qmake、应用界面和 Windows 文件版本都从这里读取。
- “设置 → 更新”显示当前版本、稳定通道、检查状态、发布说明、下载进度和发布页入口；发布说明只按纯文本显示，不执行 HTML。
- 开发和内部测试构建默认不配置更新源，更新页会明确显示“当前构建不可联网更新”。
- 只有通过 `scripts/build.ps1 -UpdateFeedUrl` 显式注入公开 HTTPS 地址的 Release 构建才启用在线更新。
- 支持 GitHub Release JSON 和独立 `update-manifest.json`；草稿版本不会被安装。
- 只接受 HTTPS 下载地址和高于当前版本的语义版本。
- 必须取得 64 位十六进制 SHA-256；优先读取 GitHub Release Asset 的 `digest`，否则读取同名 `.sha256` 资源。
- 更新程序在解压前再次校验 ZIP，并拒绝绝对路径、目录穿越和不安全条目。
- 主程序退出后才替换运行文件；替换前逐文件备份，失败时恢复旧文件。
- `config`、`prompts`、`records`、`logs`、`userdata`、`user-data` 等用户目录不会被更新包覆盖。
- 更新结果和备份位于 `%LOCALAPPDATA%\VoceKit\updates`，不混入便携版用户配置。

## 当前状态与硬性安全边界

SHA-256 只能证明下载字节与发布字节一致，不能单独证明发布者身份。正式包必须由受信任的 Windows 代码签名证书进行 Authenticode 签名，并带 RFC 3161 时间戳；私钥必须留在硬件令牌、HSM 或合规云签名服务中。不得把 PFX、P12、私钥或其 Base64 内容放入 Git、GitHub Secret、普通 runner 或日志。

现有 updater 还没有验证 ZIP/manifest 的 detached publisher signature，也没有实现客户端证书固定与轮换；它当前依赖发布前对包内 Authenticode 的审查、受保护的 GitHub 标签/Release 和 SHA-256 完整性。该协议级身份链在正式对外更新前仍必须完成：先确定云签名或自托管 HSM/硬件令牌路线，再实现 detached CMS（或等价目录签名）、客户端验证、签名证书 allowlist 与轮换/吊销策略。不能把仓库公开、哈希正确或 finalizer 通过误写成这项身份保护已经完成。

当前工作区的部署版 `vocekit.exe` 仍是未签名状态，仓库中也没有一份完成的 32 格真实验收证据。因此当前版本只能用于开发和受控预检，不能作为公开更新发布。以下门槛任意一项缺失都必须停止发布：

1. 候选包内所有要求签名的发布者文件具有真实、有效且可追溯到预期发布者的 Authenticode 签名和时间戳；
2. 精确覆盖 8 类应用 × 4 个缩放档的 32 格外置验收证据通过最终校验；
3. 如果已经存在上一公开版本，使用同一冻结归档完成并记录 `N-1 → N` 升级、用户数据保留和故障回滚。

首个公开 Release 没有真实的公开 `N-1`，不得伪造这项记录，也不能因此形成“没有首发就永远无法得到 N-1”的循环。首发只能走本文后述的 bootstrap 例外：签名、32 格验收、全新目录启动、更新脚本故障注入和回滚测试仍须全部通过；首发一旦公开，就成为下一版本必须使用的真实 `N-1`，从第二个公开版本开始不再允许该例外。

GitHub Actions 的 Release Candidate 工作流没有签名权限，只会生成带 `UNSIGNED_TEST_BUILD` 标记的短期 artifact。它使用只读仓库权限，不执行 `gh release create`，也不接触发布证书或私钥。它是自动化、部署完整性和打包结构的预检结果，不是正式候选包；不得重命名、补签、覆盖或直接提升为公开 Release 资产。

## 版本与更新源

`APP_VERSION` 使用语义版本，Git 标签必须使用完全对应的 `v` 前缀，例如版本 `0.2.0` 对应 `v0.2.0`。一个正式候选对应一个版本、一个源提交和一套冻结字节；候选出现问题时修复代码并提升版本，不能重新打包覆盖原候选。

GitHub 更新源示例：

```powershell
$updateFeed = "https://api.github.com/repos/tomhuaijbig/vocekit/releases/latest"
& .\scripts\build.ps1 -Configuration release -UpdateFeedUrl $updateFeed
```

该地址会编译进应用。不要使用需要把私有访问令牌分发给用户的更新源。

如果改用独立 JSON 清单，可用现有脚本生成与下载/发布基址绑定的 `update-manifest.json`：

```powershell
& .\scripts\create-update-manifest.ps1 `
  -ArchivePath .\dist\vocekit-qt6-portable.zip `
  -ReleaseBaseUrl "https://downloads.example.com/vocekit" `
  -ReleasePageBaseUrl "https://example.com/vocekit/releases"
```

独立清单不能绕过本文的签名、冻结候选、证据、远端 digest 和回滚门槛。

## 两阶段正式发布总览

正式发布严格分为两个阶段：

1. 在干净且与 `origin/main` 完全一致的提交上，只创建一次 Authenticode 已签名候选。此时还没有 `v*` 发布标签，候选目录一旦创建就不得覆盖或换字节。
2. 用该冻结候选完成外置真实验收；证据定稿后创建 annotated `v*` 标签，运行只读 finalizer，再把精确冻结资产上传到 draft Release。远端 digest 全部一致、适用的 `N-1` 验收通过且仓库启用 immutable releases 后，才公开发布。

任何自动化预检成功都不能跳过真实签名、外置验收、只读 finalizer、远端 digest 或 `N-1` 门槛。

## 阶段一：一次性创建已签名候选

先固定依赖并完成自动化测试，再进行 Release 构建、部署和签名。以下命令均从 `vocekit-qt6` 目录执行：

```powershell
$qtBin = "<QtBin>"
$mingwBin = "<MingwBin>"
$tag = "v0.2.0"
$updateFeed = "https://api.github.com/repos/你的公开账户/你的公开发布仓库/releases/latest"
$releaseBase = "https://github.com/你的公开账户/你的公开发布仓库/releases/download"
$releasePageBase = "https://github.com/你的公开账户/你的公开发布仓库/releases/tag"
$signerSubject = "证书中显示的发布者 Subject"
$signerThumbprint = "完整证书指纹"

& .\tests\scripts\update-helper-tests.ps1
& .\scripts\tests\windows-speech-helper-build-tests.ps1
& .\scripts\tests\runtime-helper-provenance-tests.ps1
& .\scripts\tests\release-candidate-tests.ps1
& .\scripts\tests\finalize-release-candidate-tests.ps1
& .\scripts\tests\publish-finalized-release-tests.ps1
& .\scripts\fetch-rapidocr.ps1
& .\scripts\build-runtime-helpers.ps1
& .\scripts\run-all-tests.ps1 -Configuration release -QtBin $qtBin -MingwBin $mingwBin
& .\scripts\build.ps1 -Configuration release `
  -QtBin $qtBin -MingwBin $mingwBin -UpdateFeedUrl $updateFeed
& .\scripts\deploy.ps1 -QtBin $qtBin -MingwBin $mingwBin
& .\scripts\sign-release.ps1 -CertificateThumbprint $signerThumbprint

& .\scripts\create-release-package.ps1 `
  -UpdateFeedUrl $updateFeed `
  -ReleaseBaseUrl $releaseBase `
  -ReleasePageBaseUrl $releasePageBase `
  -ExpectedSignerSubject $signerSubject `
  -ExpectedSignerThumbprint $signerThumbprint `
  -ExpectedTag $tag
```

`build.ps1` 对 Git 状态干净的 Release 构建始终在 qmake 前冷删并重建精确 `.qt6-build`，不会复用被 Git 忽略的旧对象；嵌入式来源 schema 同时绑定完整提交、`source_tree_clean=true`、`configuration=release`、版本和更新源。默认 `deploy.ps1` 先在全新的同级 staging 中部署和验证，再替换规范 `.qt6-deploy`，因此旧部署中的残留 DLL/文件不会进入本次正式包。自定义 `-Destination` 只用于兼容性开发流程，不是正式候选输入。

`sign-release.ps1` 只允许为 `vocekit.exe`、三个 VoceKit 辅助程序和三项随包 MinGW 运行库补充发布者签名；第三方文件已有的有效厂商签名保持不变，未知未签名文件或损坏/无效的已有签名会被拒绝。脚本完成后会同时用 `Get-AuthenticodeSignature` 和 `signtool verify /pa /all /tw` 复核，正式打包还会再次执行签名门槛。

`create-release-package.ps1` 会在 `dist/releases/v<version>/` 中一次性生成：

```text
vocekit-qt6-portable/
vocekit-qt6-portable.zip
vocekit-qt6-portable.zip.sha256
update-manifest.json
release-candidate.json
```

候选记录绑定 `source_commit`、`version`、`tag`、`package_name`、更新 URL、归档/sidecar/manifest 的文件名、字节数和 SHA-256，以及签名者和嵌入式 Release 构建来源。正式 creator 在写目录前要求同名标签在本地和 origin 都不存在；打包后、候选记录发布前后还会重新检查 HEAD 与完整 Git clean 状态，检测到变化就作废并删除本次隔离输出。脚本使用 `-FailIfOutputExists`，且只要版本目录已经存在就拒绝覆盖。不要使用删除目录、换文件或重新压缩的方式“修复”同一候选；任何代码、配置、签名或包内容变化都必须进入新的提交和新版本。

这些检查防止正常操作中的陈旧 ignored 对象、隐藏的 `assume-unchanged`/`skip-worktree`、Git 环境重定向和可观察到的中途源码变化；它们不是对已控制本机账户或管理员的恶意并发进程的密码学证明。恶意进程仍可能在两个检查点之间瞬时改写再恢复源码。正式签名机必须隔离，构建/签名/候选创建期间不得运行并发开发、同步、清理或注入进程，并应使用受控 Git 配置、受保护证书和可审计发布账户。

## 阶段二：外置验收、标签与只读 finalizer

真实应用验收必须针对阶段一的精确 ZIP 进行，并按 [ACCEPTANCE_MATRIX.md](ACCEPTANCE_MATRIX.md) 生成外置 JSON 与真实截图。证据目录必须位于 `dist/releases/v<version>/` 之外；候选目录、证据 JSON 和它引用的截图在验收定稿后均不得修改。

证据完成后计算绑定值，并创建 annotated 标签。标签标题和三个绑定行必须与 finalizer 要求完全一致：

```powershell
$releaseDir = ".\dist\releases\$tag"
$candidatePath = Join-Path $releaseDir "release-candidate.json"
$archivePath = Join-Path $releaseDir "vocekit-qt6-portable.zip"
$evidencePath = "<候选目录之外的验收证据 JSON 绝对路径>"
$sourceCommit = (git rev-parse HEAD).Trim()
$archiveSha = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant()
$evidenceSha = (Get-FileHash -LiteralPath $evidencePath -Algorithm SHA256).Hash.ToLowerInvariant()
$tagBindings = "source-commit: $sourceCommit`narchive-sha256: $archiveSha`nevidence-sha256: $evidenceSha"

git tag -a $tag -m "VoceKit release $tag" -m $tagBindings
```

最终标签内容必须等价于：

```text
VoceKit release v0.2.0

source-commit: <完整 source commit>
archive-sha256: <冻结 ZIP 的 SHA-256>
evidence-sha256: <外置证据 JSON 的 SHA-256>
```

标签必须指向 `release-candidate.json.source_commit`，此时 `HEAD` 和 `origin/main` 也必须仍等于该提交。阶段二只使用统一发布闸门；不要把 finalizer、标签推送、draft 上传和远端校验拆成彼此无锁的手工命令。

```powershell
$repository = "你的公开账户/你的公开发布仓库"
$releaseNotes = "<发布说明文件路径>"

$publishGate = @{
  CandidatePath = $candidatePath
  EvidencePath = $evidencePath
  ExpectedSignerSubject = $signerSubject
  ExpectedUpdateFeedUrl = $updateFeed
  ExpectedReleaseBaseUrl = $releaseBase
  ExpectedReleasePageBaseUrl = $releasePageBase
  ExpectedSignerThumbprint = $signerThumbprint
  Repository = $repository
  ReleaseNotesPath = $releaseNotes
}

# 默认模式：完整本地 finalizer + 只读 gh/ls-remote 预检；零远端写。
& .\scripts\publish-finalized-release.ps1 @publishGate

# 硬性警告：当前项目尚未闭合 detached publisher signature 和机器可验证的
# PublicationApproval/N-1 门槛，现在不要执行下一条命令。只有这些门槛已经实现、
# 独立审查通过且正式候选与真实验收证据均齐备时，才允许创建 verified draft。
# 显式开关会推送受保护标签并创建难以安全回收的远端 draft，不是普通预检。
& .\scripts\publish-finalized-release.ps1 @publishGate -CreateVerifiedDraft
```

闸门先锁住 candidate、ZIP、sidecar、manifest、外置 evidence、全部截图和 release notes，再在同一锁期内重新运行完整 finalizer。默认模式只做本地校验与远端 GET/`ls-remote` 预检；`-CreateVerifiedDraft` 才会推送捕获的原始 annotated-tag object OID，要求远端原先没有同名标签/Release，然后创建全新空 draft、上传恰好五项核心资产、核对 REST `state/size/digest`、下载五项重新计算 SHA-256，并再次核对同一个 Release ID 与远端标签。它从不使用 force、`--clobber`、删除、覆盖或自动恢复已有 draft。

任何远端步骤失败都保持 fail closed：已经推送的标签或部分 draft 不会被脚本删除、覆盖或重用；调查后修复代码并提升版本重做。不要手工“补齐”失败 draft 后继续发布。

## 当前只能形成 verified draft，不能公开

`publish-finalized-release.ps1 -Publish` 当前固定在任何远端写之前报错，脚本中没有 `draft=false` 路径。原因有两项仍未闭合：updater 尚无 detached publisher-signature/证书轮换协议；`N-1 → N` 或首发 bootstrap 的 `PublicationApproval` 也尚未形成机器可验证的签名 schema。即使 verified draft 的五项 digest 全部正确，也不得手工公开。

已有上一公开版本时，仍必须针对同一归档 SHA 完成 `N-1 → N` 实机升级、数据保留与故障回滚记录；首个公开 Release 的 bootstrap 记录必须明确写出“没有公开 N-1”，并改做全新目录启动、无 Qt/MinGW PATH 启动、设置/日志边界和更新故障注入/回滚。后续只有在 detached publisher signature、证书 allowlist/轮换和可机验 PublicationApproval 全部实现并经过独立审查后，才能新增唯一的公开提升动作；本文不提供任何绕过命令。

## 公开发布门槛

每个公开版本至少同时满足：

- 当前完整自动化测试、Release 构建、部署和运行库验证通过；
- 无 Qt/MinGW 环境变量的干净 PATH 能启动部署版；
- 候选包发布者签名、证书指纹和 RFC 3161 时间戳全部有效；
- 外置验收 JSON 精确覆盖 32 个唯一单元格，每格七项检查通过且真实截图哈希有效；
- draft Release 的五项冻结资产与本地文件的名称、字节数和远端 digest 全部一致；
- 已有上一公开版本时，使用相同归档 SHA 完成 `N-1 → N` 升级、设置和用户数据保留、异常中断回滚；首个公开 Release 则必须保留明确的 bootstrap 记录；
- 升级前后的 API Key、设置、自定义提示词、词库、历史、录音和日志均保持不变；
- 错误 SHA-256 会被拒绝，正确 ZIP 可更新并重启；
- 更新页在普通窗口、最大化、100%～200% 缩放和长中文说明下无裁切；
- 仓库 immutable releases 已启用，发布后不再修改标签或资产。

## 后续阶段

用户量扩大后可继续增加 Qt Installer Framework、stable/preview 多通道、灰度与暂停开关、增量更新和显式配置迁移器。这些后续能力不能降低当前完整包发布的签名、证据、digest 和回滚门槛。
