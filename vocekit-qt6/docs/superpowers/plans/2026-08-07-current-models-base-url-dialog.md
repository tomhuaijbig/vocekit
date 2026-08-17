# Current Models, Provider Base URLs, and Dialog Fix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace retired OpenAI and Claude model choices with the current official catalogs, migrate saved legacy IDs safely, add configurable official-provider Base URLs, and fix clipped Chinese action-button text in the custom-model dialog.

**Architecture:** Keep model lifecycle rules in `model_catalog`, endpoint normalization in the shared API URL utility, secrets in `SecretConfig`/`SecretStore`, and provider-specific request construction inside each provider. The settings UI only edits these values and previews normalized custom endpoints; dialog button metrics live in a small testable helper instead of changing global button styling.

**Tech Stack:** C++11, Qt 5.9 Widgets/Network/Test, MinGW 5.3, qmake, PowerShell test runner.

**Workspace safety:** The shared worktree already contains unrelated and overlapping uncommitted changes. Do not reset, clean, or bulk-stage it. Implementation checkpoints use focused tests and diffs; do not create implementation commits unless every staged hunk is independently proven to belong to this plan.

---

## File map

- `src/providers/model_catalog.cpp`: visible current catalog and legacy-ID normalization.
- `src/tasks/model_request_task.cpp`, `src/tasks/model_provider_request_task.cpp`: normalize requests before provider selection and before provider execution.
- `src/controllers/function_flow_runtime_adapters.cpp`: normalize saved flow-node model IDs before dependency validation.
- `src/api/api_client_utils.h/.cpp`: normalize OpenAI-compatible and Anthropic Messages endpoints.
- `src/config/secret_config.h`, `src/config/secret_store.cpp`: store OpenAI and Anthropic Base URLs.
- `src/providers/openai_compatible_model_provider.cpp`: current OpenAI default and optional gateway endpoint.
- `src/providers/claude_model_provider.cpp`: current Claude default and optional gateway endpoint.
- `src/ui/custom_model_dialog_support.h/.cpp`: testable dialog action sizing and endpoint preview text.
- `src/ui/api_settings_section.h/.cpp`: edit/save official Base URLs and apply the custom-dialog fixes.
- `vocekit.pro`: compile the new dialog-support unit.
- `tests/providers/model_catalog_tests.cpp`: exact model list and migration tests.
- `tests/tasks/model_request_task_tests.cpp`: request-time normalization coverage.
- `tests/controllers/function_flow_runtime_adapters_tests.cpp`: legacy flow-node migration coverage.
- `tests/api/api_client_utils_tests.cpp`: endpoint normalization matrix.
- `tests/config/secret_config_tests.cpp`: Base URL persistence round trip.
- `tests/providers/openai_compatible_model_provider_tests.cpp`: OpenAI gateway/default/invalid URL behavior.
- `tests/providers/claude_model_provider_tests.cpp`: Claude gateway/default/invalid URL behavior.
- `tests/ui/custom_model_dialog_support_tests.cpp/.pro`: font-metric and preview tests.
- `scripts/run-all-tests.ps1`: unchanged; used for the final regression gate.

### Task 1: Replace the catalog and define deterministic legacy migration

**Files:**
- Modify: `src/providers/model_catalog.cpp`
- Test: `tests/providers/model_catalog_tests.cpp`

- [ ] **Step 1: Add failing catalog and migration tests**

Extend `ModelCatalogTests` with these slots and implementations:

