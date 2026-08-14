# Selection Context Action Customization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把选中文字工具条的五个内置动作升级为用户可在软件设置页逐项改名、显隐、排序和配置行为的功能编辑器，同时保持固定动作身份、旧配置兼容及复制/保存纯本地边界。

**Architecture:** 新增一个纯配置规范化模块，统一保存五个固定动作的逐项配置；新增一个独立 Qt 编辑控件，设置卡只负责排序、展开和持久化协调。工具条继续只渲染，动作控制器继续只分派，模型请求构造器按点击时快照合并逐项模型、提示词和目标语言，本地动作不经过模型运行器。

**Tech Stack:** Qt 5.9 Widgets、QtTest、C++11、QJsonObject、qmake、MinGW 5.3 32-bit、PowerShell。

**Working directory:** `C:\Users\13736\Desktop\tts\vocekit`

**Design specification:** `docs/superpowers/specs/2026-08-14-selection-context-action-customization-design.md`

---

## Scope and file map

本规格是一条紧密耦合的配置闭环，不拆成多个计划。实施顺序固定为：配置与迁移 → 编辑控件 → 设置页接线 → 工具条展示 → AI 请求 → 本地动作 → 集成和视觉验收。

### New production files

- `src/config/selection_context_action_customization.h/.cpp`：逐项配置默认值、长度和枚举规范化、有效可见顺序、词库作用范围目录及有效显示名称。
- `src/ui/selection_context_action_editor.h/.cpp`：单个动作的展开编辑控件；只处理控件和类型化值，不保存设置、不执行动作。

### Modified production files

- `src/config/app_settings_data.h`：增加 `SelectionContextActionCustomization` 映射。
- `src/config/app_settings_json.cpp`：迁移、读取和合并写入 `actionCustomizations`，保留未知 JSON 字段。
- `src/providers/model_catalog.h/.cpp`：增加“迁移已退役型号但保留未知型号”的显式模型规范化 API，不改变现有 `normalizeModelId()` 语义。
- `src/ui/hub_settings_state.cpp`：在完整 `AppSettingsData` 可用后规范化逐项配置和动态词库范围。
- `src/ui/selection_context_settings_card.h/.cpp`：把排序列表升级为一次展开一项的编辑器，禁止隐藏最后一个可见动作，并提供单项/全部恢复默认。
- `src/ui/basic_settings_section.h/.cpp`、`src/ui/settings_panel.cpp`：向设置卡提供模型目录、词库作用范围目录和保存失败后的持久化快照刷新。
- `src/ui/selection_context_toolbar.h/.cpp`：按配置过滤隐藏项并使用自定义显示名称，点击仍发送固定 ID。
- `src/tasks/selection_context_model_request.h/.cpp`：应用动作级模型、提示词和翻译目标语言，拒绝不可用显式模型并保留 AI 搜索不可覆盖限制。
- `src/controllers/selection_context_action_controller.h/.cpp`：复制模式和保存默认作用范围；本地动作不启动模型任务。
- `src/app/selection_context_feature.h/.cpp`：把最新逐项配置传入工具条和控制器，并提供模型目录快照。
- `src/app/vocekit_application_runtime.cpp`：保存动作把规范化作用范围传给现有本地词条编辑器。
- `vocekit.pro`：注册两个新增生产模块。
- `docs/AI_PROJECT_GUIDE.md`、`docs/TESTING.md`：记录用户入口、安全边界和视觉/回归门禁。

### Focused tests

- Modify: `tests/config/app_settings_defaults_tests.cpp/.pro`
- Modify: `tests/config/app_settings_json_tests.cpp/.pro`
- Modify: `tests/providers/model_catalog_tests.cpp`
- Modify: `tests/ui/hub_settings_state_tests.cpp/.pro`
- Create: `tests/ui/selection_context_action_editor_tests.cpp/.pro`
- Modify: `tests/ui/selection_context_settings_card_tests.cpp/.pro`
- Modify: `tests/ui/basic_settings_section_tests.cpp/.pro`
- Modify: `tests/ui/selection_context_toolbar_tests.cpp/.pro`
- Modify: `tests/tasks/selection_context_model_request_tests.cpp/.pro`
- Modify: `tests/controllers/selection_context_action_controller_tests.cpp/.pro`
- Modify: `tests/app/selection_context_feature_tests.cpp/.pro`

## Focused Qt test command

Open one PowerShell session at the repository working directory and define this helper once:

```powershell
$qt = 'D:\QQQQQT0001\5.9\mingw53_32\bin'
$mingw = 'D:\QQQQQT0001\Tools\mingw530_32\bin'
$env:PATH = "$qt;$mingw;$env:PATH"

function Invoke-FocusedQtTest {
    param(
        [Parameter(Mandatory=$true)][string]$ProjectDir,
        [Parameter(Mandatory=$true)][string]$ProjectFile,
        [Parameter(Mandatory=$true)][string]$Target
    )
    Push-Location $ProjectDir
    try {
        $root = "release/.codex/$Target"
        & "$qt\qmake.exe" -o "Makefile.codex.$Target" $ProjectFile `
            -spec win32-g++ CONFIG+=release `
            "OBJECTS_DIR=$root/obj" "MOC_DIR=$root/moc" `
            "RCC_DIR=$root/rcc" "UI_DIR=$root/ui" "DESTDIR=$root/bin"
        if ($LASTEXITCODE -ne 0) { throw "qmake failed: $Target" }
        & "$mingw\mingw32-make.exe" -f "Makefile.codex.$Target" -j2
        if ($LASTEXITCODE -ne 0) { throw "make failed: $Target" }
        & ".\$root\bin\$Target.exe" -maxwarnings 0
        if ($LASTEXITCODE -ne 0) { throw "tests failed: $Target" }
    } finally {
        Pop-Location
    }
}
```

Every GREEN QtTest run must exit `0` with `0 failed` and `0 skipped`. RED evidence must be a failing assertion or the expected missing symbol/type; infrastructure, PATH, stale-object, or missing-MOC failures do not count as product RED.

---

### Task 1: Add typed customization defaults and JSON migration

**Files:**
- Create: `src/config/selection_context_action_customization.h`
- Create: `src/config/selection_context_action_customization.cpp`
- Modify: `src/config/app_settings_data.h`
- Modify: `src/config/app_settings_json.cpp`
- Modify: `src/providers/model_catalog.h`
- Modify: `src/providers/model_catalog.cpp`
- Modify: `tests/config/app_settings_defaults_tests.cpp`
- Modify: `tests/config/app_settings_defaults_tests.pro`
- Modify: `tests/config/app_settings_json_tests.cpp`
- Modify: `tests/config/app_settings_json_tests.pro`
- Modify: `tests/providers/model_catalog_tests.cpp`
- Modify: `vocekit.pro`

- [ ] **Step 1: Write RED defaults, migration, round-trip, and explicit-model tests**

Add direct assertions equivalent to:

```cpp
void defaultsExposeFiveIndependentActionCustomizations()
{
    const AppSettingsData data;
    QCOMPARE(data.selectionContext.actionCustomizations.size(), 5);
    for (const QString &id : defaultSelectionContextActionOrder()) {
        QVERIFY(data.selectionContext.actionCustomizations.contains(id));
        QVERIFY(data.selectionContext.actionCustomizations.value(id).visible);
    }
    QCOMPARE(
        data.selectionContext.actionCustomizations
            .value(selectionContextActionSave()).vocabularyScopeId,
        QStringLiteral("__global")
    );
    QCOMPARE(
        data.selectionContext.actionCustomizations
            .value(selectionContextActionCopy()).copyMode,
        QStringLiteral("original")
    );
}

