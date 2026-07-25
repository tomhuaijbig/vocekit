# vocekit 模块化架构重构实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在不破坏现有设置、历史、词库、快捷键和用户工作流的前提下，将 `voiceassistant.cpp` 与 `.inc` 模块迁移为可独立测试、独立编译的 Qt 5.9 C++11 模块。

**Architecture:** 迁移采用“先建立数据与接口，再替换调用者，最后删除旧实现”的顺序。共享状态由 `ApplicationContext` 持有，页面通过 `ApplicationEvents` 接收变化；配置、历史、词库、提供商、语音任务和功能执行分别形成独立模块。旧 `VoiceController` 和 `HubWindow` 在迁移期间只作为临时协调入口，每个阶段都保持 Release 可编译和旧数据可读。

**Tech Stack:** Qt 5.9 Widgets、Qt Network、Qt WebSockets、Qt Multimedia、Qt Concurrent、Qt Test、C++11、qmake、MinGW 5.3 32 位。

---

## 全程规则

1. 不重置或覆盖当前未提交改动。
2. 每个任务开始前运行 `git status --short`，只提交当前任务涉及的文件。
3. 迁移一个模块后立即把旧调用改到新模块，禁止新旧实现长期同时写同一份数据。
4. 每个 JSON 写入使用 `QSaveFile` 或现有原子写入辅助函数。
5. 模块实现不创建 `QMessageBox`；只返回错误数据或发布事件。
6. 页面类不直接扫描磁盘、不直接发送网络请求。
7. 所有异步回调使用任务编号、取消标记或 `QPointer` 防止旧任务写入新状态。
8. 每一阶段至少运行对应模块测试、Release 编译和启动冒烟测试。

## 通用构建命令

```powershell
$env:QT_BIN='D:\QQQQQT0001\5.9\mingw53_32\bin'
$env:MINGW_BIN='D:\QQQQQT0001\Tools\mingw530_32\bin'
$env:OPENSSL_BIN='D:\QQQQQT0001\Tools\mingw530_32\opt\bin'
$env:PATH="$env:MINGW_BIN;$env:QT_BIN;$env:OPENSSL_BIN;$env:PATH"

& "$env:QT_BIN\qmake.exe" vocekit.pro
& "$env:MINGW_BIN\mingw32-make.exe" -j4
```

预期：生成 `release/vocekit.exe`，退出码为 0。

---

## 阶段一：建立兼容测试和基础数据类型

### Task 1: 保存当前配置、历史和词库兼容样例

**Files:**
- Create: `tests/fixtures/settings/current_settings.json`
- Create: `tests/fixtures/secrets/empty_secrets.json`
- Create: `tests/fixtures/history/history_record.json`
- Create: `tests/fixtures/history/history_index.json`
- Create: `tests/fixtures/vocabulary/entries.json`
- Create: `tests/compatibility/compatibility_tests.cpp`
- Create: `tests/compatibility/compatibility_tests.pro`

- [ ] **Step 1: 创建无隐私兼容样例**

样例必须覆盖：

```json
{
  "functionOrder": ["dictate", "translate", "ask", "custom_1"],
  "models": {
    "dictate": "deepseek-v4-flash",
    "translate": "deepseek-v4-flash",
    "ask": "deepseek-v4-pro"
  },
  "inputModes": {
    "dictate": {
      "useSelection": false,
      "useVoice": true,
      "useScreenshot": false
    }
  },
  "customFunctions": [
    {
      "id": "custom_1",
      "name": "自定义功能 1",
      "shortcut": "Ctrl+Alt+1"
    }
  ]
}
```

历史样例必须包含普通录音、分段录音、草稿、模型、耗时和收藏夹字段。词库样例必须包含全局和功能级词条。

- [ ] **Step 2: 编写当前格式解析测试**

测试断言：

```cpp
QVERIFY(settingsDocument.isObject());
QCOMPARE(settingsObject.value("functionOrder").toArray().size(), 4);
QCOMPARE(historyObject.value("modeId").toString(), QString("dictate"));
QVERIFY(historyObject.value("segments").isArray());
QCOMPARE(vocabularyArray.size(), 2);
```

- [ ] **Step 3: 运行测试并确认样例有效**

```powershell
Set-Location tests\compatibility
& "$env:QT_BIN\qmake.exe" compatibility_tests.pro
& "$env:MINGW_BIN\mingw32-make.exe" -j2
.\release\compatibility_tests.exe -txt
```

预期：全部通过。

- [ ] **Step 4: 提交基线样例**

```powershell
git add tests/fixtures tests/compatibility
git commit -m "test: lock current data compatibility"
```

### Task 2: 建立共享错误、任务和功能数据类型

**Files:**
- Create: `src/domain/operation_error.h`
- Create: `src/domain/execution_types.h`
- Create: `src/domain/function_settings.h`
- Create: `src/domain/function_settings.cpp`
- Create: `tests/domain/domain_types_tests.cpp`
- Create: `tests/domain/domain_types_tests.pro`
- Modify: `vocekit.pro`

- [ ] **Step 1: 编写规范化失败测试**

```cpp
void DomainTypesTests::normalizesFunctionSettings()
{
    FunctionSettings settings;
    settings.id = "dictate";
    settings.recording.segmentSeconds = 2;
    settings.recording.maximumMinutes = 100;
    settings.output.resultPopupSeconds = -3;

    const FunctionSettings normalized =
        normalizeFunctionSettings(settings);

    QCOMPARE(normalized.recording.segmentSeconds, 20);
    QCOMPARE(normalized.recording.maximumMinutes, 30);
    QCOMPARE(normalized.output.resultPopupSeconds, 0);
}
```

- [ ] **Step 2: 运行测试并确认失败**

预期：因 `FunctionSettings` 或 `normalizeFunctionSettings` 不存在而失败。

- [ ] **Step 3: 实现明确数据结构**

`src/domain/operation_error.h`：