```cpp
void exposesOnlyCurrentOpenAiAndClaudeModels()
{
    SecretConfig secrets;
    const QVector<ModelOption> options = modelOptionsForSecrets(secrets);
    QStringList ids;
    for (const ModelOption &option : options) {
        ids.append(option.id);
    }

    QVERIFY(ids.contains(QStringLiteral("openai:gpt-5.6-sol")));
    QVERIFY(ids.contains(QStringLiteral("openai:gpt-5.6-terra")));
    QVERIFY(ids.contains(QStringLiteral("openai:gpt-5.6-luna")));
    QVERIFY(ids.contains(QStringLiteral("claude:claude-fable-5")));
    QVERIFY(ids.contains(QStringLiteral("claude:claude-opus-5")));
    QVERIFY(ids.contains(QStringLiteral("claude:claude-sonnet-5")));
    QVERIFY(ids.contains(QStringLiteral("claude:claude-haiku-4-5")));

    QVERIFY(!ids.contains(QStringLiteral("openai:gpt-5.5")));
    QVERIFY(!ids.contains(QStringLiteral("openai:gpt-5.4")));
    QVERIFY(!ids.contains(QStringLiteral("openai:gpt-5.4-mini")));
    QVERIFY(!ids.contains(QStringLiteral("claude:claude-opus-4-8")));
    QVERIFY(!ids.contains(QStringLiteral("claude:claude-opus-4-7")));
    QVERIFY(!ids.contains(QStringLiteral("claude:claude-sonnet-4-6")));
}

void migratesLegacyModelIds()
{
    const QString fallback = QStringLiteral("deepseek-v4-flash");
    QCOMPARE(normalizeModelId(QStringLiteral("openai:gpt-5.5"), fallback), QStringLiteral("openai:gpt-5.6-sol"));
    QCOMPARE(normalizeModelId(QStringLiteral("gpt-5.4"), fallback), QStringLiteral("openai:gpt-5.6-terra"));
    QCOMPARE(normalizeModelId(QStringLiteral("gpt-5.4-mini"), fallback), QStringLiteral("openai:gpt-5.6-luna"));
    QCOMPARE(normalizeModelId(QStringLiteral("gpt-4o"), fallback), QStringLiteral("openai:gpt-5.6-terra"));
    QCOMPARE(normalizeModelId(QStringLiteral("claude:claude-opus-4-8"), fallback), QStringLiteral("claude:claude-opus-5"));
    QCOMPARE(normalizeModelId(QStringLiteral("claude-sonnet-4-6"), fallback), QStringLiteral("claude:claude-sonnet-5"));
    QCOMPARE(normalizeModelId(QStringLiteral("claude-3-5-sonnet"), fallback), QStringLiteral("claude:claude-sonnet-5"));
    QCOMPARE(normalizeModelId(QStringLiteral("claude:claude-haiku-4-5"), fallback), QStringLiteral("claude:claude-haiku-4-5"));
}
```

- [ ] **Step 2: Run the model catalog test and verify RED**

Run from `tests/providers` after putting Qt and MinGW on `PATH`:

```powershell
qmake model_catalog_tests.pro -spec win32-g++ CONFIG+=release
mingw32-make -j2
.\release\model_catalog_tests.exe
```

Expected: failures because GPT-5.6/Claude 5 IDs are absent and legacy IDs still normalize to retired values.

- [ ] **Step 3: Replace the built-in catalog and implement migration before generic prefix handling**

Replace the OpenAI/Anthropic entries in `builtInModelOptions()` with:

```cpp
options << ModelOption{QStringLiteral("openai:gpt-5.6-sol"), QStringLiteral("GPT-5.6 Sol"), mcTr8("OpenAI")};
options << ModelOption{QStringLiteral("openai:gpt-5.6-terra"), QStringLiteral("GPT-5.6 Terra"), mcTr8("OpenAI")};
options << ModelOption{QStringLiteral("openai:gpt-5.6-luna"), QStringLiteral("GPT-5.6 Luna"), mcTr8("OpenAI")};
options << ModelOption{QStringLiteral("claude:claude-fable-5"), QStringLiteral("Claude Fable 5"), mcTr8("Anthropic")};
options << ModelOption{QStringLiteral("claude:claude-opus-5"), QStringLiteral("Claude Opus 5"), mcTr8("Anthropic")};
options << ModelOption{QStringLiteral("claude:claude-sonnet-5"), QStringLiteral("Claude Sonnet 5"), mcTr8("Anthropic")};
options << ModelOption{QStringLiteral("claude:claude-haiku-4-5"), QStringLiteral("Claude Haiku 4.5"), mcTr8("Anthropic")};
```