void legacyOrderMigratesWithoutChangingOrder()
{
    QJsonObject selection;
    selection.insert(QStringLiteral("actionOrder"),
                     QJsonArray() << QStringLiteral("copy")
                                  << QStringLiteral("translate"));
    QJsonObject root;
    root.insert(QStringLiteral("selectionContextToolbar"), selection);
    const AppSettingsData restored = appSettingsDataFromJson(root);
    QCOMPARE(restored.selectionContext.actionOrder.first(),
             selectionContextActionCopy());
    QCOMPARE(restored.selectionContext.actionCustomizations.size(), 5);
}

void customizationRoundTripPreservesUnicodeAndUnknownJson()
{
    AppSettingsData data;
    SelectionContextActionCustomization explain =
        data.selectionContext.actionCustomizations.value(
            selectionContextActionExplain());
    explain.displayName = QString::fromUtf8("给我讲明白");
    explain.modelId = QStringLiteral("openai:gpt-5.6-sol");
    explain.promptOverride = QString::fromUtf8("用三个层次解释");
    data.selectionContext.actionCustomizations.insert(
        selectionContextActionExplain(), explain);
    data.retainedRootValues = QJsonObject{
        {QStringLiteral("selectionContextToolbar"), QJsonObject{
            {QStringLiteral("futureField"), 7},
            {QStringLiteral("actionCustomizations"), QJsonObject{
                {QStringLiteral("future-action"), QJsonObject{
                    {QStringLiteral("futureKey"), true}
                }}
            }}
        }}
    };
    const QJsonObject json = appSettingsDataToJson(data);
    const AppSettingsData restored = appSettingsDataFromJson(json);
    QCOMPARE(restored.selectionContext.actionCustomizations
                 .value(selectionContextActionExplain()).displayName,
             QString::fromUtf8("给我讲明白"));
    QVERIFY(json.value(QStringLiteral("selectionContextToolbar"))
                .toObject().contains(QStringLiteral("futureField")));
}

void explicitModelNormalizationMigratesRetiredButPreservesUnknown()
{
    QCOMPARE(normalizeExplicitModelId(QStringLiteral("gpt-5.4")),
             QStringLiteral("openai:gpt-5.6-terra"));
    QCOMPARE(normalizeExplicitModelId(QStringLiteral("gpt-5.6-sol")),
             QStringLiteral("openai:gpt-5.6-sol"));
    QCOMPARE(normalizeExplicitModelId(QStringLiteral("openai:future")),
             QStringLiteral("openai:future"));
    QCOMPARE(normalizeExplicitModelId(QStringLiteral("custom:missing")),
             QStringLiteral("custom:missing"));
}

void customizationNormalizationBoundsEveryUserField()
{
    SelectionContextActionCustomizationMap values =
        defaultSelectionContextActionCustomizations();
    SelectionContextActionCustomization translate = values.value(
        selectionContextActionTranslate());
    translate.displayName = QString(40, QLatin1Char('n'));
    translate.promptOverride = QString(9000, QLatin1Char('p'));
    translate.targetLanguage = QString(80, QLatin1Char('l'));
    values.insert(selectionContextActionTranslate(), translate);
    SelectionContextActionCustomization copy = values.value(
        selectionContextActionCopy());
    copy.copyMode = QStringLiteral("invalid");
    values.insert(selectionContextActionCopy(), copy);

    const SelectionContextActionCustomizationMap normalized =
        normalizeSelectionContextActionCustomizations(
            values,
            QStringList() << QStringLiteral("__global")
                          << QStringLiteral("translate"));
    QCOMPARE(normalized.value(selectionContextActionTranslate())
                 .displayName.size(), 24);
    QCOMPARE(normalized.value(selectionContextActionTranslate())
                 .promptOverride.size(), 8000);
    QCOMPARE(normalized.value(selectionContextActionTranslate())
                 .targetLanguage.size(), 64);
    QCOMPARE(normalized.value(selectionContextActionCopy()).copyMode,
             QStringLiteral("original"));
}
```

- [ ] **Step 2: Run the three RED projects**

```powershell
Invoke-FocusedQtTest tests/config app_settings_defaults_tests.pro app_settings_defaults_tests
Invoke-FocusedQtTest tests/config app_settings_json_tests.pro app_settings_json_tests
Invoke-FocusedQtTest tests/providers model_catalog_tests.pro model_catalog_tests
```

Expected RED: missing `SelectionContextActionCustomization`, missing `actionCustomizations`, and missing `normalizeExplicitModelId`.

- [ ] **Step 3: Implement the minimal typed model and normalization API**

Create the public contract exactly once:

```cpp
struct SelectionContextActionCustomization
{
    QString displayName;
    bool visible = true;
    QString modelId;
    QString promptOverride;
    QString targetLanguage;
    QString vocabularyScopeId = QStringLiteral("__global");
    QString copyMode = QStringLiteral("original");
};