```cpp
struct OperationError
{
    QString code;
    QString message;
    QString detail;
    bool retryable = false;

    bool isEmpty() const
    {
        return code.isEmpty() && message.isEmpty();
    }
};
```

`src/domain/function_settings.h` 定义：

```cpp
struct FunctionInputSettings
{
    bool useSelection = false;
    bool useVoice = false;
    bool useScreenshot = false;
    QString screenshotTriggerMode = QStringLiteral("separate");
    QString screenshotShortcut;
};

struct FunctionRecordingSettings
{
    QString triggerMode = QStringLiteral("toggle");
    bool longRecordingEnabled = false;
    int segmentSeconds = 55;
    int maximumMinutes = 30;
    int countdownSeconds = 0;
    bool beepEnabled = false;
    QString beepPath;
};

struct FunctionOutputSettings
{
    QString outputMode = QStringLiteral("resultPopup");
    QString resultTemplate = QStringLiteral("simple");
    QStringList resultActions;
    int floatingBarSeconds = 2;
    int resultPopupSeconds = 0;
};

struct FunctionSettings
{
    QString id;
    QString name;
    bool builtIn = false;
    QString shortcut;
    QString modelId;
    QString promptId;
    FunctionInputSettings input;
    FunctionRecordingSettings recording;
    FunctionOutputSettings output;
    FunctionNetworkPolicies network;
};
```

- [ ] **Step 4: 实现范围校验并运行测试**

`normalizeFunctionSettings()` 使用 `qBound()`，保持当前录音和显示时间范围。

- [ ] **Step 5: 更新 qmake 并提交**

```powershell
git add src/domain tests/domain vocekit.pro
git commit -m "refactor: add shared domain types"
```

---

## 阶段二：配置和密钥独立化

### Task 3: 拆出 AppSettingsData 和设置 JSON 转换

**Files:**
- Create: `src/config/app_settings_data.h`
- Create: `src/config/app_settings_json.h`
- Create: `src/config/app_settings_json.cpp`
- Create: `tests/config/app_settings_json_tests.cpp`
- Create: `tests/config/app_settings_json_tests.pro`
- Modify: `vocekit.pro`

- [ ] **Step 1: 编写旧设置转换测试**

测试加载 `tests/fixtures/settings/current_settings.json`，断言：

```cpp
QCOMPARE(data.functions.size(), 4);
QCOMPARE(data.function("dictate").input.useVoice, true);
QCOMPARE(data.function("translate").input.useSelection, true);
QCOMPARE(data.function("custom_1").builtIn, false);
```

- [ ] **Step 2: 确认测试失败**

预期：`AppSettingsData` 和转换函数不存在。

- [ ] **Step 3: 实现 AppSettingsData**

```cpp
struct AppSettingsData
{
    bool trayResident = true;
    bool autoStartEnabled = false;
    bool strongSelectionEnabled = false;
    bool floatingBarEnabled = true;
    bool useSystemProxy = false;
    bool vocabularyEnabled = true;
    QString vocabularyAddMode = QStringLiteral("ask");
    bool vocabularyOnlyForVoiceInput = false;
    int vocabularyPromptEntryLimit = 16;
    QString speechProvider = QStringLiteral("baidu");
    QString ocrEngine = QStringLiteral("automatic");
    int ocrTimeoutMs = 45000;
    QString recordDirectory;
    int historyInitialLoadCount = 12;
    int historyLoadMoreCount = 25;
    int logInitialLoadCount = 20;
    int logLoadMoreCount = 30;
    QVector<FunctionSettings> functions;
    QStringList functionOrder;
};
```

窗口位置和收藏夹使用独立子结构，避免继续增加平铺字段。

- [ ] **Step 4: 实现旧 JSON 到新结构转换**

必须继续识别当前键：

- `models`
- `inputModes`
- `displayTimes`
- `recordingModes`
- `outputModes`
- `resultTemplates`
- `resultActions`
- `networkPolicies`
- `customFunctions`

- [ ] **Step 5: 实现新结构写回当前 JSON 格式**

这一阶段不改变磁盘格式，确保旧版本仍能读取测试文件。

- [ ] **Step 6: 运行配置测试**

预期：旧样例读取、规范化、写回、再次读取后数据一致。

- [ ] **Step 7: 提交**

```powershell
git add src/config/app_settings_data.* src/config/app_settings_json.* tests/config vocekit.pro
git commit -m "refactor: isolate settings data conversion"
```

### Task 4: 将设置存储拆为独立类

**Files:**
- Create: `src/config/app_settings_store.h`
- Create: `src/config/app_settings_store.cpp`
- Modify: `src/voiceassistant.cpp`
- Modify: `vocekit.pro`
- Test: `tests/config/app_settings_json_tests.cpp`

- [x] **Step 1: 为文件读写增加测试**

使用 `QTemporaryDir`：

```cpp
AppSettingsStore settings(temp.filePath("settings.json"));
QVERIFY(settings.loadOrCreateDefaults());
AppSettingsData updated = settings.snapshot();
updated.historyInitialLoadCount = 36;
QVERIFY(settings.replaceAndSave(updated));
```

断言保存使用有效 JSON，且自定义功能、窗口位置和历史设置不丢失。

- [x] **Step 2: 实现独立 AppSettingsStore**

公开接口限制为：

```cpp
class AppSettingsStore
{
public:
    explicit AppSettingsStore(const QString &path = QString());
    bool load(OperationError *error = nullptr);
    bool loadOrCreateDefaults(OperationError *error = nullptr);
    bool save(OperationError *error = nullptr) const;
    const AppSettingsData &snapshot() const;
    FunctionSettings function(const QString &id) const;
    bool updateFunction(const FunctionSettings &function);
    void updateGlobal(const AppSettingsData &data);
    bool replaceAndSave(
        const AppSettingsData &data,
        OperationError *error = nullptr
    );
};
```

页面和控制器只接收 `AppSettingsData` 快照及保存回调，不直接持有存储服务，也不自行打开设置文件。

- [x] **Step 3: 从 `voiceassistant.cpp` 删除旧类实现**