Add a private migration helper before `normalizeModelId()` uses generic prefixes:

```cpp
QString migratedLegacyModelId(QString value)
{
    value = value.trimmed().toLower();
    if (value == QStringLiteral("openai:gpt-5.5") || value == QStringLiteral("gpt-5.5")) return QStringLiteral("openai:gpt-5.6-sol");
    if (value == QStringLiteral("openai:gpt-5.4") || value == QStringLiteral("gpt-5.4")) return QStringLiteral("openai:gpt-5.6-terra");
    if (value == QStringLiteral("openai:gpt-5.4-mini") || value == QStringLiteral("gpt-5.4-mini")) return QStringLiteral("openai:gpt-5.6-luna");
    if (value.startsWith(QStringLiteral("openai:gpt-4")) || value.startsWith(QStringLiteral("gpt-4"))) return QStringLiteral("openai:gpt-5.6-terra");
    if (value == QStringLiteral("claude:claude-opus-4-8") || value == QStringLiteral("claude-opus-4-8") || value == QStringLiteral("claude:claude-opus-4-7") || value == QStringLiteral("claude-opus-4-7")) return QStringLiteral("claude:claude-opus-5");
    if (value == QStringLiteral("claude:claude-sonnet-4-6") || value == QStringLiteral("claude-sonnet-4-6") || value.contains(QStringLiteral("claude-3"))) return QStringLiteral("claude:claude-sonnet-5");
    return QString();
}
```

After exact option/title matching, return a non-empty migrated ID. Preserve current `custom:` and DeepSeek behavior. Change unknown OpenAI/GPT fallback to `openai:gpt-5.6-terra` and unknown Claude fallback to `claude:claude-sonnet-5`.

- [ ] **Step 4: Run the model catalog test and verify GREEN**

Run `.\release\model_catalog_tests.exe`.

Expected: all tests pass and no retired model is present in the returned catalog.

### Task 2: Apply migration at every runtime boundary

**Files:**
- Modify: `src/tasks/model_request_task.cpp`
- Modify: `src/tasks/model_provider_request_task.cpp`
- Modify: `src/controllers/function_flow_runtime_adapters.cpp`
- Test: `tests/tasks/model_request_task_tests.cpp`
- Test: `tests/controllers/function_flow_runtime_adapters_tests.cpp`

- [ ] **Step 1: Add failing runtime tests**

Add this slot to `ModelRequestTaskTests`; it uses the file's existing `FakeTaskModelProvider`:

```cpp
void normalizesLegacyModelIdBeforeProviderCall()
{
    QSharedPointer<FakeTaskModelProvider> provider(
        new FakeTaskModelProvider
    );
    ModelRequestTaskRequest request;
    request.modelId = QStringLiteral("openai:gpt-5.4");
    request.systemPrompt = QStringLiteral("system");
    request.userPrompt = QStringLiteral("user");

    const ModelRequestTaskResult result = runModelRequestTask(
        request,
        provider
    );

    QCOMPARE(result.text, QStringLiteral("completed"));
    QCOMPARE(
        provider->lastRequest.modelId,
        QStringLiteral("openai:gpt-5.6-terra")
    );
}
```

In `function_flow_runtime_adapters_tests.cpp`, create a compiled model node with `node.config.model.modelId = "claude:claude-sonnet-4-6"`, make `availableModelIds()` return `claude:claude-sonnet-5`, and assert dependency resolution succeeds with `nodeSettings.modelId == "claude:claude-sonnet-5"`.

- [ ] **Step 2: Run both focused tests and verify RED**

```powershell
qmake ..\tasks\model_request_task_tests.pro -spec win32-g++ CONFIG+=release
mingw32-make -j2
.\release\model_request_task_tests.exe
qmake function_flow_runtime_adapters_tests.pro -spec win32-g++ CONFIG+=release
mingw32-make -j2
.\release\function_flow_runtime_adapters_tests.exe
```

Expected: the old IDs are forwarded or rejected.

- [ ] **Step 3: Normalize classic provider requests**