typedef QMap<QString, SelectionContextActionCustomization>
    SelectionContextActionCustomizationMap;

SelectionContextActionCustomizationMap
defaultSelectionContextActionCustomizations();
SelectionContextActionCustomizationMap
normalizeSelectionContextActionCustomizations(
    const SelectionContextActionCustomizationMap &values,
    const QStringList &writableVocabularyScopeIds
);
QString selectionContextActionDisplayName(
    const QString &actionId,
    const SelectionContextActionCustomizationMap &values
);
QStringList visibleSelectionContextActionOrder(
    const QStringList &order,
    const SelectionContextActionCustomizationMap &values
);
```

Add this field to `SelectionContextSettings`:

```cpp
SelectionContextActionCustomizationMap actionCustomizations =
    defaultSelectionContextActionCustomizations();
```

Use `left(24)` for non-empty display names, `left(8000)` only after the editor has rejected overlength input, `left(64)` for target language, and `original|trim` for copy mode. When `writableVocabularyScopeIds` is empty, perform syntax-only normalization: preserve a non-empty scope for the later complete-settings pass, but change `__all` to `__global`. When a non-empty writable catalog is supplied, empty/unknown/`__all` scopes become `__global`. Normalize only the five fixed IDs, then force the first ordered action visible if every fixed action is hidden.

In `model_catalog.cpp`, split the exact retired-ID mapping from provider fallback and add:

```cpp
QString normalizeExplicitModelId(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }
    const QString knownId = canonicalCurrentOrRetiredModelId(trimmed);
    if (!knownId.isEmpty()) {
        return knownId;
    }
    return trimmed;
}
```

`canonicalCurrentOrRetiredModelId()` must contain only known current bare/canonical aliases and exact retired aliases; it must return empty for unknown provider models. `normalizeExplicitModelId()` must not read secrets or call `modelOptions()`—custom IDs and unknown IDs stay as trimmed text until the execution-time injected catalog checks availability. Keep existing `normalizeModelId()` tests unchanged so unknown OpenAI/Claude IDs still follow its established provider fallback semantics outside this feature.

In JSON, start from retained `selectionContextToolbar` and retained per-action objects, overwrite only known keys, and leave unknown action IDs/keys untouched.

- [ ] **Step 4: Run GREEN and regression checks**

```powershell
Invoke-FocusedQtTest tests/config app_settings_defaults_tests.pro app_settings_defaults_tests
Invoke-FocusedQtTest tests/config app_settings_json_tests.pro app_settings_json_tests
Invoke-FocusedQtTest tests/providers model_catalog_tests.pro model_catalog_tests
git diff --check
```

- [ ] **Step 5: Commit Task 1**

```powershell
git add src/config/selection_context_action_customization.h `
  src/config/selection_context_action_customization.cpp `
  src/config/app_settings_data.h src/config/app_settings_json.cpp `
  src/providers/model_catalog.h src/providers/model_catalog.cpp `
  tests/config/app_settings_defaults_tests.cpp `
  tests/config/app_settings_defaults_tests.pro `
  tests/config/app_settings_json_tests.cpp `
  tests/config/app_settings_json_tests.pro `
  tests/providers/model_catalog_tests.cpp vocekit.pro
git commit -m "feat: persist selection action customizations"
```

### Task 2: Normalize complete settings and expose dynamic catalogs

**Files:**
- Modify: `src/config/selection_context_action_customization.h`
- Modify: `src/config/selection_context_action_customization.cpp`
- Modify: `src/ui/hub_settings_state.cpp`
- Modify: `tests/ui/hub_settings_state_tests.cpp`
- Modify: `tests/ui/hub_settings_state_tests.pro`

- [ ] **Step 1: Write RED tests for all-hidden recovery, dynamic scope, and model preservation**

```cpp
void normalizesSelectionActionCustomizationsWithCompleteSettings()
{
    AppSettingsData source;
    FunctionSettings custom;
    custom.id = QStringLiteral("custom-summarize");
    custom.name = QString::fromUtf8("摘要");
    custom.builtIn = false;
    source.functions.append(custom);

    for (const QString &id : defaultSelectionContextActionOrder()) {
        SelectionContextActionCustomization item =
            source.selectionContext.actionCustomizations.value(id);
        item.visible = false;
        source.selectionContext.actionCustomizations.insert(id, item);
    }
    SelectionContextActionCustomization save =
        source.selectionContext.actionCustomizations.value(
            selectionContextActionSave());
    save.vocabularyScopeId = custom.id;
    source.selectionContext.actionCustomizations.insert(
        selectionContextActionSave(), save);

    HubSettingsState state(source);
    const SelectionContextSettings normalized =
        state.selectionContextSettings();
    QVERIFY(normalized.actionCustomizations.value(
        normalized.actionOrder.first()).visible);
    QCOMPARE(normalized.actionCustomizations.value(
        selectionContextActionSave()).vocabularyScopeId, custom.id);
}

