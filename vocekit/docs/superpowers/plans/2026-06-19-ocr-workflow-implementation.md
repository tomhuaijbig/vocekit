# vocekit OCR Workflow Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add non-blocking single-image OCR with RapidOCR as the primary local engine, Windows OCR as automatic fallback, optional custom cloud OCR and optional vision-model recognition.

**Architecture:** Keep OCR outside the existing `voiceassistant.cpp` monolith. The Qt process owns validation, scheduling, UI and cloud requests; local engines run in isolated helper processes that exchange one-line JSON messages. `OcrManager` permits one active request, applies the selected engine policy and converts every engine failure into an `OcrResult` instead of allowing exceptions or process failures to reach the UI.

**Tech Stack:** Qt 5.9 Widgets/Network/Concurrent, QProcess JSON protocol, RapidAI/RapidOcrOnnx pinned at commit `abd498c13a6dbe5f3b3c0d421d72e01bb3e6ee6d`, ONNX Runtime, Windows 10/11 C++/WinRT OCR, MSVC helper builds, existing MinGW Qt application.

---

## File Map

- Create `src/ocr/ocr_types.h`: shared engine, request, result and error types.
- Create `src/ocr/ocr_helper_process.h/.cpp`: one-line JSON helper protocol and timeout handling.
- Create `src/ocr/ocr_cloud_client.h/.cpp`: custom HTTPS OCR request.
- Create `src/ocr/ocr_manager.h/.cpp`: one-task scheduler and fallback policy.
- Create `src/modules/hub_ocr_page.inc`: OCR page widgets and workflow callbacks.
- Create `helpers/windows_ocr/main.cpp` and `helpers/windows_ocr/windows_ocr.vcxproj`: Windows OCR helper.
- Create `helpers/rapidocr/main.cpp` and `helpers/rapidocr/rapidocr_helper.vcxproj`: RapidOcrOnnx wrapper.
- Create `scripts/fetch-rapidocr.ps1`: pinned third-party source and model acquisition.
- Create `tests/ocr/ocr_core_tests.cpp` and `tests/ocr/ocr_core_tests.pro`: deterministic protocol, validation and fallback tests.
- Modify `src/voiceassistant.cpp`: navigation, Hub integration, history and AI handoff.
- Modify `src/modules/settings_panel.inc`: OCR settings and custom endpoint.
- Modify `src/modules/api_client.inc`: vision-model input.
- Modify `src/modules/hub_history_page.inc`: OCR history fields.
- Modify `vocekit.pro`, packaging scripts, FAQ and documentation.

### Task 1: OCR Core Types and Image Validation

**Files:**
- Create: `src/ocr/ocr_types.h`
- Create: `tests/ocr/ocr_core_tests.cpp`
- Create: `tests/ocr/ocr_core_tests.pro`
- Modify: `vocekit.pro`

- [ ] **Step 1: Add a failing validation test**

```cpp
void OcrCoreTests::rejectsOversizedFile()
{
    OcrRequest request;
    request.imagePath = QFINDTESTDATA("fixtures/oversized.bin");
    QString error;
    QVERIFY(!validateOcrImage(request.imagePath, &error));
    QVERIFY(error.contains(QStringLiteral("25 MB")));
}
```

- [ ] **Step 2: Run the test and verify it fails**

Run:

```powershell
qmake tests\ocr\ocr_core_tests.pro -spec win32-g++
mingw32-make -f Makefile.Debug
debug\ocr_core_tests.exe
```

Expected: compilation fails because `OcrRequest` and `validateOcrImage` do not exist.

- [ ] **Step 3: Implement exact shared types**

```cpp
enum class OcrEngine {
    Automatic,
    RapidOcr,
    WindowsOcr,
    CustomCloud,
    VisionModel
};

struct OcrRequest {
    QString requestId;
    QString imagePath;
    QStringList languages;
    OcrEngine engine = OcrEngine::Automatic;
};

struct OcrResult {
    bool ok = false;
    OcrEngine engine = OcrEngine::Automatic;
    QString text;
    QString errorCode;
    QString errorMessage;
    qint64 elapsedMs = -1;
    bool usedFallback = false;
};
```