Include `../providers/model_catalog.h` and `../config/app_settings_defaults.h` in `model_request_task.cpp`. When building `ModelRequest request`, assign:

```cpp
request.modelId = normalizeModelId(
    taskRequest.modelId,
    defaultModelForFunction(QString())
);
```

Include `../providers/model_catalog.h` in `model_provider_request_task.cpp`, copy the request, and normalize before both provider selection and execution:

```cpp
ModelRequestTaskRequest normalized = request;
normalized.modelId = normalizeModelId(
    request.modelId,
    defaultModelForFunction(QString())
);
const QSharedPointer<IModelProvider> provider =
    createBuiltInModelProvider(
        modelProvider(normalized.modelId),
        normalized.useSystemProxy
    );
return runModelRequestTask(normalized, provider, onDelta);
```

- [ ] **Step 4: Normalize flow-node dependencies before validation**

In the model-node branch of `FunctionFlowRuntimeAdapters::resolveDependencies`, compute:

```cpp
const QString normalizedModelId = normalizeModelId(
    node.config.model.modelId,
    defaultModelForFunction(QString())
);
```

Validate `normalizedModelId` against `modelIds` and assign it to `nodeSettings.modelId`. Do not mutate the draft graph during runtime resolution.

- [ ] **Step 5: Run both focused tests and verify GREEN**

Expected: classic and canvas flows both use visible current IDs while saved files remain untouched until normal save.

### Task 3: Add endpoint normalization and secret persistence

**Files:**
- Modify: `src/api/api_client_utils.h`
- Modify: `src/api/api_client_utils.cpp`
- Modify: `src/config/secret_config.h`
- Modify: `src/config/secret_store.cpp`
- Test: `tests/api/api_client_utils_tests.cpp`
- Test: `tests/config/secret_config_tests.cpp`

- [ ] **Step 1: Add failing URL and persistence tests**

Add these endpoint cases to `ApiClientUtilsTests`:

```cpp
void normalizesAnthropicMessageUrls()
{
    QCOMPARE(anthropicMessagesUrl(QStringLiteral("gateway.example.com")).toString(), QStringLiteral("https://gateway.example.com/v1/messages"));
    QCOMPARE(anthropicMessagesUrl(QStringLiteral("https://gateway.example.com/v1")).toString(), QStringLiteral("https://gateway.example.com/v1/messages"));
    QCOMPARE(anthropicMessagesUrl(QStringLiteral("https://gateway.example.com/v1/messages")).toString(), QStringLiteral("https://gateway.example.com/v1/messages"));
    QVERIFY(anthropicMessagesUrl(QString()).isEmpty());
}
```

In `SecretConfigTests::savesAndLoadsSpeechAndModelSecrets`, set and compare:

```cpp
secrets.openaiBaseUrl = QStringLiteral("https://openai-gateway.example/v1");
secrets.anthropicBaseUrl = QStringLiteral("https://claude-gateway.example/v1");
// after load
QCOMPARE(loaded.openaiBaseUrl, secrets.openaiBaseUrl);
QCOMPARE(loaded.anthropicBaseUrl, secrets.anthropicBaseUrl);
```

- [ ] **Step 2: Run API utility and secret tests and verify RED**

Expected: compile failure because the fields/function do not exist.

- [ ] **Step 3: Implement shared endpoint normalization**

Declare `QUrl anthropicMessagesUrl(QString text);` in `api_client_utils.h` and add this implementation:

```cpp
QUrl anthropicMessagesUrl(QString text)
{
    text = text.trimmed();
    if (text.isEmpty()) return QUrl();
    if (!text.contains(QStringLiteral("://"))) text.prepend(QStringLiteral("https://"));
    while (text.endsWith(QLatin1Char('/'))) text.chop(1);
    if (!text.endsWith(QStringLiteral("/messages"))) {
        text += text.endsWith(QStringLiteral("/v1"))
            ? QStringLiteral("/messages")
            : QStringLiteral("/v1/messages");
    }
    return QUrl(text);
}
```