用：

```cpp
#include "config/app_settings_store.h"
```

替换 1,300 行左右的内嵌 `AppSettings`。

- [x] **Step 4: 编译并运行全部现有测试**

当前实现补充：程序入口唯一持有 `AppSettingsStore`；页面和控制器通过 `AppSettingsData` 快照及保存回调访问配置。`replaceAndSave()` 在磁盘写入失败时回滚内存快照，旧 `AppSettings` 与 `legacy_app_settings.h` 已移除。

预期：Release 编译通过，配置样例兼容。

- [ ] **Step 5: 提交**

```powershell
git add src/config/app_settings_store.* src/voiceassistant.cpp vocekit.pro tests/config
git commit -m "refactor: extract app settings module"
```

### Task 5: 拆出 SecretConfig 和密钥存储

**Files:**
- Create: `src/config/secret_config.h`
- Create: `src/config/secret_config.cpp`
- Create: `src/config/secret_store.h`
- Create: `src/config/secret_store.cpp`
- Create: `tests/config/secret_store_tests.cpp`
- Modify: `src/voiceassistant.cpp`
- Modify: `src/modules/settings_panel.inc`
- Modify: `vocekit.pro`

- [ ] **Step 1: 编写空密钥和自定义模型测试**

覆盖：

- 空白示例配置。
- 百度示例代码解析。
- 多个自定义大模型。
- 旧单个自定义模型字段转换。
- 保存时不丢失未显示的语音服务密钥。

- [ ] **Step 2: 实现 SecretStore**

```cpp
class SecretStore
{
public:
    explicit SecretStore(const QString &path = QString());
    SecretConfig load(OperationError *error = nullptr) const;
    bool save(const SecretConfig &config, OperationError *error = nullptr) const;
};
```

- [ ] **Step 3: 替换全局 `loadSecrets()` 和 `saveSecrets()`**

先提供临时兼容函数调用 `SecretStore`，待提供商迁移完成后删除兼容函数。

- [ ] **Step 4: 编译、测试和提交**

```powershell
git add src/config src/voiceassistant.cpp src/modules/settings_panel.inc tests/config vocekit.pro
git commit -m "refactor: isolate secret configuration"
```

---

## 阶段三：统一事件和取消

### Task 6: 建立 ApplicationEvents

**Files:**
- Create: `src/app/application_events.h`
- Create: `src/app/application_events.cpp`
- Create: `tests/app/application_events_tests.cpp`
- Create: `tests/app/application_events_tests.pro`
- Modify: `vocekit.pro`

- [ ] **Step 1: 编写单次事件测试**

使用 `QSignalSpy`：

```cpp
ApplicationEvents events;
QSignalSpy spy(&events, SIGNAL(historyChanged(HistoryChangeSet)));
events.publishHistoryChanged(change);
QCOMPARE(spy.count(), 1);
```

- [ ] **Step 2: 定义明确变化类型**

```cpp
struct SettingsChangeSet
{
    QStringList keys;
    QStringList functionIds;
};

struct HistoryChangeSet
{
    QStringList recordIds;
    bool resetRequired = false;
};

struct VocabularyChangeSet
{
    QStringList entryIds;
    bool resetRequired = false;
};
```

- [ ] **Step 3: 实现 Qt 信号并注册元类型**

禁止使用任意字符串消息作为命令。

- [ ] **Step 4: 运行测试并提交**

```powershell
git add src/app/application_events.* tests/app vocekit.pro
git commit -m "refactor: add application event center"
```

### Task 7: 建立统一 CancellationToken

**Files:**
- Create: `src/tasks/cancellation_token.h`
- Create: `src/tasks/cancellation_token.cpp`
- Create: `tests/tasks/cancellation_token_tests.cpp`
- Create: `tests/tasks/cancellation_token_tests.pro`
- Modify: `vocekit.pro`

- [ ] **Step 1: 编写共享状态测试**

```cpp
CancellationSource source;
CancellationToken first = source.token();
CancellationToken second = source.token();
QVERIFY(!first.isCancellationRequested());
source.cancel();
QVERIFY(first.isCancellationRequested());
QVERIFY(second.isCancellationRequested());
```

- [ ] **Step 2: 实现线程安全共享状态**

使用 `QSharedPointer<State>` 和 `QAtomicInt`：

```cpp
struct CancellationState
{
    QAtomicInt cancelled;
    CancellationState() : cancelled(0) {}
};
```

- [ ] **Step 3: 增加 ExecutionId**

使用 `QUuid` 字符串，异步结果必须携带执行编号。

- [ ] **Step 4: 运行测试并提交**

```powershell
git add src/tasks tests/tasks vocekit.pro
git commit -m "refactor: add unified task cancellation"
```

---

## 阶段四：统一接口提供商

### Task 8: 拆出网络请求执行器和网络策略

**Files:**
- Create: `src/providers/network_request_executor.h`
- Create: `src/providers/network_request_executor.cpp`
- Create: `src/providers/provider_types.h`
- Create: `tests/providers/network_request_executor_tests.cpp`
- Create: `tests/providers/network_request_executor_tests.pro`
- Modify: `src/modules/api_client.inc`
- Modify: `vocekit.pro`

- [x] **Step 1: 编写本地 HTTP 假服务器测试**

测试：

- 普通 JSON 请求。
- SSE 流式分片。
- 超时。
- HTTP 401。
- 连接中断。
- 取消。
- 直连与系统代理选择。

- [x] **Step 2: 实现 NetworkRequestExecutor**

接口：

```cpp
class NetworkRequestExecutor
{
public:
    NetworkResponse get(
        const QNetworkRequest &request,
        const NetworkRequestOptions &options,
        const CancellationToken &cancellation
    );

    NetworkResponse postJson(
        const QNetworkRequest &request,
        const QByteArray &body,
        const NetworkRequestOptions &options,
        const CancellationToken &cancellation
    );

    NetworkResponse postEventStream(
        const QNetworkRequest &request,
        const QByteArray &body,
        const NetworkRequestOptions &options,
        const StreamDataCallback &onData,
        const CancellationToken &cancellation
    );
};
```