void deletedVocabularyScopeFallsBackWithoutChangingUnknownModel()
{
    AppSettingsData source;
    SelectionContextActionCustomization save =
        source.selectionContext.actionCustomizations.value(
            selectionContextActionSave());
    save.vocabularyScopeId = QStringLiteral("deleted-function");
    source.selectionContext.actionCustomizations.insert(
        selectionContextActionSave(), save);
    SelectionContextActionCustomization explain =
        source.selectionContext.actionCustomizations.value(
            selectionContextActionExplain());
    explain.modelId = QStringLiteral("openai:future");
    source.selectionContext.actionCustomizations.insert(
        selectionContextActionExplain(), explain);

    HubSettingsState state(source);
    const SelectionContextSettings normalized =
        state.selectionContextSettings();
    QCOMPARE(normalized.actionCustomizations.value(
        selectionContextActionSave()).vocabularyScopeId,
        QStringLiteral("__global"));
    QCOMPARE(normalized.actionCustomizations.value(
        selectionContextActionExplain()).modelId,
        QStringLiteral("openai:future"));
}
```

- [ ] **Step 2: Run RED**

```powershell
Invoke-FocusedQtTest tests/ui hub_settings_state_tests.pro hub_settings_state_tests
```

Expected RED: Hub state does not yet normalize the customization map or dynamic scopes.

- [ ] **Step 3: Implement one shared complete-settings normalizer**

Add and use these helpers instead of duplicating scope rules in Hub, UI, and runtime:

```cpp
QStringList writableSelectionContextVocabularyScopeIds(
    const AppSettingsData &settings)
{
    QStringList ids;
    ids << QStringLiteral("__global")
        << QStringLiteral("dictate")
        << QStringLiteral("translate")
        << QStringLiteral("ask");
    for (const FunctionSettings &function : settings.functions) {
        if (!function.builtIn && !function.id.trimmed().isEmpty()
            && !ids.contains(function.id.trimmed())) {
            ids.append(function.id.trimmed());
        }
    }
    return ids;
}

SelectionContextSettings normalizeSelectionContextSettings(
    const SelectionContextSettings &source,
    const AppSettingsData &completeSettings
);
```

The normalizer must call `normalizeSelectionContextActionOrder()`, normalize the action map with `writableSelectionContextVocabularyScopeIds()`, migrate explicit retired model IDs with `normalizeExplicitModelId()`, preserve unknown model IDs, and leave unrelated selection settings unchanged.

- [ ] **Step 4: Run GREEN and commit**

```powershell
Invoke-FocusedQtTest tests/ui hub_settings_state_tests.pro hub_settings_state_tests
git diff --check
git add src/config/selection_context_action_customization.* `
  src/ui/hub_settings_state.cpp tests/ui/hub_settings_state_tests.*
git commit -m "feat: normalize selection action settings"
```

### Task 3: Build the isolated per-action editor widget

**Files:**
- Create: `src/ui/selection_context_action_editor.h`
- Create: `src/ui/selection_context_action_editor.cpp`
- Create: `tests/ui/selection_context_action_editor_tests.cpp`
- Create: `tests/ui/selection_context_action_editor_tests.pro`
- Modify: `vocekit.pro`

- [ ] **Step 1: Write RED widget tests before production files exist**

The test must instantiate the real widget and find controls by stable object name:

```cpp
void commonFieldsEditTypedValue()
{
    SelectionContextActionEditor editor(selectionContextActionExplain());
    SelectionContextActionCustomization value;
    editor.setCustomization(value);

    QLineEdit *name = editor.findChild<QLineEdit *>(
        QStringLiteral("selectionActionDisplayName"));
    QCheckBox *visible = editor.findChild<QCheckBox *>(
        QStringLiteral("selectionActionVisible"));
    QVERIFY(name);
    QVERIFY(visible);
    name->setText(QString::fromUtf8("讲清楚"));
    visible->setChecked(false);
    QCOMPARE(editor.customization().displayName,
             QString::fromUtf8("讲清楚"));
    QVERIFY(!editor.customization().visible);
}

void actionSpecificControlsAreIsolated()
{
    SelectionContextActionEditor translate(selectionContextActionTranslate());
    QVERIFY(translate.findChild<QComboBox *>(
        QStringLiteral("selectionActionModel")));
    QVERIFY(translate.findChild<QPlainTextEdit *>(
        QStringLiteral("selectionActionPrompt")));
    QVERIFY(translate.findChild<QComboBox *>(
        QStringLiteral("selectionActionTargetLanguage")));
    QVERIFY(!translate.findChild<QComboBox *>(
        QStringLiteral("selectionActionCopyMode")));

    SelectionContextActionEditor save(selectionContextActionSave());
    QVERIFY(save.findChild<QComboBox *>(
        QStringLiteral("selectionActionVocabularyScope")));
    QVERIFY(!save.findChild<QComboBox *>(
        QStringLiteral("selectionActionModel")));
}

void promptLimitRejectsInsteadOfSilentlyTruncating();
void unavailableExplicitModelRemainsVisibleAndMarkedUnavailable();
void fontScalingKeepsLabelsButtonsAndEditorsAtLeastSizeHint();
```

- [ ] **Step 2: Run RED**

```powershell
Invoke-FocusedQtTest tests/ui `
  selection_context_action_editor_tests.pro `
  selection_context_action_editor_tests
```

Expected RED: qmake/make reports missing editor header and source.

- [ ] **Step 3: Implement the focused editor API**

```cpp
class SelectionContextActionEditor : public QFrame
{
public:
    struct Catalogs {
        QVector<QPair<QString, QString>> models;            // title, id
        QVector<QPair<QString, QString>> vocabularyScopes; // title, id
        QVector<QPair<QString, QString>> targetLanguages;  // title, value
    };
    struct Callbacks {
        std::function<void(const SelectionContextActionCustomization &)> changed;
        std::function<void()> restoreRequested;
        std::function<void(const QString &)> validationWarning;
    };

    explicit SelectionContextActionEditor(
        const QString &actionId,
        const Catalogs &catalogs = Catalogs(),
        const Callbacks &callbacks = Callbacks(),
        QWidget *parent = nullptr
    );
    void setCustomization(
        const SelectionContextActionCustomization &value);
    SelectionContextActionCustomization customization() const;
    void setExpanded(bool expanded);
    bool isExpanded() const;
};
```

Use a common row for name/visible/restore and create only the applicable action-specific fields. For prompt input, intercept text growth above 8000 characters, restore the previous accepted text, update a `selectionActionPromptCount` label, and call `validationWarning`; do not truncate silently. Use minimum heights derived from `sizeHint()` and zero vertical button padding compatible with the existing Qt 5.9 UI style.

- [ ] **Step 4: Run GREEN and commit**