Retain `openAiCompatibleChatUrl()` as the OpenAI normalizer; add invalid/hostless assertions to its existing tests.

- [ ] **Step 4: Add and persist Base URL fields**

Add to `SecretConfig`:

```cpp
QString openaiBaseUrl;
QString anthropicBaseUrl;
```

Load and save JSON keys exactly as:

```cpp
secrets.openaiBaseUrl = root.value(QStringLiteral("openai_base_url")).toString().trimmed();
secrets.anthropicBaseUrl = root.value(QStringLiteral("anthropic_base_url")).toString().trimmed();
root.insert(QStringLiteral("openai_base_url"), secrets.openaiBaseUrl.trimmed());
root.insert(QStringLiteral("anthropic_base_url"), secrets.anthropicBaseUrl.trimmed());
```

- [ ] **Step 5: Run both focused tests and verify GREEN**

Expected: root, `/v1`, full endpoint, empty value, and JSON round trip all pass.

### Task 4: Use current defaults and configurable endpoints in both providers

**Files:**
- Modify: `src/providers/openai_compatible_model_provider.cpp`
- Modify: `src/providers/claude_model_provider.cpp`
- Test: `tests/providers/openai_compatible_model_provider_tests.cpp`
- Test: `tests/providers/claude_model_provider_tests.cpp`

- [ ] **Step 1: Update provider tests first**

Change existing request expectations to `gpt-5.6-terra` and `claude-sonnet-5`. Add the following OpenAI cases using the file's fake transport:

```cpp
void officialProviderUsesConfiguredBaseUrl()
{
    const QSharedPointer<FakeOpenAiTransport> transport = fakeTransport();
    transport->jsonResponse.statusCode = 200;
    transport->jsonResponse.body = QByteArrayLiteral(
        "{\"choices\":[{\"message\":{\"content\":\"ok\"}}]}"
    );
    SecretConfig secrets = openAiSecrets();
    secrets.openaiBaseUrl = QStringLiteral("gateway.example.com/v1");
    OpenAiCompatibleModelProvider provider(
        QStringLiteral("openai"), transport, [secrets]() { return secrets; }
    );
    CancellationSource cancellation;
    ModelRequest request;
    request.modelId = QStringLiteral("openai:gpt-5.6-terra");
    request.userPrompt = QStringLiteral("test");

    const ModelResult result = provider.complete(
        request, ModelDeltaCallback(), cancellation.token()
    );

    QVERIFY(result.error.isEmpty());
    QCOMPARE(
        transport->lastRequest.url(),
        QUrl(QStringLiteral("https://gateway.example.com/v1/chat/completions"))
    );
}

void officialProviderRejectsInvalidBaseUrl()
{
    const QSharedPointer<FakeOpenAiTransport> transport = fakeTransport();
    SecretConfig secrets = openAiSecrets();
    secrets.openaiBaseUrl = QStringLiteral("not a valid url");
    OpenAiCompatibleModelProvider provider(
        QStringLiteral("openai"), transport, [secrets]() { return secrets; }
    );
    CancellationSource cancellation;
    ModelRequest request;
    request.modelId = QStringLiteral("openai:gpt-5.6-terra");

    const ModelResult result = provider.complete(
        request, ModelDeltaCallback(), cancellation.token()
    );

    QCOMPARE(result.error.code, QStringLiteral("provider.configuration"));
    QCOMPARE(transport->postJsonCount, 0);
    QCOMPARE(transport->postStreamCount, 0);
}
```

Add equivalent Claude cases:

```cpp
void usesConfiguredAnthropicBaseUrl()
{
    const QSharedPointer<FakeClaudeTransport> transport = fakeTransport();
    transport->jsonResponse.statusCode = 200;
    transport->jsonResponse.body = QByteArrayLiteral(
        "{\"content\":[{\"type\":\"text\",\"text\":\"ok\"}]}"
    );
    SecretConfig secrets = claudeSecrets();
    secrets.anthropicBaseUrl = QStringLiteral("gateway.example.com/v1");
    ClaudeModelProvider provider(
        transport, [secrets]() { return secrets; }
    );
    CancellationSource cancellation;
    ModelRequest request;
    request.modelId = QStringLiteral("claude:claude-sonnet-5");

    const ModelResult result = provider.complete(
        request, ModelDeltaCallback(), cancellation.token()
    );

    QVERIFY(result.error.isEmpty());
    QCOMPARE(
        transport->lastRequest.url(),
        QUrl(QStringLiteral("https://gateway.example.com/v1/messages"))
    );
}

void rejectsInvalidAnthropicBaseUrl()
{
    const QSharedPointer<FakeClaudeTransport> transport = fakeTransport();
    SecretConfig secrets = claudeSecrets();
    secrets.anthropicBaseUrl = QStringLiteral("not a valid url");
    ClaudeModelProvider provider(
        transport, [secrets]() { return secrets; }
    );
    CancellationSource cancellation;
    ModelRequest request;
    request.modelId = QStringLiteral("claude:claude-sonnet-5");

    const ModelResult result = provider.complete(
        request, ModelDeltaCallback(), cancellation.token()
    );

    QCOMPARE(result.error.code, QStringLiteral("provider.configuration"));
    QCOMPARE(transport->postJsonCount, 0);
    QCOMPARE(transport->postStreamCount, 0);
}
```

- [ ] **Step 2: Run provider tests and verify RED**

Expected: old defaults and official fixed endpoints make the new assertions fail.

- [ ] **Step 3: Resolve OpenAI endpoint/default from secrets**

In official OpenAI `resolveConfig()` use:

```cpp
config.displayName = QStringLiteral("OpenAI");
config.endpoint = secrets.openaiBaseUrl.trimmed().isEmpty()
    ? QUrl(QStringLiteral("https://api.openai.com/v1/chat/completions"))
    : openAiCompatibleChatUrl(secrets.openaiBaseUrl);
config.apiKey = secrets.openaiApiKey.trimmed();
config.defaultModel = QStringLiteral("gpt-5.6-terra");
```

Return `provider.configuration` before the key check completes if a non-empty normalized URL is invalid or hostless. Update the provider self-check model to `openai:gpt-5.6-terra`.

- [ ] **Step 4: Resolve Claude endpoint/default from secrets**

Include `../api/api_client_utils.h`. Add a private resolver that returns the official endpoint for empty input and `anthropicMessagesUrl(m_secrets.anthropicBaseUrl)` otherwise. Validate it before constructing `QNetworkRequest`. Change `requestModelName()` and self-check defaults to `claude-sonnet-5`.

- [ ] **Step 5: Run both provider test projects and verify GREEN**

Expected: official defaults, gateway roots, and invalid non-empty URLs behave deterministically without real network calls.

### Task 5: Add Base URL controls and repair the custom-model dialog

**Files:**
- Create: `src/ui/custom_model_dialog_support.h`
- Create: `src/ui/custom_model_dialog_support.cpp`
- Modify: `src/ui/api_settings_section.h`
- Modify: `src/ui/api_settings_section.cpp`
- Modify: `vocekit.pro`
- Create: `tests/ui/custom_model_dialog_support_tests.cpp`
- Create: `tests/ui/custom_model_dialog_support_tests.pro`

- [ ] **Step 1: Write failing dialog-support tests**

Create a Qt Widgets test that constructs a `QPushButton`, applies `styleCustomModelDialogActionButton()`, and checks:

```cpp
QVERIFY(button.minimumHeight() >= 40);
QVERIFY(button.minimumHeight() >= QFontMetrics(button.font()).height() + 16);
QVERIFY(button.styleSheet().contains(QStringLiteral("padding: 0 12px")));
QCOMPARE(customModelEndpointPreviewText(QStringLiteral("api.example.com")), QStringLiteral("最终请求地址：https://api.example.com/v1/chat/completions"));
QCOMPARE(customModelEndpointPreviewText(QString()), QStringLiteral("最终请求地址：等待填写 API URL"));
```

- [ ] **Step 2: Run the new test and verify RED**