- [x] **Step 3: 将 `ApiClient` 底层 HTTP 逻辑迁移到执行器**

保持请求 JSON 和错误文案不变。

- [x] **Step 4: 运行提供商测试和 SSL 冒烟测试**

- [ ] **Step 5: 提交**

```powershell
git add src/providers src/modules/api_client.inc tests/providers vocekit.pro
git commit -m "refactor: isolate network request execution"
```

### Task 9: 建立提供商接口和注册中心

**Files:**
- Create: `src/providers/speech_provider.h`
- Create: `src/providers/model_provider.h`
- Create: `src/providers/provider_registry.h`
- Create: `src/providers/provider_registry.cpp`
- Create: `tests/providers/provider_registry_tests.cpp`
- Modify: `vocekit.pro`

- [x] **Step 1: 编写假适配器路由测试**

```cpp
registry.addSpeechProvider(
    QSharedPointer<ISpeechProvider>(new FakeSpeechProvider("fake"))
);
QCOMPARE(registry.speechProvider("fake")->id(), QString("fake"));
QVERIFY(registry.speechProvider("missing").isNull());
```

- [x] **Step 2: 定义语音和模型接口**

接口使用设计文档中的请求和结果结构，返回 `OperationError`，不弹窗。

- [x] **Step 3: 实现 ProviderRegistry**

注册中心负责：

- 按 ID 查找。
- 刷新密钥配置。
- 单项自检。
- 未知 ID 错误。

- [x] **Step 4: 运行测试**

```powershell
git add src/providers tests/providers vocekit.pro
git commit -m "refactor: add provider registry"
```

### Task 10: 迁移百度、讯飞和自定义语音适配器

**Files:**
- Create: `src/providers/baidu_speech_provider.h/.cpp`
- Create: `src/providers/xfyun_speech_provider.h/.cpp`
- Create: `src/providers/custom_speech_provider.h/.cpp`
- Modify: `src/modules/api_client.inc`
- Modify: `src/voiceassistant.cpp`
- Test: `tests/providers/provider_registry_tests.cpp`

- [x] **Step 1: 使用假响应固定三种语音错误转换**

覆盖：

- 百度令牌失败。
- 讯飞 WebSocket 远端关闭。
- 自定义语音空结果。

- [x] **Step 2: 迁移百度识别实现**

适配器内部保留令牌缓存，调用者只传 PCM、网络策略和取消标记。

  当前进度：已新增独立 `BaiduSpeechProvider`，令牌与识别请求都经过可替换网络传输层；旧 `ApiClient` 中的百度请求和接口地址已删除。

- [x] **Step 3: 迁移讯飞 WebSocket**

循环发送音频分片时检查取消标记。取消后主动关闭 socket，并返回 `cancelled=true`。

  当前进度：已新增 `XfyunSpeechProvider` 和可替换的 `IProviderWebSocketTransport`，鉴权、PCM 分片、代理、超时、远端关闭、取消、响应解析和接口自检均不再依赖旧客户端。

- [x] **Step 4: 迁移自定义语音**

  当前进度：已新增独立 `CustomSpeechProvider`，接管 JSON 请求、可选鉴权、自定义模型名、响应解析、代理、取消、错误转换、运行日志和接口自检；旧 `ApiClient` 中的自定义语音实现已删除。

- [x] **Step 5: 将普通录音、长录音和历史分段重试改用注册中心**

- [x] **Step 6: 删除 `ApiClient::speechAsr()` 旧分支**

- [x] **Step 7: 运行录音测试、接口测试和 Release 编译**

### Task 11: 迁移大模型适配器

**Files:**
- Create: `src/providers/openai_compatible_provider.h/.cpp`
- Create: `src/providers/anthropic_provider.h/.cpp`
- Modify: `src/modules/api_client.inc`
- Modify: `src/voiceassistant.cpp`
- Test: `tests/providers/provider_registry_tests.cpp`

- [x] **Step 1: 固定普通和流式请求测试**

覆盖 DeepSeek、OpenAI、自定义兼容接口和 Claude。

- [x] **Step 2: 实现 OpenAI 兼容适配器**

配置：

```cpp
struct OpenAiCompatibleConfig
{
    QString providerId;
    QUrl endpoint;
    QString apiKey;
    QString model;
    QMap<QByteArray, QByteArray> headers;
};
```

- [x] **Step 3: 实现 Anthropic 适配器**

- [x] **Step 4: 保留现有流式失败普通请求回退规则**

- [x] **Step 5: 将结果小框、词库 AI、OCR 后续 AI 和接口自检改用注册中心**

- [x] **Step 6: 删除旧大模型请求实现**

**2026-07-24 进度：**

- DeepSeek、OpenAI、自定义 OpenAI 兼容接口和 Claude 均已完成独立 Provider 迁移。
- Provider 工厂和注册中心已把全部大模型路由到对应独立实现。
- 旧 `ApiClient` 中的大模型地址、普通请求和流式请求均已删除。
- Task 11 的大模型部分已经完成；百度、讯飞和自定义语音也已迁移为独立 Provider，旧 `ApiClient` 已删除。

---

## 阶段五：存储模块

### Task 12: 建立 HistoryStore

**Files:**
- Create: `src/domain/history_types.h/.cpp`
- Create: `src/storage/history_store.h/.cpp`
- Create: `tests/storage/history_store_tests.cpp`
- Create: `tests/storage/history_store_tests.pro`
- Modify: `src/modules/hub_history_page.inc`
- Modify: `src/voiceassistant.cpp`
- Modify: `vocekit.pro`

- [x] **Step 1: 编写临时目录历史测试**

覆盖：

- 追加普通记录。
- 追加分段录音记录。
- 读取详情。
- 分页查询。
- 搜索。
- 收藏夹。
- 删除。
- 索引重建。
- 分段重试后的多文件同步。
- 文本、录音、详细记录和全部导出。