```powershell
Invoke-FocusedQtTest tests/ui `
  selection_context_action_editor_tests.pro `
  selection_context_action_editor_tests
git diff --check
git add src/ui/selection_context_action_editor.* `
  tests/ui/selection_context_action_editor_tests.* vocekit.pro
git commit -m "feat: add selection action editor widget"
```

### Task 4: Upgrade the settings card and persistence wiring

**Files:**
- Modify: `src/ui/selection_context_settings_card.h`
- Modify: `src/ui/selection_context_settings_card.cpp`
- Modify: `src/ui/basic_settings_section.h`
- Modify: `src/ui/basic_settings_section.cpp`
- Modify: `src/ui/settings_panel.cpp`
- Modify: `tests/ui/selection_context_settings_card_tests.cpp`
- Modify: `tests/ui/selection_context_settings_card_tests.pro`
- Modify: `tests/ui/basic_settings_section_tests.cpp`
- Modify: `tests/ui/basic_settings_section_tests.pro`

- [ ] **Step 1: Write RED behavior tests for expansion, last-visible guard, reset, and rollback**

```cpp
void onlyOneActionEditorIsExpandedAtATime();

void refusesToHideLastVisibleAction()
{
    SelectionContextSettings settings;
    for (const QString &id : settings.actionOrder) {
        SelectionContextActionCustomization item =
            settings.actionCustomizations.value(id);
        item.visible = id == selectionContextActionCopy();
        settings.actionCustomizations.insert(id, item);
    }
    QString warning;
    SelectionContextSettingsCard::Callbacks callbacks;
    callbacks.validationWarning = [&](const QString &text) {
        warning = text;
    };
    SelectionContextSettingsCard card(settings, callbacks);
    SelectionContextActionEditor *copyEditor =
        card.findChild<SelectionContextActionEditor *>(
            QStringLiteral("selectionActionEditor_copy"));
    QVERIFY(copyEditor);
    copyEditor->findChild<QCheckBox *>(
        QStringLiteral("selectionActionVisible"))->setChecked(false);
    QVERIFY(card.settings().actionCustomizations.value(
        selectionContextActionCopy()).visible);
    QVERIFY(warning.contains(QString::fromUtf8("至少保留一个")));
}

void restoreOneDoesNotChangeOtherActions();
void restoreAllKeepsGlobalToolbarSettings();
void cancellingRestoreAllChangesNothing();
void failedSettingsSaveRefreshesEveryEditorFromPersistedSnapshot();
void dragOrderStillContainsEveryBuiltInIdExactlyOnce();
```

Locate row/editor widgets through object names and item data; this plan does not require a production test-only accessor.

- [ ] **Step 2: Run RED**

```powershell
Invoke-FocusedQtTest tests/ui `
  selection_context_settings_card_tests.pro `
  selection_context_settings_card_tests
Invoke-FocusedQtTest tests/ui `
  basic_settings_section_tests.pro basic_settings_section_tests
```

Expected RED: no per-action editors, no last-visible guard, and no catalog wiring.

- [ ] **Step 3: Implement list/editor coordination and real catalogs**

Add providers to `BasicSettingsSection::Callbacks`:

```cpp
std::function<QVector<QPair<QString, QString>>()> modelCatalogProvider;
std::function<QVector<QPair<QString, QString>>()> vocabularyScopeCatalogProvider;
std::function<bool()> confirmRestoreAllSelectionActions;
std::function<void(const QString &)> selectionActionValidationWarning;
```

`SettingsPanel` must build model pairs from `modelOptions()` and scope pairs from built-in writable scopes plus non-built-in functions, excluding `__all`. `SelectionContextSettingsCard` must:

```cpp
void setCatalogs(const SelectionContextActionEditor::Catalogs &catalogs);
void rebuildActionEditors();
void setExpandedAction(const QString &actionId);
void restoreActionDefaults(const QString &actionId);
void restoreAllActionDefaults();
bool applyCustomization(
    const QString &actionId,
    const SelectionContextActionCustomization &value
);
```

Keep `actionOrder` as the only ordering source. Store fixed action ID in each `QListWidgetItem::UserRole`; never derive identity from display text. Give every editor the stable object name `selectionActionEditor_<fixed-id>`. After any accepted change, emit one full `SelectionContextSettings` snapshot through `settingsChanged`. “恢复全部” must call `confirmRestoreAllSelectionActions` and change nothing when it returns false. On save failure, existing `SettingsPanel::saveAndRefresh()` reloads the persisted snapshot; ensure `BasicSettingsSection::refreshFromSettings()` calls `setSettings()` so every expanded and collapsed editor rolls back together.

- [ ] **Step 4: Run GREEN, then existing settings regressions**

```powershell
Invoke-FocusedQtTest tests/ui `
  selection_context_settings_card_tests.pro `
  selection_context_settings_card_tests
Invoke-FocusedQtTest tests/ui `
  basic_settings_section_tests.pro basic_settings_section_tests
Invoke-FocusedQtTest tests/ui `
  settings_panel_access_factory_tests.pro `
  settings_panel_access_factory_tests
git diff --check
```

- [ ] **Step 5: Commit Task 4**

```powershell
git add src/ui/selection_context_settings_card.* `
  src/ui/basic_settings_section.* src/ui/settings_panel.cpp `
  tests/ui/selection_context_settings_card_tests.* `
  tests/ui/basic_settings_section_tests.*
git commit -m "feat: edit selection actions in settings"
```

### Task 5: Render visibility and custom names without changing action identity

**Files:**
- Modify: `src/ui/selection_context_toolbar.h`
- Modify: `src/ui/selection_context_toolbar.cpp`
- Modify: `src/app/selection_context_feature.cpp`
- Modify: `tests/ui/selection_context_toolbar_tests.cpp`
- Modify: `tests/ui/selection_context_toolbar_tests.pro`
- Modify: `tests/app/selection_context_feature_tests.cpp`
- Modify: `tests/app/selection_context_feature_tests.pro`

- [ ] **Step 1: Write RED toolbar and feature tests**