`validateOcrImage()` must accept PNG/JPG/JPEG/BMP/WebP, reject files over 25 MB, call `QImageReader::canRead()`, and return Chinese error text.

- [ ] **Step 4: Add tests for valid PNG, unsupported extension and corrupt image**

Expected assertions:

```cpp
QVERIFY(validateOcrImage(validPng, &error));
QVERIFY(!validateOcrImage(unsupportedGif, &error));
QVERIFY(!validateOcrImage(corruptPng, &error));
```

- [ ] **Step 5: Run the test and main build**

Expected: tests pass and Qt debug build succeeds.

- [ ] **Step 6: Commit**

```bash
git add src/ocr/ocr_types.h tests/ocr vocekit.pro
git commit -m "feat: add OCR request and validation types"
```

### Task 2: Local Helper JSON Protocol

**Files:**
- Create: `src/ocr/ocr_helper_process.h`
- Create: `src/ocr/ocr_helper_process.cpp`
- Modify: `tests/ocr/ocr_core_tests.cpp`
- Modify: `vocekit.pro`

- [ ] **Step 1: Add failing parser tests**

```cpp
void OcrCoreTests::parsesHelperSuccess()
{
    const QByteArray json = R"({"requestId":"a","ok":true,"text":"中文 ABC","elapsedMs":42})";
    const OcrResult result = parseOcrHelperResponse(json, OcrEngine::RapidOcr);
    QVERIFY(result.ok);
    QCOMPARE(result.text, QString::fromUtf8("中文 ABC"));
}
```

Also test malformed JSON and an explicit `MODEL_MISSING` response.

- [ ] **Step 2: Implement `OcrHelperProcess`**

Public API:

```cpp
class OcrHelperProcess : public QObject {
public:
    explicit OcrHelperProcess(QObject *parent = nullptr);
    OcrResult recognize(
        const QString &program,
        const QStringList &arguments,
        const OcrRequest &request,
        int timeoutMs
    );
    void stop();
};
```

Use `QProcess`, write compact JSON plus `\n`, close the write channel, wait in the worker thread only, cap stdout/stderr at 1 MB, terminate then kill on timeout, and never throw.

- [ ] **Step 3: Add a fake helper executable test**

Create the test helper using the same Qt test project. It must support `success`, `failure`, `malformed` and `timeout` command-line modes.

- [ ] **Step 4: Run protocol tests**

Expected: all four modes return stable `OcrResult` values without hanging.

- [ ] **Step 5: Commit**

```bash
git add src/ocr/ocr_helper_process.* tests/ocr vocekit.pro
git commit -m "feat: add isolated OCR helper protocol"
```

### Task 3: Windows OCR Helper

**Files:**
- Create: `helpers/windows_ocr/main.cpp`
- Create: `helpers/windows_ocr/windows_ocr.vcxproj`
- Create: `scripts/build-ocr-helpers.ps1`
- Modify: `tests/ocr/ocr_core_tests.cpp`

- [ ] **Step 1: Implement the C++/WinRT helper**

The helper must:

```cpp
winrt::init_apartment(winrt::apartment_type::multi_threaded);
auto file = StorageFile::GetFileFromPathAsync(path).get();
auto stream = file.OpenAsync(FileAccessMode::Read).get();
auto decoder = BitmapDecoder::CreateAsync(stream).get();
auto bitmap = decoder.GetSoftwareBitmapAsync().get();
auto engine = OcrEngine::TryCreateFromLanguage(Language(L"zh-Hans"));
auto result = engine.RecognizeAsync(bitmap).get();
```

Return one compact UTF-8 JSON line. If `zh-Hans` is unavailable, try `en`; if both fail return `LANGUAGE_NOT_INSTALLED`.

