# Windows 真实应用验收规范

本文件是静态发布规范，不是测试报告，也不是供每个版本手工改成“通过”的状态表。不得在发布时修改此 tracked Markdown 来伪造验收结果。每个正式候选的实际结果必须写入候选目录之外的独立 JSON，并由 `scripts/finalize-existing-release-candidate.ps1` 对 JSON、真实截图和冻结候选进行绑定校验。

自动化 QtTest、GitHub Actions artifact、口头确认或本文件中的说明均不能替代真实应用验收。

## 外置证据与候选绑定

证据 JSON 必须：

- 位于 `dist/releases/v<version>/` 候选目录之外；它引用的截图也必须位于证据 JSON 所在目录之下；
- 使用 `schema_version: 1` 和 `kind: "vocekit-release-acceptance-evidence"`；
- 顶层 `source_commit`、`version`、`tag` 与 `release-candidate.json` 完全一致；
- 顶层 `archive`、`sidecar`、`manifest` 各自包含并精确匹配候选记录中的 `name`、`bytes`、`sha256`；
- 顶层 `candidate_record` 包含 `name: "release-candidate.json"` 以及该文件的真实 `bytes`、`sha256`；
- 只有全部单元格真实完成后才写入顶层 `overall_status: "passed"`；
- 顶层 `cells` 恰好包含下文规定的 32 个唯一单元格。

现有校验器没有另一个顶层 `package_name` 字段，不要发明该字段。包名由已被 `candidate_record.sha256` 锚定的 `release-candidate.json.package_name` 给出，并通过顶层 `archive.name`、`sidecar.name`、`manifest.name` 及其字节数/SHA-256 绑定到实际文件。也就是说，提交、版本、标签、包文件名、字节数和 SHA-256 都必须归属于同一份冻结候选。

证据 JSON 和截图一旦用于计算 annotated 标签的 `evidence-sha256` 就进入只读状态。任何重新截图、重编码、修改 JSON、替换 ZIP 或重新签名都会使绑定失效；需要修复时必须创建新提交和新版本候选。

## 精确的 8 × 4 覆盖

每个应用类别必须分别覆盖 `100`、`125`、`150`、`200` 四个 `scale_percent`，合计恰好 32 个 `(category_id, scale_percent)` 唯一组合，不得缺失、重复或增加其他缩放档。

| 应用类别 | `category_id` | 合法 `applications[].id` 与覆盖规则 |
| --- | --- | --- |
| 系统文本编辑器 | `system_text_editor` | 必须包含 `windows_notepad` |
| Office 文字处理 | `office_word_processor` | `microsoft_word`、`wps_writer` 至少一个 |
| Office 表格/演示 | `office_spreadsheet_presentation` | `microsoft_excel`、`microsoft_powerpoint`、`wps_spreadsheet`、`wps_presentation` 至少一个 |
| 浏览器 | `browser` | 每个缩放档必须同时包含 `microsoft_edge` 和 `google_chrome` |
| 即时通信 | `instant_messaging` | 必须包含 `wechat` |
| 协作办公 | `collaboration_office` | 必须包含 `feishu` |
| 代码/文本编辑器 | `code_text_editor` | `visual_studio_code`、`notepad_plus_plus` 至少一个 |
| PDF/只读内容 | `pdf_readonly` | `edge_pdf`、`adobe_acrobat_reader` 至少一个 |

应用 ID 必须使用上表的固定值，不能用显示名称代替。每个 `applications` 元素必须包含非空 `id`、`name`、`version`，同一格内 ID 不得重复。单元格的 `application_name` 和 `application_version` 必须分别按应用数组顺序用 ` + ` 连接；浏览器格因此必须同时记录 Edge 与 Chrome 的名称、版本和各自截图。

## 每个单元格的结构

每个单元格必须包含：

- `category_id` 和 `scale_percent`；
- `status: "passed"`，仅在该格所有实际操作完成后写入；
- 非空 `application_name`、`application_version`、`windows_version`、`display_resolution`、`monitor_coordinates`、`tested_at`；
- 结构化 `applications` 数组，且符合上表应用 ID 规则；
- `screenshots` 数组，覆盖该格中的每一个应用；
- 空数组 `issues`；存在未解决问题的格不能进入通过证据；
- 恰好七个、ID 唯一且 `status: "passed"` 的 `checks`。

`tested_at` 必须是可解析的带时区时间。分辨率、屏幕坐标、Windows 版本和应用版本必须来自实际验收机器，不能使用模板占位值。

## 每个单元格的必测动作

每个单元格的 `checks` 必须恰好包含下列七个 ID，不能缺少、重复或增加未知 ID：