```cpp
void customPresentationFiltersAndRenamesWithoutChangingIds()
{
    SelectionContextToolbar toolbar;
    SelectionContextSettings settings;
    SelectionContextActionCustomization search =
        settings.actionCustomizations.value(selectionContextActionAiSearch());
    search.displayName = QString::fromUtf8("问 AI");
    settings.actionCustomizations.insert(selectionContextActionAiSearch(), search);
    SelectionContextActionCustomization save =
        settings.actionCustomizations.value(selectionContextActionSave());
    save.visible = false;
    settings.actionCustomizations.insert(selectionContextActionSave(), save);

    QString clicked;
    toolbar.setActionTriggeredCallback([&](const QString &id) { clicked = id; });
    toolbar.setActionPresentation(settings.actionOrder,
                                  settings.actionCustomizations);
    QCOMPARE(toolbar.findChild<QToolButton *>(
        QStringLiteral("selectionAction_ai-search"))->text(),
        QString::fromUtf8("问 AI"));
    QVERIFY(!toolbar.findChild<QToolButton *>(
        QStringLiteral("selectionAction_save")));
    QTest::mouseClick(toolbar.findChild<QToolButton *>(
        QStringLiteral("selectionAction_ai-search")), Qt::LeftButton);
    QCOMPARE(clicked, selectionContextActionAiSearch());
}

void hiddenBuiltInsDoNotAppearInOverflowOrMoreMenu();
void settingsRefreshChangesTheNextToolbarPresentationWithoutRestart();
```

- [ ] **Step 2: Run RED**

```powershell
Invoke-FocusedQtTest tests/ui `
  selection_context_toolbar_tests.pro selection_context_toolbar_tests
Invoke-FocusedQtTest tests/app `
  selection_context_feature_tests.pro selection_context_feature_tests
```

- [ ] **Step 3: Implement one presentation setter**

```cpp
void SelectionContextToolbar::setActionPresentation(
    const QStringList &order,
    const SelectionContextActionCustomizationMap &customizations)
{
    m_actionOrder = visibleSelectionContextActionOrder(order, customizations);
    m_actionTitles.clear();
    for (const QString &id : m_actionOrder) {
        m_actionTitles.insert(
            id,
            selectionContextActionDisplayName(id, customizations));
    }
    rebuildActionButtons();
}
```

Preserve `setActionOrder()` as a compatibility wrapper that passes default customizations until all callers are migrated. In every button/menu lambda, capture the stable ID by value. `SelectionContextFeature::applyToolbarSettings()` must pass the current normalized order and customization map; classic custom functions remain in the existing “更多” catalog and are not merged into this map.

- [ ] **Step 4: Run GREEN, regressions, and commit**

```powershell
Invoke-FocusedQtTest tests/ui `
  selection_context_toolbar_tests.pro selection_context_toolbar_tests
Invoke-FocusedQtTest tests/app `
  selection_context_feature_tests.pro selection_context_feature_tests
git diff --check
git add src/ui/selection_context_toolbar.* src/app/selection_context_feature.cpp `
  tests/ui/selection_context_toolbar_tests.* `
  tests/app/selection_context_feature_tests.*
git commit -m "feat: present customized selection actions"
```

### Task 6: Apply independent AI model, prompt, and target language

**Files:**
- Modify: `src/tasks/selection_context_model_request.h`
- Modify: `src/tasks/selection_context_model_request.cpp`
- Modify: `src/controllers/selection_context_action_controller.h`
- Modify: `src/controllers/selection_context_action_controller.cpp`
- Modify: `src/app/selection_context_feature.cpp`
- Modify: `tests/tasks/selection_context_model_request_tests.cpp`
- Modify: `tests/tasks/selection_context_model_request_tests.pro`
- Modify: `tests/controllers/selection_context_action_controller_tests.cpp`
- Modify: `tests/controllers/selection_context_action_controller_tests.pro`

- [ ] **Step 1: Write RED request tests with an injected model catalog**

```cpp
void eachAiActionUsesOnlyItsOwnCustomization()
{
    SelectionContextModelRequestInput input;
    input.selectedText = QString::fromUtf8("原文");
    input.modelOptions = QVector<ModelOption>()
        << ModelOption{QStringLiteral("openai:gpt-5.6-sol"),
                       QStringLiteral("GPT-5.6 Sol"), QStringLiteral("OpenAI")};
    SelectionContextActionCustomization explain;
    explain.modelId = QStringLiteral("openai:gpt-5.6-sol");
    explain.promptOverride = QString::fromUtf8("分三层解释");
    input.settings.selectionContext.actionCustomizations.insert(
        selectionContextActionExplain(), explain);
    input.actionId = selectionContextActionExplain();

    const SelectionContextModelRequest built =
        buildSelectionContextModelRequest(input);
    QVERIFY(built.valid);
    QCOMPARE(built.modelRequest.modelId,
             QStringLiteral("openai:gpt-5.6-sol"));
    QVERIFY(built.modelRequest.userPrompt.contains(
        QString::fromUtf8("分三层解释")));
}

void translateOverrideDoesNotModifyGlobalTargetLanguage();
void blankOverrideUsesExistingBuiltInPrompt();
void unavailableExplicitModelReturnsSelectionModelUnavailable();
void aiSearchCustomPromptCannotRemoveNonSearchWarningOrSafetySuffix();
void followUpKeepsSelectedTextWrapperAndCustomInstruction();
```

- [ ] **Step 2: Run RED**

```powershell
Invoke-FocusedQtTest tests/tasks `
  selection_context_model_request_tests.pro `
  selection_context_model_request_tests
Invoke-FocusedQtTest tests/controllers `
  selection_context_action_controller_tests.pro `
  selection_context_action_controller_tests
```

- [ ] **Step 3: Implement request snapshot semantics**

Extend input and controller access with injected model options:

```cpp
struct SelectionContextModelRequestInput
{
    QString actionId;
    QString selectedText;
    QString previousAnswer;
    QString followUpQuestion;
    AppSettingsData settings;
    PromptRuntimeSnapshot prompts;
    QVector<ModelOption> modelOptions;
};

std::function<QVector<ModelOption>()> modelOptionsSnapshot;
```

Wire production options once in `SelectionContextFeature`:

```cpp
actionAccess.modelOptionsSnapshot = []() {
    return modelOptions();
};
```

For a non-empty explicit model, call `normalizeExplicitModelId()`, then require an exact ID match in the injected options. Return `selection.action_model_unavailable` without invoking the runner when unavailable. Empty model continues to use the corresponding built-in function configuration (`ask` for AI Search/Explain, `translate` for Translate).

Treat `promptOverride` as the action instruction block: it replaces only the built-in action-specific sentence. The application still generates selected-text framing, previous answer, follow-up question, target-language line, privacy/long-text gates, and the immutable AI Search suffix. Do not place prompt text or selected text in diagnostic summaries.

- [ ] **Step 4: Run GREEN, existing runner tests, and commit**

```powershell
Invoke-FocusedQtTest tests/tasks `
  selection_context_model_request_tests.pro `
  selection_context_model_request_tests
Invoke-FocusedQtTest tests/controllers `
  selection_context_action_controller_tests.pro `
  selection_context_action_controller_tests
Invoke-FocusedQtTest tests/tasks `
  selection_context_model_runner_tests.pro `
  selection_context_model_runner_tests
git diff --check
git add src/tasks/selection_context_model_request.* `
  src/controllers/selection_context_action_controller.* `
  src/app/selection_context_feature.cpp `
  tests/tasks/selection_context_model_request_tests.* `
  tests/controllers/selection_context_action_controller_tests.*
git commit -m "feat: customize selection AI actions"
```

### Task 7: Implement local copy mode and vocabulary scope

**Files:**
- Modify: `src/controllers/selection_context_action_controller.h`
- Modify: `src/controllers/selection_context_action_controller.cpp`
- Modify: `src/app/selection_context_feature.h`
- Modify: `src/app/selection_context_feature.cpp`
- Modify: `src/app/vocekit_application_runtime.cpp`
- Modify: `tests/controllers/selection_context_action_controller_tests.cpp`
- Modify: `tests/app/selection_context_feature_tests.cpp`

- [ ] **Step 1: Write RED tests proving both actions remain local**

```cpp
void copyTrimModeUsesLocalClipboardAndNeverRunsModel()
{
    Harness h;
    h.settings.selectionContext.actionCustomizations[
        selectionContextActionCopy()].copyMode = QStringLiteral("trim");
    h.controller.setSelection(snapshot(QStringLiteral("  text  ")));
    h.controller.triggerAction(selectionContextActionCopy());
    QCOMPARE(h.copiedTexts, QStringList() << QStringLiteral("text"));
    QCOMPARE(h.modelRunCount, 0);
    QCOMPARE(h.networkConsentCount, 0);
}

void saveOpensLocalEditorOnceWithConfiguredScope()
{
    Harness h;
    h.settings.selectionContext.actionCustomizations[
        selectionContextActionSave()].vocabularyScopeId =
            QStringLiteral("translate");
    h.controller.setSelection(snapshot(QString::fromUtf8("术语")));
    h.controller.triggerAction(selectionContextActionSave());
    QCOMPARE(h.vocabularyEditorCalls.size(), 1);
    QCOMPARE(h.vocabularyEditorCalls.first().scopeId,
             QStringLiteral("translate"));
    QCOMPARE(h.modelRunCount, 0);
    QCOMPARE(h.networkConsentCount, 0);
}

void deletedScopeIsNormalizedToGlobalBeforeOpeningEditor();
void missingVocabularyEditorReportsLocalFailureWithoutModelFallback();
void localCallbackDestroyingControllerDoesNotUseAfterFree();
```

- [ ] **Step 2: Run RED**

```powershell
Invoke-FocusedQtTest tests/controllers `
  selection_context_action_controller_tests.pro `
  selection_context_action_controller_tests
Invoke-FocusedQtTest tests/app `
  selection_context_feature_tests.pro selection_context_feature_tests
```

- [ ] **Step 3: Change the local callback contract and dispatch from a settings snapshot**

```cpp
struct SelectionContextActionAccess
{
    std::function<bool(const QString &)> copyText;
    std::function<void(const QString &, const QString &)> openVocabularyEditor;
    std::function<AppSettingsData()> settingsSnapshot;
    // existing validation, model, render, logging, and consent callbacks remain
};
```

Read one settings snapshot before both local branches. `copy` applies `trimmed()` only for `copyMode == "trim"`; `original` passes the exact selected string. `save` obtains the already normalized `vocabularyScopeId` and calls `openVocabularyEditor(text, scopeId)` once. Keep `QPointer` guards after callbacks exactly as in the existing local branches.

Change the feature/runtime bridge to:

```cpp
selectionFeatureAccess.openVocabularyEditor =
    [&voice](const QString &text, const QString &scopeId) {
        voice.addVocabularyLocallyForFlow(text, scopeId, QString());
    };
```

Do not call `addVocabularyForFlow()`: that route can use AI depending on vocabulary settings and would violate the local-only contract.

- [ ] **Step 4: Run GREEN and commit**

```powershell
Invoke-FocusedQtTest tests/controllers `
  selection_context_action_controller_tests.pro `
  selection_context_action_controller_tests
Invoke-FocusedQtTest tests/app `
  selection_context_feature_tests.pro selection_context_feature_tests
git diff --check
git add src/controllers/selection_context_action_controller.* `
  src/app/selection_context_feature.* `
  src/app/vocekit_application_runtime.cpp `
  tests/controllers/selection_context_action_controller_tests.cpp `
  tests/app/selection_context_feature_tests.cpp
git commit -m "feat: customize local selection actions"
```

### Task 8: Run visual, integration, release, and full-suite gates

**Files:**
- Modify: `tests/ui/selection_context_settings_card_tests.cpp`
- Modify: `docs/AI_PROJECT_GUIDE.md`
- Modify: `docs/TESTING.md`
- Verify: all files changed by Tasks 1-7

- [ ] **Step 1: Add native Windows visual states before claiming UI completion**

Extend the real settings-card test to render these states at 100%, 125%, and 150% font scaling:

```cpp
struct VisualCase {
    int percent;
    QString expandedActionId;
    QString displayName;
    QString fileName;
};
const QVector<VisualCase> cases = {
    {100, selectionContextActionAiSearch(),
     QString::fromUtf8("AI 搜索：分析并回答"),
     QStringLiteral("selection-actions-ai-100.png")},
    {125, selectionContextActionTranslate(),
     QString::fromUtf8("翻译成我指定的目标语言"),
     QStringLiteral("selection-actions-translate-125.png")},
    {150, selectionContextActionSave(),
     QString::fromUtf8("保存到词库并确认作用范围"),
     QStringLiteral("selection-actions-save-150.png")}
};
```

For every visible `QLabel`, `QAbstractButton`, `QComboBox`, `QLineEdit`, and prompt editor, assert actual height is at least `sizeHint().height()` after layout activation. Save PNGs only under an ignored `build-selection-action-customization/visual` directory set by `VOCEKIT_VISUAL_OUTPUT_DIR`. On Windows, fail if the chosen CJK font has no Chinese glyphs or screenshots contain no dark text pixels; do not accept an offscreen empty-font pass.

- [ ] **Step 2: Build and run the complete focused matrix**

```powershell
Invoke-FocusedQtTest tests/config app_settings_defaults_tests.pro app_settings_defaults_tests
Invoke-FocusedQtTest tests/config app_settings_json_tests.pro app_settings_json_tests
Invoke-FocusedQtTest tests/providers model_catalog_tests.pro model_catalog_tests
Invoke-FocusedQtTest tests/ui hub_settings_state_tests.pro hub_settings_state_tests
Invoke-FocusedQtTest tests/ui selection_context_action_editor_tests.pro selection_context_action_editor_tests
Invoke-FocusedQtTest tests/ui selection_context_settings_card_tests.pro selection_context_settings_card_tests
Invoke-FocusedQtTest tests/ui basic_settings_section_tests.pro basic_settings_section_tests
Invoke-FocusedQtTest tests/ui selection_context_toolbar_tests.pro selection_context_toolbar_tests
Invoke-FocusedQtTest tests/tasks selection_context_model_request_tests.pro selection_context_model_request_tests
Invoke-FocusedQtTest tests/controllers selection_context_action_controller_tests.pro selection_context_action_controller_tests
Invoke-FocusedQtTest tests/app selection_context_feature_tests.pro selection_context_feature_tests
```

Expected: every executable exits `0`, `0 failed`, `0 skipped`.

- [ ] **Step 3: Inspect the three native PNGs**

Open each PNG with the local image viewer and verify Chinese text, the long display name, prompt counter, model/target/scope combos, switches, reset buttons, and scroll boundaries are visible without clipping or overlap. Record exact dimensions and SHA-256 hashes in the final implementation report.

- [ ] **Step 4: Build the production target without test macros**

```powershell
$qt = 'D:\QQQQQT0001\5.9\mingw53_32\bin'
$mingw = 'D:\QQQQQT0001\Tools\mingw530_32\bin'
$env:PATH = "$qt;$mingw;$env:PATH"
$build = 'build-selection-action-customization'
& "$qt\qmake.exe" vocekit.pro -o Makefile.codex.selection-actions `
  -spec win32-g++ CONFIG+=release `
  "TARGET=vocekit_selection_actions_verify" `
  "OBJECTS_DIR=$build/obj" "MOC_DIR=$build/moc" `
  "RCC_DIR=$build/rcc" "UI_DIR=$build/ui" "DESTDIR=$build/release"
if ($LASTEXITCODE -ne 0) { throw 'main qmake failed' }
& "$mingw\mingw32-make.exe" -f Makefile.codex.selection-actions -j2
if ($LASTEXITCODE -ne 0) { throw 'main release build failed' }
Get-FileHash "$build\release\vocekit_selection_actions_verify.exe" -Algorithm SHA256
```

- [ ] **Step 5: Run full regression and privacy/static checks**

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File scripts/run-all-tests.ps1 -Configuration release
rg -n "promptOverride|selectedText|displayName" src -g "*log*" -g "*.cpp"
git diff --check
git status --short
Get-Process | Where-Object {
    $_.ProcessName -like '*selection_context*' -or
    $_.ProcessName -like '*vocekit_selection_actions*'
}
```

Expected full-suite summary: `Failed=0`, `Skipped=0`, `InfrastructureFailures=0`. Manually inspect every logging match and confirm no selected text, user prompt, custom display name, clipboard text, or vocabulary body is logged. Only fixed action ID, lengths, status, and safe error codes are permitted. No focused-test or verification executable may remain running.

- [ ] **Step 6: Update docs and commit the final gate**

Document the software path “设置 → 常用设置 → 选中文字工具条 → 工具条功能”, the five editable field sets, local-only Save/Copy behavior, AI Search non-web disclaimer, migration behavior, and the visual test matrix.

```powershell
git add tests/ui/selection_context_settings_card_tests.cpp `
  docs/AI_PROJECT_GUIDE.md docs/TESTING.md
git commit -m "test: verify customizable selection actions"
git log -8 --oneline
git status --short
```

Do not delete or stage pre-existing unrelated untracked files. Do not overwrite a running `release/vocekit.exe`; the verification target uses a separate executable name and directory.

## Spec coverage index

- Design §§2-3 goals/non-goals: Tasks 1-7 preserve fixed IDs, local-only actions, classic custom-function isolation, non-web AI Search, and existing result-card behavior.
- Design §4 UI and every per-action field: Tasks 3-4; native long-Chinese and scaling proof in Task 8.
- Design §5 persistence, validation, legacy migration, unknown JSON, model retirement, and last-visible recovery: Tasks 1-2.
- Design §§6-7 component boundaries and snapshot data flow: Tasks 3-7.
- Design §8 errors and privacy: Tasks 2, 4, 6, 7, and Task 8 log audit.
- Design §§9-10 test matrix and acceptance: every task supplies RED/GREEN evidence; Task 8 supplies the complete focused matrix, native screenshots, production build, full suite, privacy audit, hashes, and residual-process check.
- Design §11 recommended sequence: Tasks 1-8 follow the same dependency order.