- [x] **Step 2: 定义 HistoryRecord 和查询结构**

```cpp
struct HistoryQuery
{
    QString modeId;
    QString searchText;
    QString favoriteFolder;
    int offset = 0;
    int limit = 25;
};

struct HistoryQueryResult
{
    QVector<HistorySummary> records;
    int total = 0;
};
```

- [x] **Step 3: 实现当前目录结构**

所有路径生成、索引、总文件和功能分类同步都移入 `HistoryStore`。

- [x] **Step 4: 将语音和 OCR 保存改为 `HistoryRecordService`**

- [x] **Step 5: 将页面读取迁入 `HistoryStore`，修改迁入 `HistoryRecordService`**

- [x] **Step 6: 运行历史测试和现有回归测试**

- [ ] **Step 7: 提交**

### Task 13: 建立 VocabularyStore 和 PromptStore

**Files:**
- Create: `src/domain/vocabulary_types.h/.cpp`
- Create: `src/storage/vocabulary_store.h/.cpp`
- Create: `src/storage/prompt_store.h/.cpp`
- Create: `tests/storage/vocabulary_store_tests.cpp`
- Create: `tests/storage/prompt_store_tests.cpp`
- Modify: `src/modules/hub_vocabulary_page.inc`
- Modify: `src/voiceassistant.cpp`
- Modify: `vocekit.pro`

- [ ] **Step 1: 编写词库测试**

覆盖读写、去重、相关词条、作用范围、最多注入数量、版本快照和回滚。

- [ ] **Step 2: 编写提示词测试**

覆盖默认提示词、用户提示词、锁定、功能提示词选择和版本号。

- [ ] **Step 3: 实现两个存储模块**

- [ ] **Step 4: 将本地预修正、提示词注入和输出修正改用 VocabularyStore**

- [ ] **Step 5: 将旧页面改为使用存储模块**

- [ ] **Step 6: 删除全局词库和提示词静态读写函数**

- [ ] **Step 7: 运行测试并提交**

---

## 阶段六：语音控制和功能执行管线

### Task 14: 拆出 SpeechTaskController

**Files:**
- Create: `src/tasks/speech_task_controller.h`
- Create: `src/tasks/speech_task_controller.cpp`
- Create: `tests/tasks/speech_task_controller_tests.cpp`
- Modify: `src/voiceassistant.cpp`
- Modify: `vocekit.pro`

- [ ] **Step 1: 编写假录音器和假语音适配器测试**

覆盖：

- 倒计时取消。
- 切换录音。
- 按住说话。
- 60 秒以内普通录音。
- 自动分段。
- 单段重试。
- 全部分段失败。
- 取消正在识别的任务。
- 旧任务结果不覆盖新任务。

- [ ] **Step 2: 定义语音控制器接口**

```cpp
class SpeechTaskController : public QObject
{
    Q_OBJECT

public:
    ExecutionId start(
        const FunctionSettings &function,
        const CancellationToken &cancellation
    );
    void finish(const ExecutionId &id);
    void cancel(const ExecutionId &id);

signals:
    void stateChanged(const SpeechTaskState &state);
    void finished(const SpeechTaskResult &result);
};
```

- [ ] **Step 3: 迁移录音、波形、分段和识别队列**

- [ ] **Step 4: `VoiceController` 改为订阅语音结果**

- [ ] **Step 5: 删除旧录音状态成员和方法**

- [ ] **Step 6: 运行录音与语音控制测试、Release 编译并提交**

### Task 15: 建立 FunctionExecutionPipeline

**Files:**
- Create: `src/tasks/function_execution_pipeline.h`
- Create: `src/tasks/function_execution_pipeline.cpp`
- Create: `tests/tasks/function_execution_pipeline_tests.cpp`
- Modify: `src/voiceassistant.cpp`
- Modify: `src/result_flow_config.cpp`
- Modify: `vocekit.pro`

- [ ] **Step 1: 用假存储和假提供商编写管线测试**

至少覆盖：

1. 仅语音听写。
2. 选中文字翻译。
3. 选中文字问答。
4. 截图 OCR 后翻译。
5. 截图加语音。
6. 自定义功能。
7. 听写跳过模型。
8. 流式失败后普通请求回退。
9. 取消。
10. 历史保存失败。
11. 词库预修正和输出修正。

- [ ] **Step 2: 实现执行请求和结果**

使用设计文档中的 `FunctionExecutionRequest` 和 `FunctionExecutionResult`。

- [ ] **Step 3: 实现私有阶段方法**

```cpp
InputCollectionResult collectInput(...);
SpeechTaskResult collectSpeech(...);
QString applyInputVocabulary(...);
PromptRequest buildPrompt(...);
ModelResult runModel(...);
QString applyOutputVocabulary(...);
OperationError persistHistory(...);
PresentationDecision decidePresentation(...);
```

- [ ] **Step 4: 发布执行状态事件**

状态包括：

- 准备。
- 录音。
- 识别中。
- OCR 中。
- 模型处理中。
- 写入中。
- 完成。
- 失败。
- 已取消。

- [ ] **Step 5: 迁移听写功能并人工验证**

先只让听写使用新管线，翻译、问答和自定义仍使用旧入口。

- [ ] **Step 6: 迁移翻译、问答和自定义功能**

- [x] **Step 7: 删除旧 `runContext()`、`processDictate()`、`processTranslate()`、`processAsk()` 和 `processCustom()`**

  当前进度：四个 `process*()` 中转方法、`processInputThroughPipeline()` 和 `runContext()` 均已删除；`VoiceFunctionExecutionPipeline` 接管输入预处理、上下文、流式和终态分流，`VoiceRunLifecycleController` 接管模型执行与历史收尾，`VoiceResultPresentationController` 接管结果展示。

- [ ] **Step 8: 运行全量核心测试并提交**

### Task 16: 将 VoiceController 缩为输入协调器并删除