Expected: compile failure because the support unit does not exist.

- [ ] **Step 3: Implement the dialog-support unit**

Public declarations:

```cpp
class QPushButton;
void styleCustomModelDialogActionButton(
    QPushButton *button,
    const QString &background,
    const QString &foreground
);
QString customModelEndpointPreviewText(const QString &input);
```

Implementation requirements:

```cpp
const int minimumHeight = qMax(
    40,
    QFontMetrics(button->font()).height() + 16
);
button->setMinimumHeight(minimumHeight);
button->setStyleSheet(QStringLiteral(
    "QPushButton { background: %1; color: %2; border: none; "
    "border-radius: 6px; padding: 0 12px; font-weight: 600; }"
).arg(background, foreground));
```

For preview text, normalize with `openAiCompatibleChatUrl()`, require a valid host, and return either `最终请求地址：<url>`, `最终请求地址：等待填写 API URL`, or `最终请求地址：API URL 无效`.

- [ ] **Step 4: Add official Base URL fields to the settings UI**

Add `m_openaiBaseUrlEdit` and `m_anthropicBaseUrlEdit` members. Initialize them from `SecretConfig`, insert `plainInputRow` entries directly below the matching API-key rows, explain empty=official and root/`v1`/full endpoint behavior in `apiRowDetailText()`, and save both values in `saveSecretsFromUi()`.

Use placeholders:

```cpp
apiTr8("留空使用官方地址，例如：https://gateway.example.com/v1")
```

- [ ] **Step 5: Make custom URL meaning and final endpoint explicit**

Change the row label to `API URL（接口地址）`, use `https://api.example.com/v1/chat/completions` as the example, add a word-wrapped preview label below the URL row, and connect `QLineEdit::textChanged` to refresh it with `customModelEndpointPreviewText()`.

- [ ] **Step 6: Apply robust sizing to all five dialog actions**

Call `styleCustomModelDialogActionButton()` for `新增模型`, `测试`, `删除`, `取消`, and `保存`. Remove `setFixedHeight(32/36/38)` for these controls. Use dark/white/red foreground-background combinations matching the current design.

- [ ] **Step 7: Register sources and run the new test GREEN**

Add the support `.cpp` to `SOURCES` and `.h` to `HEADERS` in `vocekit.pro`. The test `.pro` links Qt Widgets/Test plus `api_client_utils.cpp` and `custom_model_dialog_support.cpp`.

Expected: button metrics and URL preview tests pass.

### Task 6: Integration verification and release rebuild

**Files:**
- Verify all files listed above
- Do not modify unrelated dirty files

- [ ] **Step 1: Run focused projects**

Run the model catalog, model request, flow runtime adapter, API utility, secret config, OpenAI provider, Claude provider, and custom dialog support test executables. Expected: zero failures.

- [ ] **Step 2: Build the release executable**

```powershell
$env:Path='D:\QQQQQT0001\5.9\mingw53_32\bin;D:\QQQQQT0001\Tools\mingw530_32\bin;' + $env:Path
qmake vocekit.pro -spec win32-g++ CONFIG+=release
mingw32-make -j2
```

Expected: exit code 0 and `release\vocekit.exe` updated.

- [ ] **Step 3: Run the complete regression suite**

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run-all-tests.ps1 -Configuration release
```

Expected: all discovered projects pass, with zero failed and zero infrastructure failures.

- [ ] **Step 4: Perform visual clipping checks**

Launch the release app, open Settings → Interfaces → Custom Models, and inspect one-profile and multi-profile states at normal Windows scaling and increased text scaling. Confirm the full glyphs for `测试` and `删除`, no overlap with the profile title, usable long URL input, readable endpoint preview, and scroll access to all rows/buttons. Repeat with the main window both compact and maximized.

- [ ] **Step 5: Read back the delivered state**

Verify no retired ID appears in `modelOptions()`, read the two Base URL keys from a temporary `SecretStore` round trip, confirm no test `vocekit` process remains, and record the release executable timestamp and SHA-256. Do not send paid API requests during this verification.
