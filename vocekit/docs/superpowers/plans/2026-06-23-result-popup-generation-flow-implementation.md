# 结果小框和生成流程实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现结果现场恢复、流式失败普通请求回退、结果操作自定义、结果小框透明度、测试模板预览和每功能分阶段网络策略。

**Architecture:** 新建独立的结果流程配置和恢复文件模块，负责配置规范化、回退判断和恢复 JSON；`AppSettings` 只负责持久化。现有 `ApiClient` 增加请求级代理策略，功能运行上下文保存语音、图片和模型三类网络策略快照。结果小框根据配置动态排列按钮并报告处理完成。

**Tech Stack:** Qt 5.9 Widgets、Qt Network、Qt Test、C++11、QSaveFile、QJsonDocument。

---

### Task 1: 结果流程配置核心

**Files:**
- Create: `src/result_flow_config.h`
- Create: `src/result_flow_config.cpp`
- Create: `tests/result_flow/result_flow_tests.cpp`
- Create: `tests/result_flow/result_flow_tests.pro`
- Modify: `vocekit.pro`

- [x] 先编写配置规范化、网络策略、流式回退判断和恢复 JSON 的失败测试。
- [x] 运行测试并确认因为实现缺失而失败。
- [x] 实现最小配置核心和恢复文件读写。
- [x] 运行测试并确认通过。

### Task 2: 配置迁移和即时保存

**Files:**
- Modify: `src/voiceassistant.cpp`
- Modify: `config/settings.example.json`
- Test: `tests/ui/result_flow_contract.ps1`

- [x] 编写配置字段契约测试，要求 `resultActions`、`networkPolicies`、`resultPopupOpacity` 和自定义功能对应字段存在。
- [x] 运行契约测试并确认失败。
- [x] 扩展 `CustomFunctionDef` 和 `AppSettings` 的默认值、读取、清理、保存及访问方法。
- [x] 运行契约测试与配置核心测试。

### Task 3: 每功能结果操作与网络策略界面

**Files:**
- Modify: `src/voiceassistant.cpp`
- Test: `tests/ui/result_flow_contract.ps1`

- [x] 在功能页输出控制中增加结果操作列表，使用可勾选、可拖动的 `QListWidget`。
- [x] 增加语音识别、图片识别和大模型三个网络策略下拉框。
- [x] 连接列表变化和下拉框变化到即时保存。
- [x] 运行界面契约测试并编译。

### Task 4: 请求级网络策略

**Files:**
- Modify: `src/modules/api_client.inc`
- Modify: `src/ocr/ocr_cloud_client.h`
- Modify: `src/ocr/ocr_cloud_client.cpp`
- Modify: `src/voiceassistant.cpp`
- Modify: `src/modules/hub_ocr_page.inc`
- Modify: `src/modules/hub_history_page.inc`
- Modify: `src/modules/settings_panel.inc`

- [x] 为 `ApiClient` 增加请求级网络策略，并确保 HTTP 与讯飞 WebSocket 使用同一策略。
- [x] 云端 OCR 配置改用字符串网络策略。
- [x] 在功能运行上下文中保存三类策略快照，并在对应阶段应用。
- [x] 保留全局网络开关作为 `inherit` 的来源。
- [x] 编译并运行 OCR、录音和 SSL 测试。

### Task 5: 流式失败回退

**Files:**
- Modify: `src/modules/api_client.inc`
- Modify: `src/voiceassistant.cpp`
- Test: `tests/result_flow/result_flow_tests.cpp`

- [x] 测试认证、限流、模型不存在和取消不会回退，网络中断与超时会回退。
- [x] 流式失败后清空不完整内容并用普通请求重试一次。
- [x] 日志记录回退原因、接收字数和普通请求结果。
- [x] 运行结果流程测试并编译。

### Task 6: 自动保存和恢复现场

**Files:**
- Modify: `src/voiceassistant.cpp`
- Modify: `src/modules/result_choice_popup.inc`
- Test: `tests/result_flow/result_flow_tests.cpp`

- [x] 在模型处理、流式增量和结果待处理阶段保存 `config/runtime/recovery.json`。
- [x] 写入、替换、关闭或正式失败后清理恢复文件。
- [x] 启动时提示恢复、查看或丢弃，不自动重新调用接口。
- [x] 保留原有用户修改后写入历史草稿的逻辑。
- [x] 运行恢复文件测试和启动冒烟测试。

### Task 7: 结果小框透明度、操作排列和测试工具

**Files:**
- Modify: `src/modules/result_choice_popup.inc`
- Modify: `src/voiceassistant.cpp`
- Test: `tests/ui/result_flow_contract.ps1`

- [x] 结果小框按功能配置显示和排列操作，关闭固定保留。
- [x] 应用 60% 至 100% 的透明度设置。
- [x] 功能页增加透明度控制并即时保存。
- [x] 结果小框测试增加功能、模板、透明度和状态选择。
- [x] 编译并完成界面契约检查。

### Task 8: 常见问题、回归验证和测试包

**Files:**
- Modify: `src/voiceassistant.cpp`
- Modify: `docs/DEVELOPMENT_LOG.md`

- [x] 增加恢复文件、双重请求失败和网络策略问题说明。
- [x] 运行结果流程、截图、录音、OCR、常见问题分页、SSL 和界面契约测试。
- [x] 使用 Qt 5.9 MinGW 构建 release。
- [x] 运行部署脚本、运行包内启动冒烟测试并生成 `dist/vocekit-test.zip`。