1. `selection_toolbar`：用鼠标拖选、双击和键盘选择文字，确认悬浮工具栏出现规则正确、不抢焦点且位置可用。
2. `actions_consent`：复制、保存到词库、翻译、解释和 AI 搜索使用无敏感内容样本；需要确认的动作在拒绝后不得发送请求。
3. `replace_selection`：自动写入和“替换选中”只操作原选区；选区失效后必须拒绝，不能退化为普通插入。
4. `response_states`：检查流式、完成、错误、降级、固定和继续追问状态下的中文长文本、按钮、下拉框、代码块和链接。
5. `screen_layout`：检查屏幕四边、任务栏、负坐标副屏、最大化和紧凑窗口，文字不得裁切、重叠或越界。
6. `privacy_boundaries`：密码框、VoceKit 自身窗口、阻止名单应用和提权窗口不得泄露选中文字或错误弹窗。
7. `late_results_cleanup`：关闭结果、切换选区、暂停观察、锁屏或退出后，迟到结果不得覆盖新会话，剪贴板和焦点必须恢复。

七项检查必须在每个应用、每个缩放档上实际完成；不能用同一格或同一截图推定其他格通过。

## 真实截图要求

每个 `screenshots` 元素必须包含：

- `application_id`：必须对应本单元格 `applications[].id`；每个被测应用至少有一张截图；
- `reference`：相对于证据 JSON 所在目录的安全相对路径；
- `sha256`：对应文件真实字节的 64 位十六进制 SHA-256。

全部 32 格范围内，截图 `reference` 和截图 SHA-256 都必须全局唯一；不得复用同一文件、同一图片字节或占位图来覆盖多个格。截图必须是验收过程中真实截取、能辨认应用和验收状态的桌面画面，不能使用生成图、空白图、测试色块或仅为满足尺寸的占位图片。

finalizer 只接受 `.png`、`.jpg`、`.jpeg`、`.webp`，会检查文件签名、完整解码、像素读取和实际 SHA-256。每张图必须至少 `320 × 180`，宽高均不得超过 `8192`，总像素不得超过 20,000,000，文件大小为 4 字节至 50 MiB；文件不得是符号链接、junction 或其他 reparse point。`reference` 不得是绝对路径、包含 `.`/`..`、Windows 保留名或逃出证据目录。

浏览器类别的每一个缩放档必须同时实测 Edge 与 Chrome，并至少分别提供一张绑定到 `microsoft_edge` 和 `google_chrome` 的真实截图。

## 生成与审核证据的规则

1. 先按 [UPDATES.md](UPDATES.md) 阶段一生成一次性 Authenticode 已签名候选；不要先创建发布标签。
2. 在隔离验收机器上只使用该候选的精确 ZIP，记录其文件名、字节数和 SHA-256。
3. 在候选目录之外建立证据目录，按 32 个唯一组合逐格实测并保存原始截图。
4. 从 `release-candidate.json` 和实际文件读取顶层绑定值，不得手抄猜测；从截图文件计算 SHA-256。
5. 任一格失败时保留真实失败记录用于修复分析，但不得把它写成 `passed` 或用空 `issues` 掩盖。修复后创建新版本候选并重新完成受影响门槛。
6. 32 格全部完成后才生成最终 JSON、计算其 SHA-256、创建绑定该哈希的 annotated 标签，并运行 `finalize-existing-release-candidate.ps1`。
7. finalizer 通过后，候选、证据 JSON 和截图仍保持只读；它们作为同一次发布审核的不可变输入。

finalizer 能机械检查字段、应用 ID、覆盖数量、图片格式/尺寸/哈希、候选绑定、标签、签名和输入未变化，但不能判断截图是否真的执行了七项人工操作。发布审核人仍须抽查原始截图、验收机器记录和 `N-1 → N` 实机升级/回滚记录，严禁用自动生成的 JSON 或截图冒充执行结果。

## 发布阻断条件

以下任一条件成立，draft Release 必须保持未公开：

- 候选没有真实有效的 Authenticode 发布者签名和 RFC 3161 时间戳；
- 外置 JSON 不是精确 32 个唯一单元格，任一格缺少七项检查、合法应用 ID 或真实截图；
- 浏览器任一缩放档没有同时覆盖 Edge 与 Chrome；
- JSON、候选记录、归档、sidecar、manifest 或截图的名称/字节数/SHA-256 不匹配；
- finalizer 未通过，或候选/证据在 finalizer 前后发生变化；
- 已有上一公开版本但尚未使用同一冻结归档完成 `N-1 → N` 实机升级、用户数据保留和故障回滚；首个公开 Release 则必须按 [UPDATES.md](UPDATES.md) 保留明确的 bootstrap 记录，不能伪造不存在的公开 `N-1`。

不得通过修改本 Markdown、把 GitHub Actions 的 unsigned artifact 当正式包、覆盖 Release 资产或强推标签来绕过上述门槛。