**Files:**
- Create: `src/controllers/function_command_controller.h/.cpp`
- Modify: `src/controllers/voice_controller.cpp`
- Modify: `vocekit.pro`

- [x] **Step 1: 把快捷键命令映射迁移到 FunctionCommandController**

它只负责：

- 处理快捷键按下和释放。
- 记录目标窗口。
- 启动选区读取或截图。
- 创建取消源。
- 调用执行管线。

  当前进度：`FunctionCommandController` 已统一接管快捷键按下与释放、目标窗口记录、词库快捷加入、截图入口、选中文字读取、语音配置校验和录音启动。`VoiceController` 不再保存命令运行状态，只组装独立工作流访问接口。

- [x] **Step 2: 把结果展示交给 ResultPresenter**

Create:

- `src/controllers/voice_result_presentation_controller.h`
- `src/controllers/voice_result_presentation_controller.cpp`

它负责自动写入、普通结果小框、截图结果窗和浮动条状态，不负责模型调用。

- [ ] **Step 3: 删除旧 VoiceController**

- [ ] **Step 4: 编译、人工冒烟和提交**

---

## 阶段七：页面独立化

### Task 17: 将 SettingsPanel `.inc` 改为独立页面类

**Files:**
- Create: `src/ui/pages/settings_page.h`
- Create: `src/ui/pages/settings_page.cpp`
- Delete: `src/modules/settings_panel.inc`
- Modify: `src/voiceassistant.cpp`
- Modify: `vocekit.pro`
- Create: `tests/ui/settings_page_tests.cpp`

- [ ] **Step 1: 编写页面创建和保存事件测试**

测试：

- 页面可在无主窗口情况下创建。
- 修改设置后只发布一次 `settingsChanged`。
- 保存密钥通过 `SecretStore`。
- 页面销毁后事件不会访问失效控件。

- [ ] **Step 2: 将 SettingsPanel 类移入独立文件**

构造函数改为：

```cpp
SettingsPage(
    AppSettings *settings,
    SecretStore *secrets,
    ProviderRegistry *providers,
    ApplicationEvents *events,
    QWidget *parent = nullptr
);
```

- [ ] **Step 3: 删除对全局函数和 HubWindow 的依赖**

- [ ] **Step 4: 删除 `.inc`、编译和提交**

### Task 18: 将历史页面改为独立 HistoryPage

**Files:**
- Create: `src/ui/pages/history_page.h`
- Create: `src/ui/pages/history_page.cpp`
- Delete: `src/modules/hub_history_page.inc`
- Modify: `src/voiceassistant.cpp`
- Modify: `vocekit.pro`
- Create: `tests/ui/history_page_tests.cpp`

- [ ] **Step 1: 编写分页和事件刷新测试**

- [ ] **Step 2: 将页面所需状态移入 HistoryPage**

包括搜索、筛选、选择、缓存、加载代数和分页。

- [ ] **Step 3: 所有数据操作改用 HistoryStore**

- [ ] **Step 4: 通过 ApplicationEvents 刷新**

- [ ] **Step 5: 删除 `.inc`、运行大量历史数据测试并提交**

### Task 19: 将词库页面改为独立 VocabularyPage

**Files:**
- Create: `src/ui/pages/vocabulary_page.h`
- Create: `src/ui/pages/vocabulary_page.cpp`
- Delete: `src/modules/hub_vocabulary_page.inc`
- Modify: `src/voiceassistant.cpp`
- Modify: `vocekit.pro`
- Create: `tests/ui/vocabulary_page_tests.cpp`

- [ ] **Step 1: 编写搜索、保存和事件刷新测试**

- [ ] **Step 2: 页面只依赖 VocabularyStore 和词条建议回调**

- [ ] **Step 3: 删除全局读写调用**

- [ ] **Step 4: 删除 `.inc`、编译和提交**

### Task 20: 迁移提示词、OCR、测试工具和公共控件

**Files:**
- Create: `src/ui/pages/prompts_page.h/.cpp`
- Create: `src/ui/pages/ocr_page.h/.cpp`
- Create: `src/ui/pages/diagnostics_page.h/.cpp`
- Create: `src/ui/widgets/floating_bar.h/.cpp`
- Create: `src/ui/widgets/result_choice_popup.h/.cpp`
- Create: `src/ui/widgets/history_row_frame.h/.cpp`
- Delete:
  - `src/modules/hub_ocr_page.inc`
  - `src/modules/hub_input_tests.inc`
  - `src/modules/floating_bar.inc`
  - `src/modules/result_choice_popup.inc`
  - `src/modules/history_row_frame.inc`
- Modify: `src/voiceassistant.cpp`
- Modify: `vocekit.pro`

- [ ] **Step 1: 每个页面和控件增加独立创建测试**

- [ ] **Step 2: 逐个迁移，不同时删除多个旧文件**

顺序：

1. `HistoryRowFrame`
2. `FloatingBar`
3. `ResultChoicePopup`
4. `PromptsPage`
5. `OcrPage`
6. `DiagnosticsPage`

- [ ] **Step 3: 每迁移一个模块都编译一次**

- [ ] **Step 4: 删除对应 `.inc` 并提交**

---

## 阶段八：主窗口、平台和启动

### Task 21: 建立精简 HubWindow

**Files:**
- Create: `src/ui/hub_window.h`
- Create: `src/ui/hub_window.cpp`
- Create: `src/ui/page_router.h/.cpp`
- Delete: `src/modules/command_center_shell.inc`
- Modify: `src/voiceassistant.cpp`
- Modify: `vocekit.pro`

- [ ] **Step 1: 编写页面切换测试**

测试主页、历史、词库、提示词、图片识别、测试、日志、设置和常见问题路由。

- [ ] **Step 2: 迁移窗口外壳、导航和页面创建**

`HubWindow` 不再包含页面数据读写方法。

- [ ] **Step 3: 常见问题跳转改用 PageRouter**

删除 `g_openFaqCallback`。

- [ ] **Step 4: 删除旧 HubWindow 实现并提交**