- [ ] **Step 2: Build with the installed Windows SDK**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Msbuild\Current\Bin\MSBuild.exe' helpers\windows_ocr\windows_ocr.vcxproj /p:Configuration=Release /p:Platform=x64
```

Expected: `helpers\bin\vocekit-windows-ocr.exe`.

- [ ] **Step 3: Run a generated bilingual fixture**

Use a test-created PNG containing `中文 OCR 123`. Verify the helper exits normally and returns non-empty text. Do not assert exact punctuation because Windows language packs differ.

- [ ] **Step 4: Verify missing-language handling**

Call the helper with an invalid language tag and expect `LANGUAGE_NOT_INSTALLED`.

- [ ] **Step 5: Commit**

```bash
git add helpers/windows_ocr scripts/build-ocr-helpers.ps1 tests/ocr
git commit -m "feat: add Windows OCR fallback helper"
```

### Task 4: RapidOCR Helper and Pinned Dependency

**Files:**
- Create: `helpers/rapidocr/main.cpp`
- Create: `helpers/rapidocr/rapidocr_helper.vcxproj`
- Create: `scripts/fetch-rapidocr.ps1`
- Create: `third_party/rapidocr/README.md`
- Modify: `.gitignore`
- Modify: `scripts/build-ocr-helpers.ps1`

- [ ] **Step 1: Add a pinned fetch script**

The script must download:

```text
https://github.com/RapidAI/RapidOcrOnnx/archive/abd498c13a6dbe5f3b3c0d421d72e01bb3e6ee6d.zip
```

It must place source under `third_party/rapidocr/src`, verify SHA-256 stored in the script after the first approved download, and refuse a mismatched archive.

- [ ] **Step 2: Wrap RapidOcrOnnx**

The helper reads the same JSON protocol, loads det/cls/rec models once, recognizes one image, joins OCR lines by `\n`, and stays alive for further requests until stdin closes or it receives:

```json
{"action":"shutdown"}
```

- [ ] **Step 3: Report deterministic dependency errors**

Required codes:

```text
HELPER_MISSING
MODEL_MISSING
RUNTIME_MISSING
MODEL_LOAD_FAILED
IMAGE_DECODE_FAILED
RECOGNITION_FAILED
```

- [ ] **Step 4: Build and run the bilingual fixture**

Expected: helper returns `ok=true`, non-empty text and `elapsedMs`.

- [ ] **Step 5: Rename one model and verify graceful failure**

Expected: `MODEL_MISSING`; no crash and no Windows error dialog.

- [ ] **Step 6: Commit**

```bash
git add helpers/rapidocr scripts/fetch-rapidocr.ps1 scripts/build-ocr-helpers.ps1 third_party/rapidocr/README.md .gitignore
git commit -m "feat: add RapidOCR primary helper"
```

### Task 5: OCR Manager, Lazy Loading and Fallback

**Files:**
- Create: `src/ocr/ocr_manager.h`
- Create: `src/ocr/ocr_manager.cpp`
- Modify: `tests/ocr/ocr_core_tests.cpp`
- Modify: `vocekit.pro`

- [ ] **Step 1: Add a failing fallback test**

```cpp
void OcrCoreTests::automaticFallsBackToWindows()
{
    FakeEngine rapid(OcrResult{false, OcrEngine::RapidOcr, {}, "MODEL_MISSING", "missing"});
    FakeEngine windows(OcrResult{true, OcrEngine::WindowsOcr, "fallback text"});
    OcrResult result = runAutomaticFallback(rapid, windows, request);
    QVERIFY(result.ok);
    QVERIFY(result.usedFallback);
    QCOMPARE(result.engine, OcrEngine::WindowsOcr);
}
```

- [ ] **Step 2: Implement one-task scheduling**

Public API:

```cpp
bool isBusy() const;
void recognize(const OcrRequest &request);
void cancel();
std::function<void(const QString &)> statusCallback;
std::function<void(const OcrResult &)> finishedCallback;
```

Run work with `QtConcurrent::run`; deliver results to the GUI thread with `QFutureWatcher`.

- [ ] **Step 3: Implement automatic fallback**

Only these RapidOCR failures trigger Windows OCR:

```text
HELPER_MISSING
MODEL_MISSING
RUNTIME_MISSING
MODEL_LOAD_FAILED
IMAGE_DECODE_FAILED
RECOGNITION_FAILED
TIMEOUT
INVALID_RESPONSE
```

If both fail, combine messages as:

```text
RapidOCR：...
Windows OCR：...
```

- [ ] **Step 4: Implement idle release**

After each RapidOCR task, start a 5-minute single-shot timer. A new task restarts it. Timer expiry sends `shutdown`, waits 2 seconds, then terminates the helper.

- [ ] **Step 5: Run busy, cancel, fallback and idle tests**

Expected: no second task starts, cancellation returns `CANCELLED`, fallback preserves both diagnostics.

- [ ] **Step 6: Commit**

```bash
git add src/ocr/ocr_manager.* tests/ocr vocekit.pro
git commit -m "feat: add OCR scheduling and fallback"
```

### Task 6: Custom Cloud OCR and Vision Recognition

**Files:**
- Create: `src/ocr/ocr_cloud_client.h`
- Create: `src/ocr/ocr_cloud_client.cpp`
- Modify: `src/modules/api_client.inc`
- Modify: `src/voiceassistant.cpp`
- Modify: `tests/ocr/ocr_core_tests.cpp`

- [ ] **Step 1: Add response extraction tests**

Test `text`, `result`, `content`, `data.text`, `data.result`, malformed JSON and HTTP authentication errors.

- [ ] **Step 2: Implement custom cloud request**

Send:

```json
{
  "image": "<base64>",
  "mimeType": "image/png",
  "languages": ["zh-Hans", "en"],
  "model": "configured model"
}
```

Use `Authorization: Bearer` only when the key is non-empty. Enforce configured timeout and 25 MB input limit.

- [ ] **Step 3: Extend model profiles**

Add:

```cpp
bool supportsImages = false;
```

Persist it in `custom_models` JSON and expose it in the custom model editor.

- [ ] **Step 4: Add `ApiClient::visionTextRecognition`**

It must reject models without image support before network transmission. For OpenAI-compatible providers, send an image URL data object and a text instruction. For unsupported built-in providers return `VISION_NOT_SUPPORTED`.

- [ ] **Step 5: Verify privacy logging**

Search runtime logs after a fixture request. They may contain file size and service name, but must not contain Base64, API keys or recognized text.

- [ ] **Step 6: Commit**

```bash
git add src/ocr/ocr_cloud_client.* src/modules/api_client.inc src/voiceassistant.cpp tests/ocr
git commit -m "feat: add optional cloud and vision OCR"
```

### Task 7: OCR Settings and Interface Self-Test

**Files:**
- Modify: `src/voiceassistant.cpp`
- Modify: `src/modules/settings_panel.inc`
- Modify: `config/settings.example.json`
- Modify: `config/secrets.example.json`

- [ ] **Step 1: Add settings defaults**

```json
{
  "ocrEngine": "automatic",
  "ocrLanguages": ["zh-Hans", "en"],
  "ocrTimeoutMs": 45000,
  "ocrRapidIdleReleaseSeconds": 300,
  "ocrCustomModel": ""
}
```

Secrets:

```json
{
  "custom_ocr_url": "",
  "custom_ocr_api_key": "",
  "custom_ocr_model": ""
}
```

- [ ] **Step 2: Add the OCR interface section**

The card must show the selected engine only. Cloud fields appear only for custom cloud OCR; vision model selection appears only for AI image recognition.

- [ ] **Step 3: Add “测试当前 OCR”**

Generate a temporary image containing `vocekit OCR 测试 123`. Local tests never show an upload prompt. Cloud/vision tests show one explicit confirmation before sending.

- [ ] **Step 4: Persist and reload**

Restart the app and verify engine, languages, URL, key masking and model remain correct.

- [ ] **Step 5: Commit**

```bash
git add src/voiceassistant.cpp src/modules/settings_panel.inc config/*.example.json
git commit -m "feat: add OCR settings and interface test"
```

### Task 8: OCR Page and AI Handoff

**Files:**
- Create: `src/modules/hub_ocr_page.inc`
- Modify: `src/voiceassistant.cpp`
- Modify: `src/modules/result_choice_popup.inc`

- [ ] **Step 1: Add the OCR navigation page**

Place “OCR” below “词库”. The page must have one preview area, one editable result editor and the actions defined in the design.

- [ ] **Step 2: Implement image selection and preview**

Use:

```cpp
QFileDialog::getOpenFileName(
    this,
    tr8("选择截图"),
    QString(),
    tr8("图片 (*.png *.jpg *.jpeg *.bmp *.webp)")
);
```

Scale preview with `Qt::KeepAspectRatio` and never enlarge the original.

- [ ] **Step 3: Connect OCR status**

Required visible states:

```text
等待识别
正在识别
RapidOCR 不可用，正在切换 Windows OCR
识别完成
识别失败
```

- [ ] **Step 4: Connect AI actions**

Pass edited OCR text into the existing function pipeline as text-only input. “问答” opens the existing follow-up dialog with OCR text as selected context. Custom functions are populated dynamically.

- [ ] **Step 5: Verify editability**

Change one recognized word before clicking translation. History and the model request must use the edited text.

- [ ] **Step 6: Commit**

```bash
git add src/modules/hub_ocr_page.inc src/voiceassistant.cpp src/modules/result_choice_popup.inc
git commit -m "feat: add OCR workspace and AI actions"
```

### Task 9: OCR History, FAQ and Packaging

**Files:**
- Modify: `src/modules/hub_history_page.inc`
- Modify: `src/voiceassistant.cpp`
- Modify: `docs/DEVELOPMENT_LOG.md`
- Modify: `docs/AI_PROJECT_GUIDE.md`
- Modify: `docs/TESTING.md`
- Modify: `scripts/deploy.ps1`
- Modify: `scripts/package-test.ps1`

- [ ] **Step 1: Save OCR history**

Persist:

```json
{
  "modeId": "ocr",
  "ocrEngine": "rapidocr",
  "ocrLanguages": ["zh-Hans", "en"],
  "ocrElapsedMs": 320,
  "ocrUsedFallback": false,
  "imageFileName": "screenshot.png"
}
```

Do not copy or preserve the image.

- [ ] **Step 2: Add OCR history tab and details**

Show engine, languages, OCR time, fallback state and editable-result text.

- [ ] **Step 3: Add numbered FAQ entries**

Add every error listed in the design and map popup text through `faqIdForTitle()` and `faqIdForPopup()`.

- [ ] **Step 4: Package helpers and models**

`deploy.ps1` must fail with a clear message when a required helper or model is missing. The test ZIP must contain:

```text
vocekit.exe
ocr/rapidocr/...
ocr/windows/vocekit-windows-ocr.exe
```

- [ ] **Step 5: Run privacy scan**

Run:

```powershell
rg -n "sk-|client_secret=|api_key\":\\s*\"[^\\\"]+|C:\\Users\\[^\\]+" dist docs src config scripts
```

Expected: no real secrets or user-specific paths in the distributable source/package.

- [ ] **Step 6: Build and smoke-test**

Run Qt debug/release builds, helper builds, OCR tests and `cppcheck`. Manually test RapidOCR, forced fallback, both unavailable, cloud confirmation and AI handoff.

- [ ] **Step 7: Commit**

```bash
git add src docs scripts config vocekit.pro
git commit -m "feat: complete OCR workflow"
```