### Task 22: 拆出 Windows 平台能力

**Files:**
- Create:
  - `src/platform/global_hotkeys.h/.cpp`
  - `src/platform/selection_reader.h/.cpp`
  - `src/platform/clipboard_bridge.h/.cpp`
  - `src/platform/tray_controller.h/.cpp`
  - `src/platform/windows_autostart.h/.cpp`
  - `src/platform/chinese_text_context_menu.h/.cpp`
- Modify: `src/voiceassistant.cpp`
- Modify: `vocekit.pro`

- [ ] **Step 1: 为快捷键解析、选区读取失败和开机启动命令编写测试**

- [ ] **Step 2: 迁移全局快捷键**

原生事件仍只投递 Qt 事件，不执行录音和网络请求。

- [ ] **Step 3: 迁移选区、剪贴板、托盘和开机启动**

- [ ] **Step 4: 删除旧平台实现并提交**

### Task 23: 建立 ApplicationContext 和 VocekitApplication

**Files:**
- Create: `src/app/application_context.h`
- Create: `src/app/application_context.cpp`
- Create: `src/app/vocekit_application.h`
- Create: `src/app/vocekit_application.cpp`
- Modify: `src/main.cpp`
- Modify: `src/voiceassistant.h`
- Modify: `src/voiceassistant.cpp`
- Modify: `vocekit.pro`

- [ ] **Step 1: 编写无窗口初始化测试**

验证配置、事件、存储和提供商创建顺序。

- [ ] **Step 2: 实现 ApplicationContext**

共享模块使用明确所有权：

```cpp
class ApplicationContext
{
public:
    AppSettingsStore settings;
    SecretStore secrets;
    ApplicationEvents events;
    ProviderRegistry providers;
    HistoryStore history;
    VocabularyStore vocabulary;
    PromptStore prompts;
};
```

- [ ] **Step 3: 实现 VocekitApplication**

负责：

- Qt 应用初始化。
- 中文翻译和样式。
- 创建上下文。
- 创建窗口、任务、托盘和快捷键。
- 正确取消任务并退出。

- [ ] **Step 4: 缩小 `voiceassistant.cpp`**

最终内容：

```cpp
#include "voiceassistant.h"
#include "app/vocekit_application.h"

int runVocekit(int argc, char *argv[])
{
    VocekitApplication application;
    return application.run(argc, argv);
}
```

- [ ] **Step 5: Release 编译、启动和提交**

---

## 阶段九：清理、完整验证和发布

### Task 24: 删除旧 `.inc` 和重复实现

**Files:**
- Delete: `src/modules/*.inc`
- Modify: `vocekit.pro`
- Modify: `docs/SOURCE_STRUCTURE.md`
- Modify: `docs/AI_PROJECT_GUIDE.md`
- Modify: `docs/DEVELOPMENT_LOG.md`

- [ ] **Step 1: 搜索残留**

```powershell
rg -n '#include "modules/|src/modules/|class VoiceController|g_openFaqCallback' src vocekit.pro docs
```

预期：没有旧 `.inc` 引用和旧控制器实现。

- [ ] **Step 2: 检查文件大小**

```powershell
Get-ChildItem src -Recurse -File -Include *.cpp,*.h |
  ForEach-Object {
    [pscustomobject]@{
      Lines=(Get-Content $_.FullName).Count
      Path=$_.FullName
    }
  } |
  Sort-Object Lines -Descending |
  Select-Object -First 20
```

目标：

- `voiceassistant.cpp` 不超过 100 行。
- 新页面 `.cpp` 尽量不超过 2,000 行。
- 新业务模块尽量不超过 1,200 行。

- [ ] **Step 3: 更新文档**

文档必须准确说明新模块、依赖方向、配置迁移、任务取消和事件刷新。

- [ ] **Step 4: 提交清理**

### Task 25: 运行完整自动验证

**Files:**
- Modify tests only when a test exposes a real regression.

- [ ] **Step 1: 运行所有 Qt Test 工程**

至少包括：

- compatibility
- domain
- config
- app events
- cancellation
- providers
- storage
- speech task
- execution pipeline
- settings page
- history page
- vocabulary page
- capture
- OCR
- recording
- result flow
- FAQ
- SSL runtime

- [ ] **Step 2: 运行组件行为测试和静态检查**

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File scripts\run-all-tests.ps1

& 'C:\Program Files\Cppcheck\cppcheck.exe' `
  --enable=warning,performance,portability `
  --std=c++11 --language=c++ `
  --suppress=missingIncludeSystem `
  src
```

- [ ] **Step 3: Release 编译**

预期：无编译错误。

- [ ] **Step 4: 启动冒烟**

启动后检查：

- 普通启动显示主窗口。
- `--autostart` 不显示主窗口。
- 托盘存在。
- 页面可以切换。
- 关闭主窗口按设置隐藏或退出。

### 2026-07-24 OpenAI 兼容 Provider 迁移进展

- [x] 新增可注入网络传输和密钥快照的 `OpenAiCompatibleModelProvider`。
- [x] 官方 OpenAI 与 `custom` / `custom:<id>` 统一走独立 Provider。
- [x] 设置页自定义接口测试支持未保存表单快照。
- [x] 删除旧 `ApiClient` 中 OpenAI、自定义大模型和共用流式解析。
- [x] 修复接口自检把提供商编号当成模型名的问题。
- [x] 完成 114 个测试程序、731 项测试、发布版构建、运行库核验、SSL 冒烟和静态检查。

### 2026-07-24 Claude Provider 迁移进展

- [x] 新增可注入网络传输和密钥快照的 `ClaudeModelProvider`。
- [x] 实现 Anthropic 请求头、模型名归一化、普通响应和跨数据块 SSE 流式响应。
- [x] Provider 工厂和注册中心把 `claude` 路由到独立实现。
- [x] 删除旧 `ApiClient` 中通用模型、Claude 和旧事件流请求路径。
- [x] 完成 116 个测试程序、748 项测试、发布版构建、运行库核验、SSL 冒烟和静态检查。
- [x] 新增独立 `CustomSpeechProvider`，并删除旧客户端中的自定义语音配置、自检和识别路径。
- [x] 完成 117 个测试程序、764 项测试、发布版构建、运行库核验、SSL 冒烟和静态检查。
- [x] 新增独立 `BaiduSpeechProvider`，并删除旧客户端中的百度配置、自检、令牌和识别路径。
- [x] 完成 118 个测试程序、781 项测试、发布版构建、运行库核验、SSL 冒烟和静态检查。
- [x] 新增独立 `XfyunSpeechProvider` 和 `IProviderWebSocketTransport`，删除旧 `ApiClient` 与迁移期适配器。
- [x] 完成 119 个测试程序、790 项测试、发布版构建、运行库核验、SSL 冒烟、启动冒烟和静态检查。
- [x] OCR、本地辅助进程、云端 OCR 和截图后模型动作接入统一取消令牌。
- [x] 主语音模型流程、结果重试/追问/流式生成和结果小框关闭接入统一取消令牌。
- [x] 完成 119 个测试程序、799 项测试、发布版构建、运行库核验、SSL 冒烟、后台启动和静态检查。
- [x] 接口自检、网络诊断、Provider 配置检查、OCR 自检、DNS 查询和 HTTP 探测接入统一取消令牌。
- [x] 重复启动诊断会取消旧任务；离开测试工具页会取消仍在运行的接口和网络检查。
- [x] 完成 121 个测试工程、804 项 QtTest、2 个独立程序、发布版构建、运行库核验、SSL 冒烟、后台启动和静态检查。
- [x] 历史新增、收藏、删除、分段重试、OCR 保存和导入后索引重建统一经过 `HistoryRecordService`。
- [x] 历史页收藏、删除、导入、分段重试和收藏夹新增统一发布 `HistoryChangeSet`，由 `ApplicationEvents` 同步历史页与首页最近记录。
- [x] 完成 122 个测试工程、812 项 QtTest、2 个独立构建工程、发布版构建、运行库核验、SSL 冒烟、后台启动和静态检查。
- [x] 设置页、主页功能设置和语音控制器触发的设置修改统一发布 `SettingsChangeSet`，删除保存后的重复直接刷新。
- [x] 设置保存失败立即停止刷新；完成 122 个测试工程、815 项 QtTest 和 2 个独立构建工程验证。
- [x] 提示词保存和内置或自定义功能的编辑、新增、删除统一发布 `SettingsChangeSet`，删除附属页面直接刷新。
- [x] 完成 8 个定向测试程序、49 项测试及全量 122 个测试工程验证。
- [x] 提示词新增、复制和删除统一走设置事件；词库快捷加入只发布 `VocabularyChangeSet`。
- [x] 审计页面激活刷新与跨页面刷新边界，完成第 20 项最终收口。

### Task 26: 人工回归和测试包

**Files:**
- Modify: `docs/TESTING.md`
- Generate: `dist/vocekit-test`
- Generate: `dist/vocekit-test.zip`

- [ ] **Step 1: 人工测试核心功能**

逐项执行：

- 听写。
- 翻译选中文字。
- 问答。
- 自定义功能。
- 截图 OCR。
- 截图翻译覆盖。
- 自动写入。
- 结果小框所有操作。
- 普通和分段录音。
- 按住说话。
- 词库修正和词条回滚。
- 历史搜索、详情、删除、收藏、导入导出。
- 接口自检和网络诊断。
- 取消正在执行的任务。
- 退出后恢复未完成结果。

- [ ] **Step 2: 验证旧数据**

使用测试副本加载旧设置、旧历史、旧词库和旧提示词，禁止使用真实用户数据直接做破坏性测试。

- [ ] **Step 3: 部署和打包**

```powershell
& powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File scripts\deploy.ps1 `
  -Configuration release `
  -QtBin $env:QT_BIN `
  -MingwBin $env:MINGW_BIN `
  -OpenSslBin $env:OPENSSL_BIN

& powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File scripts\package-test.ps1 `
  -PackageName vocekit-test
```

- [ ] **Step 4: 包内启动和隐私检查**

确认测试包不包含：

- 真实密钥。
- `config/runtime/recovery.json`。
- 日志。
- 历史索引。
- 用户录音。
- 用户提示词。
- 用户词库。
- 本机绝对路径。

- [ ] **Step 5: 记录最终指标**

在开发日志中记录：

- `voiceassistant.cpp` 最终行数。
- 删除的 `.inc` 数量。
- 新增模块测试数量。
- 全量测试通过数量。
- 测试包 SHA256。

---

## 阶段停止条件

出现以下任一情况时，不进入下一阶段：

1. Release 无法编译。
2. 当前阶段新增测试失败。
3. 旧配置无法读取。
4. 历史、词库或提示词出现数据丢失。
5. 软件启动或托盘行为变化。
6. 异步任务出现悬空回调或退出崩溃。
7. 测试包包含隐私文件。

必须先修复当前阶段，再继续后续迁移。

## 最终验收

- [x] `voiceassistant.cpp` 不超过 100 行或已删除。
- [ ] `src/modules/*.inc` 全部删除。
- [ ] 设置、历史、词库、提示词、OCR 和测试工具均为独立页面类。
- [ ] 语音任务由 `SpeechTaskController` 负责。
- [ ] 功能流程由 `FunctionExecutionPipeline` 负责。
- [ ] 语音、大模型和云 OCR 通过 `ProviderRegistry` 获取适配器。
- [ ] 所有长任务使用统一取消标记和执行编号。
- [ ] UI 不直接读写历史、词库或网络。
- [x] 配置使用明确数据结构并兼容旧 JSON。
- [x] 历史读取由 `HistoryStore` 负责，生产修改由 `HistoryRecordService` 唯一负责。
- [x] 跨页面状态变化通过 `ApplicationEvents` 刷新。
- [ ] 全部自动测试、Release 构建、启动和测试包检查通过。
