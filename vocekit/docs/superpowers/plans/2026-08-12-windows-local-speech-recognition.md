# Windows Local Speech Recognition Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a selectable Windows-local speech provider that streams provisional offline transcription into the existing floating bar, performs a same-engine local batch fallback, and reaches the existing AI/write workflow exactly once with the final text.

**Architecture:** Keep Qt as the only microphone owner and send its existing 16 kHz mono 16-bit PCM to one .NET Framework helper process per recording. A focused QProcess adapter translates line-delimited helper JSON into the existing provider-neutral streaming callbacks, while an `ISpeechProvider` implementation reuses the same helper in batch mode. Persist one global Windows speech language setting, expose the provider through the shared catalog, and preserve all existing cloud-provider behavior.

**Tech Stack:** C++11, Qt 5.9 Core/Widgets/Multimedia/Concurrent, QProcess, QtTest, qmake, MinGW 5.3 x86, C# 7.3, .NET Framework 4.8 x64, System.Speech, MSBuild/Visual Studio 2022.

**Design:** `docs/superpowers/specs/2026-08-12-windows-local-speech-recognition-design.md`

---

## File map

### New helper files

- `helpers/windows_speech/windows_speech.csproj` — x64 .NET Framework 4.8 console-helper build.
- `helpers/windows_speech/Program.cs` — command-line validation, recognizer selection, JSON event output, probe, stream, and batch modes.
- `helpers/windows_speech/ProducerConsumerAudioStream.cs` — bounded PCM stream with the minimal seek/declared-length compatibility required by System.Speech, plus EOF and cancellation semantics.
- `scripts/build-windows-speech-helper.ps1` — deterministic Release helper build and output validation.
- `scripts/build-runtime-helpers.ps1` — builds both existing OCR helpers and the new speech helper.

### New Qt production files

- `src/providers/windows_speech_helper_protocol.h/.cpp` — provider-independent helper arguments, event parsing, language/error normalization, and application-directory-relative helper path.
- `src/providers/windows_speech_helper_client.h/.cpp` — synchronous probe/batch QProcess client with timeout, output limits, and cancellation.
- `src/providers/windows_speech_provider.h/.cpp` — `ISpeechProvider` adapter for configuration check and batch recognition.
- `src/providers/windows_streaming_speech_session.h/.cpp` — asynchronous QProcess streaming session and bounded PCM queue.
- `src/ui/windows_speech_settings_card.h/.cpp` — real language/status/test card used by API settings.

### Modified Qt production files

- `src/config/app_settings_data.h`, `src/config/app_settings_defaults.h/.cpp`, `src/config/app_settings_json.cpp` — provider ID, language catalog, normalization, persistence, and legacy default.
- `src/providers/provider_types.h`, `src/providers/built_in_provider_factory.cpp`, `src/providers/provider_configuration.cpp` — language propagation and Windows provider registration.
- `src/providers/streaming_speech_session.h`, `src/providers/streaming_speech_session_factory.h/.cpp`, `src/providers/streaming_speech_session_factory_default.cpp` — language/run ID and Windows session factory.
- `src/tasks/speech_recognition_task.h/.cpp`, `src/tasks/voice_speech_recognition_executor.h/.cpp`, `src/tasks/voice_long_recording_recognition_coordinator.h/.cpp`, `src/tasks/voice_long_recording_segment_executor.h/.cpp` — normal and long-recording batch language/error-code propagation.
- `src/controllers/voice_recording_workflow_controller.h/.cpp`, `src/controllers/voice_controller.cpp` — request language, local fallback state, actionable diagnostic wording, and exactly-once gates.
- `src/ui/attention_message.h/.cpp` — warning dialog action that opens Windows language settings when a local recognizer is missing.
- `src/ui/api_settings_section.h/.cpp`, `src/ui/settings_panel.cpp`, `src/ui/hub_settings_state.h/.cpp` — Windows card load/save/refresh.
- `src/tasks/interface_self_check_task.h/.cpp`, `src/ui/interface_self_check_card.h/.cpp`, `src/ui/diagnostics_panel.cpp` — Windows helper/recognizer self-check.
- `src/ui/function_command_page.cpp`, `src/controllers/tray_controller.cpp` — remove hard-coded provider lists.
- `vocekit.pro` — register new Qt production sources and headers.
- `scripts/run-all-tests.ps1`, `scripts/deploy.ps1`, `scripts/package-test.ps1`, `scripts/verify-runtime.ps1`, `docs/TESTING.md` — standalone fake-helper handling and build/deploy/probe/package contract.

### New and modified tests

- Modify `tests/config/app_settings_defaults_tests.cpp` and `tests/config/app_settings_json_tests.cpp`.
- Create `tests/providers/windows_speech_helper_protocol_tests.cpp/.pro`.
- Create `tests/providers/fake_windows_speech_helper.cpp/.pro`.
- Create `tests/providers/windows_speech_helper_client_tests.cpp/.pro`.
- Create `tests/providers/windows_speech_provider_tests.cpp/.pro`.
- Create `tests/providers/windows_streaming_speech_session_tests.cpp/.pro`.
- Modify `tests/providers/built_in_provider_factory_tests.cpp/.pro`, `tests/providers/provider_configuration_tests.cpp/.pro`, and `tests/providers/streaming_speech_session_factory_tests.cpp/.pro`.
- Modify `tests/tasks/voice_speech_recognition_executor_tests.cpp/.pro` and `tests/tasks/interface_self_check_task_tests.cpp/.pro`.
- Modify `tests/controllers/voice_recording_workflow_controller_tests.cpp/.pro` and `tests/controllers/tray_controller_exit_tests.cpp/.pro`.
- Create `tests/ui/windows_speech_settings_card_tests.cpp/.pro`.
- Modify `tests/ui/api_settings_section_header_tests.cpp`, `tests/ui/function_command_page_tests.cpp`, and their `.pro` files when the new production sources are linked.

---

## Exact focused-test command

Run every named QtTest `.pro` below from the repository root with this PowerShell function; pass the exact project paths listed in each task:

```powershell
$QtBin = 'D:\QQQQQT0001\5.9\mingw53_32\bin'
$MingwBin = 'D:\QQQQQT0001\Tools\mingw530_32\bin'
$env:PATH = "$QtBin;$MingwBin;$env:PATH"
function Invoke-VocekitReleaseTest([string]$ProjectPath) {
    $project = Get-Item -LiteralPath $ProjectPath
    $targetLine = Select-String -LiteralPath $project.FullName `
        -Pattern '^\s*TARGET\s*=\s*(.+?)\s*$' | Select-Object -First 1
    if (-not $targetLine) { throw "TARGET missing: $ProjectPath" }
    $target = $targetLine.Matches[0].Groups[1].Value.Trim()
    $makefile = "Makefile.codex.windows-speech.$target"
    Push-Location $project.DirectoryName
    try {
        & (Join-Path $QtBin 'qmake.exe') -o $makefile $project.Name `
            -spec win32-g++ 'CONFIG+=release'
        if ($LASTEXITCODE -ne 0) { throw "qmake failed: $target" }
        & (Join-Path $MingwBin 'mingw32-make.exe') -f $makefile -j2
        if ($LASTEXITCODE -ne 0) { throw "build failed: $target" }
        $exe = Join-Path $project.DirectoryName "release\$target.exe"
        & $exe -maxwarnings 0
        if ($LASTEXITCODE -ne 0) { throw "test failed: $target" }
    } finally {
        Pop-Location
    }
}
```

For the standalone fake-helper project, use the same qmake/make portion but do not run it without scenario arguments; the client/session QtTests launch it themselves. All expected GREEN QtTest runs must end with `0 failed, 0 skipped`.

---

### Task 1: Add the provider and language settings contract

**Files:**

- Modify: `src/config/app_settings_data.h`
- Modify: `src/config/app_settings_defaults.h`
- Modify: `src/config/app_settings_defaults.cpp`
- Modify: `src/config/app_settings_json.cpp`
- Test: `tests/config/app_settings_defaults_tests.cpp`
- Test: `tests/config/app_settings_json_tests.cpp`

- [ ] **Step 1: Write failing provider-catalog tests**

Add cases proving that Windows is a stable fourth provider and that language normalization is closed over three supported values:

```cpp
void windowsSpeechProviderIsCatalogued()
{
    QCOMPARE(speechProviderWindowsLocal(), QStringLiteral("windows-local"));
    QCOMPARE(normalizeSpeechProvider(QStringLiteral("WINDOWS-LOCAL")),
             speechProviderWindowsLocal());
    QCOMPARE(speechProviderTitle(speechProviderWindowsLocal()),
             QString::fromUtf8("Windows 本地语音识别"));
    QCOMPARE(supportedSpeechProviderIds(), QStringList()
        << speechProviderBaidu()
        << speechProviderXfyun()
        << speechProviderCustom()
        << speechProviderWindowsLocal());
}

void windowsSpeechLanguageNormalizesSafely()
{
    QCOMPARE(normalizeWindowsSpeechLanguage(QString()),
             windowsSpeechLanguageFollowWindows());
    QCOMPARE(normalizeWindowsSpeechLanguage(QStringLiteral("ZH-cn")),
             windowsSpeechLanguageChinese());
    QCOMPARE(normalizeWindowsSpeechLanguage(QStringLiteral("en-US")),
             windowsSpeechLanguageEnglish());
    QCOMPARE(normalizeWindowsSpeechLanguage(QStringLiteral("fr-FR")),
             windowsSpeechLanguageFollowWindows());
}
```

- [ ] **Step 2: Write failing JSON compatibility tests**

```cpp
void missingWindowsSpeechLanguageUsesFollowWindows()
{
    const AppSettingsData restored = appSettingsDataFromJson(QJsonObject());
    QCOMPARE(restored.windowsSpeechLanguage,
             windowsSpeechLanguageFollowWindows());
}

void windowsSpeechLanguageRoundTrips()
{
    AppSettingsData source;
    source.speechProvider = speechProviderWindowsLocal();
    source.windowsSpeechLanguage = windowsSpeechLanguageEnglish();
    const AppSettingsData restored = appSettingsDataFromJson(
        appSettingsDataToJson(source)
    );
    QCOMPARE(restored.speechProvider, speechProviderWindowsLocal());
    QCOMPARE(restored.windowsSpeechLanguage, windowsSpeechLanguageEnglish());
}
```

- [ ] **Step 3: Run both QtTest projects and verify RED**

Run from the repository root after defining `Invoke-VocekitReleaseTest`:

```powershell
Invoke-VocekitReleaseTest 'tests\config\app_settings_defaults_tests.pro'
Invoke-VocekitReleaseTest 'tests\config\app_settings_json_tests.pro'
```

Expected: compile failure because the Windows provider/language functions and persisted member do not exist.

- [ ] **Step 4: Implement the catalog and persisted field**

Add to `AppSettingsData`:

```cpp
QString speechProvider = QStringLiteral("baidu");
QString windowsSpeechLanguage = QStringLiteral("follow-windows");
```

Expose and implement:

```cpp
QString speechProviderWindowsLocal();
QString windowsSpeechLanguageFollowWindows();
QString windowsSpeechLanguageChinese();
QString windowsSpeechLanguageEnglish();
QStringList supportedWindowsSpeechLanguages();
QString normalizeWindowsSpeechLanguage(const QString &language);
QString windowsSpeechLanguageTitle(const QString &language);
```

Return `windows-local`, `follow-windows`, `zh-CN`, and `en-US` exactly. Preserve the current fallback of an unknown provider to Baidu, but recognize `windows-local` before that fallback. Read JSON with:

```cpp
data.windowsSpeechLanguage = normalizeWindowsSpeechLanguage(
    root.value(QStringLiteral("windowsSpeechLanguage"))
        .toString(windowsSpeechLanguageFollowWindows())
);
```

Write the normalized value under `windowsSpeechLanguage`.

- [ ] **Step 5: Run GREEN and legacy regressions**

Run both projects again. Expected: all tests pass, including old unknown-provider fallback and retained-root-value tests.

- [ ] **Step 6: Commit**

```powershell
git add src/config/app_settings_data.h src/config/app_settings_defaults.h src/config/app_settings_defaults.cpp src/config/app_settings_json.cpp tests/config/app_settings_defaults_tests.cpp tests/config/app_settings_json_tests.cpp
git commit -m "feat: add Windows speech settings contract"
```

### Task 2: Build the Windows speech helper and bounded audio stream

**Files:**

- Create: `helpers/windows_speech/windows_speech.csproj`
- Create: `helpers/windows_speech/ProducerConsumerAudioStream.cs`
- Create: `helpers/windows_speech/Program.cs`
- Create: `scripts/build-windows-speech-helper.ps1`
- Create: `scripts/build-runtime-helpers.ps1`

- [ ] **Step 1: Add a helper self-test that initially fails to build**

Define these modes in the command parser and make `--self-test` exercise the stream without loading a recognizer:

```csharp
if (options.SelfTest)
{
    using (var stream = new ProducerConsumerAudioStream(32))
    {
        Require(stream.TryWriteChunk(new byte[] { 1, 2, 3, 4 }), "write");
        stream.CompleteWriting();
        var output = new byte[8];
        var count = stream.Read(output, 0, output.Length);
        Require(count == 4, "bounded stream length");
        Require(output.Take(4).SequenceEqual(new byte[] { 1, 2, 3, 4 }),
                "bounded stream bytes");
        Require(stream.Read(output, 0, output.Length) == 0, "EOF");
    }
    WriteEvent("self-test", options.RunId, new { ok = true });
    return 0;
}
```

Run the new build script before creating the project. Expected: fail with `windows_speech.csproj` missing.

- [ ] **Step 2: Implement the bounded producer/consumer stream**

Implement a forward-only SAPI-compatible `Stream` with a monitor-protected queue:

```csharp
public sealed class ProducerConsumerAudioStream : Stream
{
    private readonly Queue<byte[]> chunks = new Queue<byte[]>();
    private readonly int capacityBytes;
    private int queuedBytes;
    private int chunkOffset;
    private bool completed;
    private bool cancelled;
    private long readPosition;
    private readonly long declaredLength = 64L * 1024L * 1024L;

    public bool TryWriteChunk(byte[] value)
    {
        lock (chunks)
        {
            if (completed || cancelled || queuedBytes + value.Length > capacityBytes)
                return false;
            chunks.Enqueue(value);
            queuedBytes += value.Length;
            Monitor.PulseAll(chunks);
            return true;
        }
    }

    public void CompleteWriting()
    {
        lock (chunks) { completed = true; Monitor.PulseAll(chunks); }
    }

    public void Cancel()
    {
        lock (chunks) { cancelled = true; chunks.Clear(); queuedBytes = 0; Monitor.PulseAll(chunks); }
    }
}
```

`Read` must wait while it has not filled the caller's requested count and the queue is temporarily empty, return queued bytes in order, and return a short read/zero only at EOF or cancel. `CanRead=true`, `CanSeek=true`, `CanWrite=false`. `Length` reports the fixed 64 MiB declared maximum without allocating it. `Position` reports consumed bytes. Only `Seek(0, SeekOrigin.Current)` and setting `Position` to its current value are accepted; every reposition attempt and write throws `NotSupportedException`. Reject cumulative input beyond the 64 MiB declaration. Keep the actual queued-memory capacity at 64,000 bytes.

These unusual semantics are required because the .NET Framework System.Speech/SAPI wrapper probes `Length`, `Position`, and `Seek(0, Current)` at bind time. Add self-test assertions that a zero-length declaration is rejected by the test fixture, and that the compatible stream produces audio reads without allowing rewind.

- [ ] **Step 3: Implement probe, stream, and batch modes**

Use `SpeechRecognitionEngine.InstalledRecognizers()` and deterministic language resolution:

```csharp
private static RecognizerInfo ResolveRecognizer(string requested)
{
    var installed = SpeechRecognitionEngine.InstalledRecognizers().ToList();
    string target = requested == "follow-windows"
        ? CultureInfo.CurrentUICulture.Name
        : requested;
    var exact = installed.FirstOrDefault(x =>
        String.Equals(x.Culture.Name, target, StringComparison.OrdinalIgnoreCase));
    if (exact != null) return exact;
    if (requested == "follow-windows")
    {
        string neutral = new CultureInfo(target).TwoLetterISOLanguageName;
        var sameLanguage = installed.FirstOrDefault(x =>
            x.Culture.TwoLetterISOLanguageName == neutral);
        if (sameLanguage != null) return sameLanguage;
    }
    throw new HelperException("RECOGNIZER_MISSING", target);
}
```

For `--probe`, load `DictationGrammar` and emit one `probe` JSON object containing `ok`, `resolvedLanguage`, and `installedLanguages`. For stream/batch, feed stdin on a producer thread into the bounded stream, call `SetInputToAudioStream(...16000, Sixteen, Mono)`, publish `hypothesis` and `recognized` events, and after EOF publish exactly one `final` with concatenated committed text. Catch exceptions into stable `error` JSON with `errorCode`; keep stdout JSON-only and send diagnostics to stderr.

- [ ] **Step 4: Create the .NET Framework project and build scripts**

The project must target x64 Release and reference `System.Speech`:

```xml
<Project ToolsVersion="15.0" DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup>
    <Configuration Condition=" '$(Configuration)' == '' ">Release</Configuration>
    <Platform Condition=" '$(Platform)' == '' ">x64</Platform>
    <ProjectGuid>{9F9AC44A-8B6F-4D22-B1FC-7E3FE74C8D51}</ProjectGuid>
    <OutputType>Exe</OutputType>
    <TargetFrameworkVersion>v4.8</TargetFrameworkVersion>
    <PlatformTarget>x64</PlatformTarget>
    <LangVersion>7.3</LangVersion>
    <AssemblyName>vocekit-windows-speech</AssemblyName>
    <OutputPath>..\bin\</OutputPath>
  </PropertyGroup>
  <ItemGroup>
    <Reference Include="System" />
    <Reference Include="System.Core" />
    <Reference Include="System.Speech" />
    <Reference Include="System.Web.Extensions" />
    <Compile Include="Program.cs" />
    <Compile Include="ProducerConsumerAudioStream.cs" />
  </ItemGroup>
  <Import Project="$(MSBuildToolsPath)\Microsoft.CSharp.targets" />
</Project>
```

`build-windows-speech-helper.ps1` must locate VS2022 MSBuild, build `/p:Configuration=Release /p:Platform=x64`, assert `helpers/bin/vocekit-windows-speech.exe`, then run `--self-test --run-id build-check` and require exit code zero. `build-runtime-helpers.ps1` must invoke `build-ocr-helpers.ps1` and the new script and propagate either failure.

- [ ] **Step 5: Verify helper self-test and real probe**

Run:

```powershell
.\scripts\build-windows-speech-helper.ps1
& .\helpers\bin\vocekit-windows-speech.exe --self-test --run-id manual-self-test
& .\helpers\bin\vocekit-windows-speech.exe --probe --language zh-CN --run-id zh-probe
& .\helpers\bin\vocekit-windows-speech.exe --probe --language en-US --run-id en-probe
```

Expected: each command exits zero; self-test reports `ok:true`; probes resolve exactly `zh-CN` and `en-US` on the current machine. Before integrating Qt, run a helper-only synthesized-stream test for both languages and require at least one `hypothesis` or `recognized` event plus `InputStreamEnded=true`; this locks in the SAPI compatibility semantics discovered during feasibility probing.

- [ ] **Step 6: Commit helper sources and scripts**

Do not add `helpers/bin` build outputs:

```powershell
git add helpers/windows_speech scripts/build-windows-speech-helper.ps1 scripts/build-runtime-helpers.ps1
git commit -m "feat: add Windows speech recognition helper"
```

### Task 3: Add the Qt helper protocol and synchronous client

**Files:**

- Create: `src/providers/windows_speech_helper_protocol.h`
- Create: `src/providers/windows_speech_helper_protocol.cpp`
- Create: `src/providers/windows_speech_helper_client.h`
- Create: `src/providers/windows_speech_helper_client.cpp`
- Create: `tests/providers/windows_speech_helper_protocol_tests.cpp`
- Create: `tests/providers/windows_speech_helper_protocol_tests.pro`
- Create: `tests/providers/fake_windows_speech_helper.cpp`
- Create: `tests/providers/fake_windows_speech_helper.pro`
- Create: `tests/providers/windows_speech_helper_client_tests.cpp`
- Create: `tests/providers/windows_speech_helper_client_tests.pro`

- [ ] **Step 1: Write RED protocol tests**

Cover every accepted event and every rejection boundary:

```cpp
void parsesFinalEvent()
{
    const WindowsSpeechHelperEvent event = parseWindowsSpeechHelperEvent(
        QByteArray("{\"protocolVersion\":1,\"runId\":\"r1\",\"type\":\"final\",\"text\":\"你好\"}")
    );
    QVERIFY(event.valid);
    QCOMPARE(event.type, WindowsSpeechHelperEventType::Final);
    QCOMPARE(event.runId, QStringLiteral("r1"));
    QCOMPARE(event.text, QString::fromUtf8("你好"));
}

void rejectsWrongVersionOrMissingRunId()
{
    QVERIFY(!parseWindowsSpeechHelperEvent(
        QByteArray("{\"protocolVersion\":2,\"runId\":\"r1\",\"type\":\"ready\"}")
    ).valid);
    QVERIFY(!parseWindowsSpeechHelperEvent(
        QByteArray("{\"protocolVersion\":1,\"type\":\"ready\"}")
    ).valid);
}
```

Also assert helper arguments contain `--mode`, `--run-id`, `--language`, `--sample-rate 16000`, `--channels 1`, `--bits 16`, and that the runtime helper path is `<applicationDir>/speech/windows/vocekit-windows-speech.exe`. Add a regression where `applicationDir` ends in `release`: the function must retain `release` rather than applying the config/history `appBasePath()` rule that strips it.

- [ ] **Step 2: Run the protocol project and verify RED**

Expected: compile failure because the protocol types do not exist.

- [ ] **Step 3: Implement the protocol types and strict parser**

Define:

```cpp
enum class WindowsSpeechHelperEventType {
    Invalid, Ready, Hypothesis, Recognized, Final, Probe, Error, SelfTest
};

struct WindowsSpeechHelperEvent {
    bool valid = false;
    WindowsSpeechHelperEventType type = WindowsSpeechHelperEventType::Invalid;
    QString runId;
    QString text;
    QString errorCode;
    QString errorMessage;
    QString resolvedLanguage;
    QStringList installedLanguages;
};

QString windowsSpeechHelperPathForApplicationDir(const QString &applicationDir);
QStringList windowsSpeechHelperArguments(const QString &mode,
    const QString &runId, const QString &language,
    int sampleRate, int channelCount, int bits);
WindowsSpeechHelperEvent parseWindowsSpeechHelperEvent(const QByteArray &line);
QString windowsSpeechOperationErrorCode(const QString &helperErrorCode);
bool isWindowsSpeechConfigurationErrorCode(const QString &operationErrorCode);
```

Reject non-object JSON, version not equal to 1, missing run ID, unknown event type, missing required text/error fields, embedded newline, and payloads over 64 KiB. Normalize display errors without echoing raw stdout or transcript. Map helper codes deterministically: `PROGRAM_MISSING -> speech.windows.program_missing`, `RECOGNIZER_MISSING -> speech.windows.recognizer_missing`, `SYSTEM_SPEECH_UNAVAILABLE -> speech.windows.runtime_missing`, `GRAMMAR_LOAD_FAILED -> speech.windows.grammar_load_failed`, `NO_SPEECH -> speech.empty_result`, `CANCELLED -> operation.cancelled`, and all other failures to `speech.windows.local`.

- [ ] **Step 4: Build a deterministic fake helper**

The fake Qt console program reads mode switches and stdin, then emits controlled JSON. Support arguments `--scenario ready-final`, `split-lines`, `invalid-json`, `wrong-run-id`, `crash`, `no-ready`, `no-final`, `duplicate-final`, and `echo-pcm-size`. Its normal path must emit:

```cpp
writeEvent(QStringLiteral("ready"), runId, QJsonObject());
writeEvent(QStringLiteral("hypothesis"), runId,
           QJsonObject{{QStringLiteral("text"), QString::fromUtf8("你")}});
writeEvent(QStringLiteral("recognized"), runId,
           QJsonObject{{QStringLiteral("text"), QString::fromUtf8("你好")}});
const QByteArray pcm = QFile().readAll(); // use a QFile opened on stdin
writeEvent(QStringLiteral("final"), runId,
           QJsonObject{{QStringLiteral("text"), QString::fromUtf8("你好")},
                       {QStringLiteral("pcmBytes"), pcm.size()}});
```

- [ ] **Step 5: Write RED client tests using the fake helper**

Test probe and batch calls with dependency-injected program/arguments:

```cpp
WindowsSpeechHelperClient client(fakeHelperPath());
WindowsSpeechBatchRequest request;
request.runId = QStringLiteral("batch-1");
request.language = QStringLiteral("zh-CN");
request.pcm = QByteArray::fromHex("010203040506");
request.timeoutMs = 3000;
const WindowsSpeechHelperResult result = client.recognize(request);
QVERIFY(result.ok);
QCOMPARE(result.text, QString::fromUtf8("你好"));
QCOMPARE(result.pcmBytesObserved, request.pcm.size());
```

Add missing program, start failure, invalid JSON, wrong run ID, output over 1 MiB, timeout, crash, cancellation, empty final, and duplicate-final cases. Expected RED: client types are missing.

- [ ] **Step 6: Implement the synchronous client**

Use `QProcess::SeparateChannels`, wait for start up to 5 seconds, write all PCM with partial-write handling, close the write channel, poll at 50 ms for cancellation/timeout, cap stdout and stderr at 1 MiB, and accept exactly one matching terminal event. Return stable codes such as `PROGRAM_MISSING`, `START_FAILED`, `WRITE_FAILED`, `INVALID_RESPONSE`, `RUN_ID_MISMATCH`, `OUTPUT_TOO_LARGE`, `PROCESS_CRASHED`, `TIMEOUT`, `CANCELLED`, and `EMPTY_TEXT`.

- [ ] **Step 7: Run all three projects GREEN and commit**

Build fake helper first, then protocol and client tests. Expected: all pass without network, microphone, or installed recognizer dependency.

```powershell
git add src/providers/windows_speech_helper_protocol.* src/providers/windows_speech_helper_client.* tests/providers/fake_windows_speech_helper.* tests/providers/windows_speech_helper_protocol_tests.* tests/providers/windows_speech_helper_client_tests.*
git commit -m "feat: add Windows speech helper client"
```

### Task 4: Add the batch Windows speech Provider

**Files:**

- Create: `src/providers/windows_speech_provider.h`
- Create: `src/providers/windows_speech_provider.cpp`
- Modify: `src/providers/provider_types.h`
- Modify: `src/providers/built_in_provider_factory.cpp`
- Modify: `src/providers/provider_configuration.cpp`
- Modify: `src/tasks/speech_recognition_task.h`
- Modify: `src/tasks/speech_recognition_task.cpp`
- Modify: `src/tasks/voice_speech_recognition_executor.h`
- Modify: `src/tasks/voice_speech_recognition_executor.cpp`
- Modify: `src/tasks/voice_long_recording_recognition_coordinator.h`
- Modify: `src/tasks/voice_long_recording_recognition_coordinator.cpp`
- Modify: `src/tasks/voice_long_recording_segment_executor.h`
- Modify: `src/tasks/voice_long_recording_segment_executor.cpp`
- Create: `tests/providers/windows_speech_provider_tests.cpp`
- Create: `tests/providers/windows_speech_provider_tests.pro`
- Modify: `tests/providers/built_in_provider_factory_tests.cpp`
- Modify: `tests/providers/built_in_provider_factory_tests.pro`
- Modify: `tests/providers/provider_configuration_tests.cpp`
- Modify: `tests/providers/provider_configuration_tests.pro`
- Modify: `tests/tasks/voice_speech_recognition_executor_tests.cpp`
- Modify: `tests/tasks/voice_speech_recognition_executor_tests.pro`

- [ ] **Step 1: Write RED Provider and propagation tests**

Use a fake client function so the tests never invoke System.Speech:

```cpp
void recognizePassesPcmAndLanguageToLocalHelper()
{
    WindowsSpeechBatchRequest captured;
    WindowsSpeechProvider provider(
        [&captured](const WindowsSpeechBatchRequest &request) {
            captured = request;
            WindowsSpeechHelperResult result;
            result.ok = true;
            result.text = QString::fromUtf8("本地结果");
            return result;
        },
        WindowsSpeechProvider::ProbeFunction()
    );
    SpeechRecognitionRequest request;
    request.audioData = QByteArray::fromHex("01020304");
    request.language = windowsSpeechLanguageChinese();
    const SpeechRecognitionResult result = provider.recognize(
        request, CancellationToken()
    );
    QVERIFY(result.isSuccess());
    QCOMPARE(captured.pcm, request.audioData);
    QCOMPARE(captured.language, windowsSpeechLanguageChinese());
}
```

Add assertions that Windows configuration does not ask for API keys, registry/factory returns ID `windows-local`, helper error codes map to the exact namespaced codes defined in Task 3, and the voice executor preserves both language and error code through `SpeechRecognitionProviderTaskRequest`.

- [ ] **Step 2: Run the focused projects and verify RED**

Expected: compile failures for the missing provider and language fields.

- [ ] **Step 3: Propagate language through batch task types**

Add `QString language = QStringLiteral("follow-windows");` to `VoiceSpeechRecognitionRequest`, `SpeechRecognitionProviderTaskRequest`, and `VoiceLongRecordingRecognitionConfig`. Copy it in `VoiceSpeechRecognitionExecutor::run`, then into `SpeechRecognitionRequest` in `runSpeechRecognitionProviderTask`, and from the long-recording config into each segment request:

```cpp
speechRequest.language = request.language;
providerRequest.language = request.language;
request.speech.language = m_config.language;
```

Preserve the structured provider error code alongside the localized message:

```cpp
struct SpeechRecognitionTaskResult {
    // existing fields...
    QString errorCode;
};

struct VoiceSpeechRecognitionResult {
    // existing fields...
    QString errorCode;
};

struct VoiceLongRecordingSegmentResult {
    // existing fields...
    QString errorCode;
};
```

`runSpeechRecognitionProviderTask` copies `providerResult.error.code`; `VoiceSpeechRecognitionExecutor` copies it; `VoiceLongRecordingSegmentExecutor` keeps the last attempt's code. Existing cloud tests must assert their established message behavior is unchanged.

Existing cloud providers ignore the field and retain their current request behavior.

- [ ] **Step 4: Implement and register `WindowsSpeechProvider`**

Implement `id()` as `speechProviderWindowsLocal()`. `checkConfiguration()` performs a `follow-windows` probe and reports the resolved/installed languages. `recognize()` validates 16 kHz PCM, calls the injected batch client with a fresh execution/run ID, maps cancellation before and after the call, rejects an empty final text, and records only provider/language/duration/text length in logs.

Add factory branches before existing fallback logic:

```cpp
if (normalized == speechProviderWindowsLocal()) {
    return createWindowsSpeechProvider();
}
```

Register it in `registerBuiltInProviders`. In `speechProviderConfigurationErrorForSecrets`, return an empty string for Windows because it requires no secret; the real provider/session probe remains authoritative for helper/language availability.

- [ ] **Step 5: Run GREEN and cloud-provider regressions**

Run Windows provider, built-in factory, provider configuration, voice executor, Baidu provider, Xfyun provider, and custom provider projects. Expected: all pass; no fake test calls the real helper or network.

- [ ] **Step 6: Commit**

```powershell
git add src/providers/windows_speech_provider.* src/providers/provider_types.h src/providers/built_in_provider_factory.cpp src/providers/provider_configuration.cpp src/tasks/speech_recognition_task.* src/tasks/voice_speech_recognition_executor.* src/tasks/voice_long_recording_recognition_coordinator.* src/tasks/voice_long_recording_segment_executor.* tests/providers/windows_speech_provider_tests.* tests/providers/built_in_provider_factory_tests.* tests/providers/provider_configuration_tests.* tests/tasks/voice_speech_recognition_executor_tests.*
git commit -m "feat: add Windows local speech provider"
```

### Task 5: Add the asynchronous Windows streaming session

**Files:**

- Create: `src/providers/windows_streaming_speech_session.h`
- Create: `src/providers/windows_streaming_speech_session.cpp`
- Modify: `src/providers/streaming_speech_session.h`
- Modify: `src/providers/streaming_speech_session_factory.h`
- Modify: `src/providers/streaming_speech_session_factory.cpp`
- Modify: `src/providers/streaming_speech_session_factory_default.cpp`
- Create: `tests/providers/windows_streaming_speech_session_tests.cpp`
- Create: `tests/providers/windows_streaming_speech_session_tests.pro`
- Modify: `tests/providers/streaming_speech_session_factory_tests.cpp`
- Modify: `tests/providers/streaming_speech_session_factory_tests.pro`

- [ ] **Step 1: Write RED session tests against the fake helper**

Cover start/ready, PCM before ready, partial writes, snapshot replacement, commit, final, finish EOF, cancel, queue overflow, bad JSON, wrong run ID, duplicate final, startup timeout, final timeout, crash, and destruction cleanup:

```cpp
void streamsPcmAndDeliversOneFinal()
{
    QStringList completed;
    QVector<StreamingTranscriptSnapshot> snapshots;
    StreamingSpeechCallbacks callbacks;
    callbacks.transcriptUpdated = [&](const StreamingTranscriptSnapshot &s) {
        snapshots.append(s);
    };
    callbacks.completed = [&](const QString &text) { completed.append(text); };

    StreamingSpeechSessionRequest request;
    request.provider = speechProviderWindowsLocal();
    request.language = windowsSpeechLanguageChinese();
    request.runId = QStringLiteral("stream-1");
    WindowsStreamingSpeechSession session(
        fakeHelperPath(), QStringList() << QStringLiteral("--scenario")
                                       << QStringLiteral("ready-final"),
        request, callbacks, fastTiming()
    );
    QString error;
    QVERIFY(session.start(&error));
    QVERIFY(session.pushAudio(QByteArray::fromHex("01020304")));
    session.finish();
    QTRY_COMPARE(completed.size(), 1);
    QCOMPARE(completed.first(), QString::fromUtf8("你好"));
}
```

- [ ] **Step 2: Run session and factory tests to verify RED**

Expected: compile failure because `WindowsStreamingSpeechSession`, `request.language`, `request.runId`, and `createWindows` do not exist.

- [ ] **Step 3: Extend the neutral request and factory dependency**

Add:

```cpp
QString language = QStringLiteral("follow-windows");
QString runId;
```

to `StreamingSpeechSessionRequest`, and add a no-secret local factory:

```cpp
using LocalProviderFactory = std::function<QSharedPointer<IStreamingSpeechSession>(
    const StreamingSpeechSessionRequest &,
    const StreamingSpeechCallbacks &
)>;
LocalProviderFactory createWindows;
```

Extend the callbacks with a structured non-transient startup failure:

```cpp
std::function<void(const QString &, const QString &)> configurationFailed;
```

The generic factory must route `windows-local` without loading secrets, reject a missing local factory with a local-component message, and leave existing Baidu/Xfyun credential gates unchanged.

- [ ] **Step 4: Implement the QProcess session**

The session owns a `QProcess`, startup/final timers, stdout buffer, bounded audio queue, committed/provisional strings, and a terminal/degraded flag. Use `readyReadStandardOutput`, `bytesWritten`, `finished`, and `errorOccurred`. Parse complete newline-delimited JSON only; keep an incomplete final line buffered. On helper events:

```cpp
case WindowsSpeechHelperEventType::Hypothesis:
    m_provisionalText = event.text;
    emitSnapshot();
    break;
case WindowsSpeechHelperEventType::Recognized:
    m_committedText += event.text;
    m_provisionalText.clear();
    emitSnapshot();
    break;
case WindowsSpeechHelperEventType::Final:
    completeOnce(event.text.trimmed());
    break;
case WindowsSpeechHelperEventType::Error:
    handleHelperError(event.errorCode, windowsSpeechDisplayError(event));
    break;
```

Queue no more than 64,000 bytes. Before `ready`, `PROGRAM_MISSING`, `RECOGNIZER_MISSING`, `SYSTEM_SPEECH_UNAVAILABLE`, and `GRAMMAR_LOAD_FAILED` call `configurationFailed(windowsSpeechOperationErrorCode(code), message)` exactly once; these are not transient streaming failures. A `QProcess::FailedToStart` before `ready` uses `speech.windows.program_missing` when the resolved file is absent and `speech.windows.local` otherwise. After `ready`, process, protocol, timeout, and queue failures use `degraded`. `finish()` marks finalizing, drains queued PCM, then closes the write channel exactly once. `cancel()` disconnects business callbacks, clears buffers, terminates and after 250 ms kills the process. Destructor calls the same cleanup without invoking callbacks.

- [ ] **Step 5: Wire the production factory**

`createDefaultStreamingSpeechSession` resolves `<QCoreApplication::applicationDirPath()>/speech/windows/vocekit-windows-speech.exe` and constructs the Windows session. It must not use `appBasePath()` because that helper deliberately strips a development `debug`/`release` directory for configuration files; runtime children live beside the deployed executable. It must not pass proxy settings to the local helper and must use the normalized configured language/run ID already present in the request.

- [ ] **Step 6: Run GREEN and existing streaming regressions**

Run fake helper, Windows session, generic factory, Baidu streaming, and Xfyun streaming projects. Expected: all pass and no residual fake-helper processes remain.

- [ ] **Step 7: Commit**

```powershell
git add src/providers/windows_streaming_speech_session.* src/providers/streaming_speech_session.h src/providers/streaming_speech_session_factory.* tests/providers/windows_streaming_speech_session_tests.* tests/providers/streaming_speech_session_factory_tests.*
git commit -m "feat: stream Windows local speech through helper"
```

### Task 6: Integrate language, local fallback, and exactly-once workflow behavior

**Files:**

- Modify: `src/controllers/voice_recording_workflow_controller.h`
- Modify: `src/controllers/voice_recording_workflow_controller.cpp`
- Modify: `src/controllers/voice_controller.cpp`
- Modify: `src/ui/attention_message.h`
- Modify: `src/ui/attention_message.cpp`
- Modify: `tests/controllers/voice_recording_workflow_controller_tests.cpp`
- Modify: `tests/controllers/voice_recording_workflow_controller_tests.pro`

- [ ] **Step 1: Write RED workflow tests**

Add real controller tests proving:

1. Windows streaming request receives normalized language and a non-empty run ID.
2. provisional/committed snapshots update the C floating bar but do not call downstream processing.
3. successful final calls downstream exactly once and never invokes batch recognition.
4. degraded/final-timeout Windows streaming invokes batch exactly once with provider `windows-local`, the same language, and full original PCM.
5. duplicate/late final, previous generation, and cancelled sessions call downstream zero additional times.
6. cancel does not batch fallback and terminates the fake session.
7. long-recording segment requests preserve the configured Windows language.
8. `RECOGNIZER_MISSING` and `PROGRAM_MISSING` survive batch and long-recording task boundaries and use the actionable Windows warning path rather than a cloud-key warning.
9. a pre-ready configuration failure stops the current recording and does not batch fallback through the same known-bad component.

Representative assertions:

```cpp
QCOMPARE(capturedStreamingRequest.provider, speechProviderWindowsLocal());
QCOMPARE(capturedStreamingRequest.language, windowsSpeechLanguageEnglish());
QVERIFY(!capturedStreamingRequest.runId.trimmed().isEmpty());
QCOMPARE(batchRequests.size(), 1);
QCOMPARE(batchRequests.first().provider, speechProviderWindowsLocal());
QCOMPARE(batchRequests.first().language, windowsSpeechLanguageEnglish());
QCOMPARE(batchRequests.first().audioData, recordedPcm);
QCOMPARE(processedTexts, QStringList() << QString::fromUtf8("最终文本"));
```

- [ ] **Step 2: Run the controller project and verify RED**

Expected failures: language/run ID are absent and batch language is not propagated.

- [ ] **Step 3: Populate streaming and batch requests**

When creating a session:

```cpp
request.language = normalizeWindowsSpeechLanguage(
    m_settings.windowsSpeechLanguage
);
request.runId = QString::number(m_operationGeneration)
    + QLatin1Char('-') + QUuid::createUuid().toString()
        .remove(QLatin1Char('{')).remove(QLatin1Char('}'));
```

For classic and flow batch requests:

```cpp
request.language = normalizeWindowsSpeechLanguage(
    m_settings.windowsSpeechLanguage
);
```

Set the same normalized language on `VoiceLongRecordingRecognitionConfig` before scheduling segments. Cloud providers continue to ignore the language value.

- [ ] **Step 4: Preserve same-engine fallback and diagnostics**

Reuse the current generic degraded path so its batch request keeps `m_settings.speechProvider` or `m_flow.provider`; do not select another provider. When the active provider is Windows, set the floating status to “实时识别已中断，确认后将本地重新识别” and batch status to “正在使用 Windows 本地语音识别重新识别”. Keep `m_streamingTerminalHandled`, operation generation, flow run ID, and cancellation token as the gates before final delivery.

Extend `VoiceRecordingWorkflowAccess` with an actionable local-component failure callback:

```cpp
std::function<void(const QString &, const QString &)> showWindowsSpeechFailure;
```

Add to `attention_message.h/.cpp`:

```cpp
void showAttentionWarningWithAction(
    QWidget *parent,
    const QString &title,
    const QString &text,
    const QString &actionText,
    const std::function<void()> &action
);
```

The dialog adds one `QMessageBox::ActionRole` button and calls the action only when that button is clicked. `VoiceController` receives `(errorCode, message)` and uses `isWindowsSpeechConfigurationErrorCode(...)` rather than matching localized text. For `speech.windows.recognizer_missing`, `speech.windows.runtime_missing`, and `speech.windows.grammar_load_failed`, it shows action text “打开 Windows 语言设置” and calls `QDesktopServices::openUrl(QUrl("ms-settings:regionlanguage"))`. For `speech.windows.program_missing`, it shows a non-actionable warning that tells the user to reinstall or fully extract the application; opening language settings cannot repair a missing packaged helper. Normal batch, flow batch, long-recording segments, and streaming use the same mapping. Add a modal test that clicks the language action and verifies exactly one callback, an OK path that verifies zero, and a program-missing path that verifies no language-settings button.

Wire `StreamingSpeechCallbacks::configurationFailed` separately from `degraded`. When it arrives before `ready`, end the active generation through the existing cancellation path, stop capture, preserve the WAV path for diagnostics, set processing false, and invoke `showWindowsSpeechFailure`; do not schedule batch fallback because the same helper or recognizer is already known unusable. A process/protocol failure after `ready` still uses the same-engine batch fallback.

- [ ] **Step 5: Run GREEN and long-recording/flow regressions**

Run the full controller project. Expected: all old streaming and batch cases plus new Windows cases pass. Explicitly inspect that test doubles receive exactly one batch call and one-or-zero downstream calls as required.

- [ ] **Step 6: Commit**

```powershell
git add src/controllers/voice_recording_workflow_controller.h src/controllers/voice_recording_workflow_controller.cpp src/controllers/voice_controller.cpp src/ui/attention_message.h src/ui/attention_message.cpp tests/controllers/voice_recording_workflow_controller_tests.cpp tests/controllers/voice_recording_workflow_controller_tests.pro
git commit -m "feat: integrate Windows speech recording workflow"
```

### Task 7: Add the Windows language/status/test settings card and self-check

**Files:**

- Create: `src/ui/windows_speech_settings_card.h`
- Create: `src/ui/windows_speech_settings_card.cpp`
- Modify: `src/ui/api_settings_section.h`
- Modify: `src/ui/api_settings_section.cpp`
- Modify: `src/ui/settings_panel.cpp`
- Modify: `src/ui/hub_settings_state.h`
- Modify: `src/ui/hub_settings_state.cpp`
- Modify: `src/tasks/interface_self_check_task.h`
- Modify: `src/tasks/interface_self_check_task.cpp`
- Modify: `src/ui/interface_self_check_card.h`
- Modify: `src/ui/interface_self_check_card.cpp`
- Modify: `src/ui/diagnostics_panel.cpp`
- Create: `tests/ui/windows_speech_settings_card_tests.cpp`
- Create: `tests/ui/windows_speech_settings_card_tests.pro`
- Modify: `tests/ui/api_settings_section_header_tests.cpp`
- Modify: `tests/ui/api_settings_section_header_tests.pro`
- Modify: `tests/ui/hub_settings_state_tests.cpp`
- Modify: `tests/tasks/interface_self_check_task_tests.cpp`
- Modify: `tests/tasks/interface_self_check_task_tests.pro`

- [ ] **Step 1: Write RED real-widget card tests**

Construct the card with an injected probe and open-settings callback. Verify all three language values, legacy default, test busy/result state, missing-language error, callback execution, minimum 40 px button height, no fixed-height clipping, and 150% font scaling:

```cpp
QComboBox *language = card.findChild<QComboBox *>(
    QStringLiteral("windowsSpeechLanguageBox")
);
QPushButton *test = card.findChild<QPushButton *>(
    QStringLiteral("windowsSpeechTestButton")
);
QPushButton *open = card.findChild<QPushButton *>(
    QStringLiteral("windowsSpeechOpenSettingsButton")
);
QVERIFY(language && test && open);
QCOMPARE(language->count(), 3);
QCOMPARE(language->currentData().toString(),
         windowsSpeechLanguageFollowWindows());
QVERIFY(test->minimumHeight() >= 40);
QVERIFY(open->minimumHeight() >= 40);
```

- [ ] **Step 2: Write RED save/refresh and self-check tests**

Assert `ApiSettingsSnapshot` carries `windowsSpeechLanguage`, saving the API section persists provider/ocr/language together, `HubSettingsState` normalizes it, and an injected self-check probe yields a success or stable missing-recognizer line without reading secrets or using network.

- [ ] **Step 3: Run focused projects and verify RED**

Expected: missing card, snapshot member, state accessors, and self-check dependency.

- [ ] **Step 4: Implement the card with background probing**

Define callbacks:

```cpp
struct WindowsSpeechSettingsCardCallbacks {
    std::function<QStringList(const QString &, const CancellationToken &)> probe;
    std::function<void()> openWindowsLanguageSettings;
};
```

Use `DiagnosticTaskRunner` so clicking Test never blocks the UI. Show actual resolved language and installed languages on success. On `RECOGNIZER_MISSING`, show the requested language and enable a button that calls:

```cpp
QDesktopServices::openUrl(QUrl(QStringLiteral("ms-settings:regionlanguage")));
```

Expose `language()` and `setLanguage()` using normalized IDs, not translated labels.

- [ ] **Step 5: Integrate API settings load/save/visibility**

Add `windowsSpeechLanguage` to `ApiSettingsSnapshot` and change the save callback to:

```cpp
std::function<bool(const QString &, const QString &, const QString &)>
    saveRuntimeSettings;
```

Create the card under the speech rows, set it visible only for `speechProviderWindowsLocal()`, populate the provider combo by iterating `supportedSpeechProviderIds()`, and save the card language with speech provider and OCR engine. `SettingsPanel` updates `AppSettingsData.windowsSpeechLanguage`; `HubSettingsState` adds normalized getter/setter methods.

- [ ] **Step 6: Add the self-check route**

Extend `InterfaceSelfCheckRequest` with `windowsSpeechLanguage`, `applicationDirPath`, and an injectable probe function. For target `windows_speech` or `all`, invoke the helper probe at `<applicationDirPath>/speech/windows/...`, map result lines, and respect cancellation. The diagnostics card supplies `QCoreApplication::applicationDirPath()` separately from its existing `applicationBasePath` used by OCR/config helpers. Add “Windows 本地语音识别” to the self-check target combo. Tests inject a fake probe; they must not require the built helper.

- [ ] **Step 7: Run GREEN and clipping regressions**

Run card, API settings header, HubSettingsState, self-check, custom dialog support, and secret config tests. Render the card with 100%, 125%, 150% font sizes and verify labels/buttons are not clipped.

- [ ] **Step 8: Commit**

```powershell
git add src/ui/windows_speech_settings_card.* src/ui/api_settings_section.* src/ui/settings_panel.cpp src/ui/hub_settings_state.* src/tasks/interface_self_check_task.* src/ui/interface_self_check_card.* src/ui/diagnostics_panel.cpp tests/ui/windows_speech_settings_card_tests.* tests/ui/api_settings_section_header_tests.* tests/ui/hub_settings_state_tests.cpp tests/tasks/interface_self_check_task_tests.*
git commit -m "feat: add Windows speech settings and diagnostics"
```

### Task 8: Expose Windows speech in function, canvas, and tray selectors

**Files:**

- Modify: `src/ui/function_command_page.cpp`
- Modify: `src/controllers/tray_controller.cpp`
- Modify: `tests/ui/function_command_page_tests.cpp`
- Modify: `tests/ui/function_command_page_tests.pro`
- Modify: `tests/controllers/tray_controller_exit_tests.cpp`
- Modify: `tests/controllers/tray_controller_exit_tests.pro`

- [ ] **Step 1: Write RED selector tests**

In the real function page, find the global speech combo and the flow inspector combo and assert they contain each ID from `supportedSpeechProviderIds()` exactly once. In the tray test, inspect actions by `data()` and assert the same provider set, exclusive checking, current-provider refresh, and a Windows action triggering `setSpeechProvider("windows-local")` exactly once.

```cpp
for (const QString &providerId : supportedSpeechProviderIds()) {
    QCOMPARE(speechBox->findData(providerId) >= 0, true);
    QCOMPARE(countData(speechBox, providerId), 1);
}
```

- [ ] **Step 2: Run both projects and verify RED**

Expected: function popup has only three entries and tray lacks Custom/Windows actions.

- [ ] **Step 3: Replace hard-coded lists with the catalog**

In `function_command_page.cpp`:

```cpp
for (const QString &id : supportedSpeechProviderIds()) {
    speechBox->addItem(speechProviderTitle(id), id);
}
```

In `TrayController`, build an action map:

```cpp
QMap<QString, QAction *> speechActions;
for (const QString &id : supportedSpeechProviderIds()) {
    QAction *action = speechMenu->addAction(speechProviderTitle(id));
    action->setData(id);
    action->setCheckable(true);
    action->setActionGroup(speechGroup);
    speechActions.insert(id, action);
    connect(action, &QAction::triggered, this, [this, id]() {
        if (m_callbacks.setSpeechProvider) m_callbacks.setSpeechProvider(id);
    });
}
```

The `aboutToShow` handler checks the action matching the normalized current ID. The canvas already receives the catalog through `FunctionCommandPage`; preserve its draft-only behavior.

- [ ] **Step 4: Run GREEN and selector regressions**

Run function command page, function canvas editor, tray controller, and hub window header tests. Expected: four providers exactly once at each route and no navigation/callback regression.

- [ ] **Step 5: Commit**

```powershell
git add src/ui/function_command_page.cpp src/controllers/tray_controller.cpp tests/ui/function_command_page_tests.cpp tests/ui/function_command_page_tests.pro tests/controllers/tray_controller_exit_tests.cpp tests/controllers/tray_controller_exit_tests.pro
git commit -m "feat: expose Windows speech across selectors"
```

### Task 9: Register sources and package the helper as a required runtime

**Files:**

- Modify: `vocekit.pro`
- Modify: `scripts/run-all-tests.ps1`
- Modify: `scripts/deploy.ps1`
- Modify: `scripts/package-test.ps1`
- Modify: `scripts/verify-runtime.ps1`
- Modify: `docs/TESTING.md`

- [ ] **Step 1: Add RED deployment assertions before copying the helper**

Add `speech\windows\vocekit-windows-speech.exe` to `verify-runtime.ps1` required files and make it run:

```powershell
$speechHelper = Join-Path $RuntimeDir 'speech\windows\vocekit-windows-speech.exe'
$probeOutput = & $speechHelper --probe --language follow-windows --run-id runtime-verify 2>&1
if ($LASTEXITCODE -ne 0 -or ($probeOutput -join "`n") -notmatch '"type":"probe"') {
    throw "Windows speech helper probe failed.`n$($probeOutput -join "`n")"
}
```

Run runtime verification against the current Release directory. Expected: fail because the speech helper is not deployed.

- [ ] **Step 2: Register all new Qt sources and headers**

Add the Windows protocol/client/provider/streaming session/settings card `.cpp` files to `SOURCES` and matching headers to `HEADERS` in `vocekit.pro`. Run qmake immediately; expected after implementation: no duplicate or missing source entry.

Update `scripts/run-all-tests.ps1` so both `fake_ocr_helper` and `fake_windows_speech_helper` are counted as standalone build-only programs and are not executed with no arguments:

```powershell
if ($target -in @('fake_ocr_helper', 'fake_windows_speech_helper')) {
    ++$standalonePrograms
    continue
}
```

- [ ] **Step 3: Deploy the helper**

Resolve source from `helpers/bin/vocekit-windows-speech.exe`, require it, create `<executableDir>/speech/windows`, and copy it under the stable runtime name. Error messages instruct running `scripts/build-runtime-helpers.ps1`.

- [ ] **Step 4: Extend package verification**

Before archive creation, assert the packaged helper exists and run its `--probe` from the package path. Keep `.pdb`, helper source, build logs, PCM/WAV, and personal config excluded. Do not require the x64 helper PE machine to match the x86 Qt executable; it is a child process, not a loaded DLL. Add this distinction to `verify-runtime.ps1` comments and `docs/TESTING.md`.

- [ ] **Step 5: Build, deploy, and verify from clean paths**

Run:

```powershell
.\scripts\build-runtime-helpers.ps1
& 'D:\QQQQQT0001\5.9\mingw53_32\bin\qmake.exe' vocekit.pro -spec win32-g++ 'CONFIG+=release'
& 'D:\QQQQQT0001\Tools\mingw530_32\bin\mingw32-make.exe' -j2
.\scripts\deploy.ps1 -Configuration release -QtBin 'D:\QQQQQT0001\5.9\mingw53_32\bin' -MingwBin 'D:\QQQQQT0001\Tools\mingw530_32\bin' -OpenSslBin 'D:\QQQQQT0001\Tools\mingw530_32\opt\bin'
.\scripts\package-test.ps1 -PackageName vocekit-windows-speech-test
```

Expected: helper self-test/probe, Qt Release build, runtime verification, package verification, and archive creation all succeed.

- [ ] **Step 6: Commit**

```powershell
git add vocekit.pro scripts/run-all-tests.ps1 scripts/deploy.ps1 scripts/package-test.ps1 scripts/verify-runtime.ps1 docs/TESTING.md
git commit -m "build: package Windows speech helper"
```

### Task 10: Full verification, real recognizer smoke, UI proof, and review

**Files:**

- Modify only files required by failures proven in this task.
- Store generated logs/screenshots under an ignored verification build directory; do not commit binaries, recordings, credentials, or personal settings.

- [ ] **Step 1: Run the helper's real zh-CN and en-US smoke checks**

Generate disposable audio inside the verification build directory with the installed `System.Speech.Synthesis.SpeechSynthesizer` voices: Microsoft Huihui Desktop (`zh-CN`) says “这是本地语音识别测试”, and Microsoft Zira or David Desktop (`en-US`) says “this is a local speech recognition test”. Convert each WAV payload to 16 kHz mono 16-bit PCM with a documented temporary conversion step and feed it to batch mode. Assert each probe resolves the requested language and each batch call produces a terminal `final` or stable `NO_SPEECH` event; exact acoustic wording is informational, not a hard assertion. Confirm the helper creates no network connections, delete the disposable samples after evidence capture, and leave no helper process behind.

- [ ] **Step 2: Run the complete Qt Release suite**

```powershell
.\scripts\run-all-tests.ps1 -Configuration release
```

Expected final summary: `Failed=0`, `Skipped=0`, and `InfrastructureFailures=0`. If the isolated environment lacks existing ignored OCR assets, stage only those assets through a temporary verified path and remove that path in `finally`; never treat missing infrastructure as a source pass.

- [ ] **Step 3: Run the clean Release/package launch check**

Launch the packaged `vocekit.exe` with a PATH containing only Windows system directories. Verify the main window opens, choose Windows local, run the settings probe, begin/cancel one recording, and confirm `vocekit-windows-speech.exe` exits. Then begin/confirm one recording and verify final/no-speech handling without cloud credentials.

- [ ] **Step 4: Capture real widget visual evidence**

Render and inspect:

- API settings Windows card at normal and 150% font scale;
- C floating bar with long committed Chinese plus blue provisional Chinese;
- C floating bar with long English;
- B compact recording, finalizing, fallback, and failure states;
- missing-language message with Test and Open Settings buttons.

At 100%, 125%, and 150% scaling, confirm no Chinese/English clipping, overlap, inaccessible cross/check buttons, unbounded growth, or focus stealing.

- [ ] **Step 5: Audit privacy, process lifecycle, and exactly-once evidence**

Search logs and package files to confirm no complete transcript, raw PCM, credential, signed URL, or personal recording was included. Use Process Explorer/PowerShell to confirm zero residual helper processes after success, timeout, cancel, settings close, main-window close, and application exit. Re-run the controller tests that assert one final downstream call and zero calls after cancel/late messages.

- [ ] **Step 6: Run diff and repository hygiene checks**

```powershell
git diff --check
git status --short
git log --oneline --decorate -12
```

Expected: no staged/uncommitted tracked change after fixes are committed. Preserve and report pre-existing untracked build artifacts rather than deleting user data.

- [ ] **Step 7: Request a final code review and fix proven findings**

Review against the design sections for local-only routing, language resolution, QProcess limits, cancellation, exactly-once delivery, UI clipping, helper packaging, and Qt 5.9/C++11 compatibility. For every accepted finding, add a failing regression test, implement the smallest correction, rerun the affected suite, then commit:

```powershell
git add src providers tests scripts vocekit.pro docs/TESTING.md
git commit -m "fix: harden Windows local speech recognition"
```

Before committing, inspect `git diff --cached --name-only` and unstage every path unrelated to an accepted review finding; the command is a starting set, not permission to include generated or unrelated files.

- [ ] **Step 8: Produce the delivery report**

Report commit IDs, helper and main executable hashes, exact full-suite totals, real probe results, package path, visual evidence paths, process-cleanup evidence, any environment-only concern, and a concise user flow: Settings → Interfaces → Windows local → language → Test → select provider → record.

---

## Specification coverage audit

- Independent System.Speech helper, raw-PCM stdin, EOF confirm, terminate cancel: Tasks 2, 3, and 5.
- One process per recording, bounded queues, run-ID gates, timeouts, cleanup: Tasks 3, 5, 6, and 10.
- Provisional/committed/final behavior and B/C floating presentation: Tasks 5, 6, and 10.
- Same Windows-engine batch fallback without cloud routing: Tasks 4 and 6.
- Follow Windows / zh-CN / en-US language semantics and missing-component UX: Tasks 1, 2, and 7.
- Interface, function, canvas, and tray selection consistency: Tasks 7 and 8.
- No API keys and deterministic self-check: Tasks 4 and 7.
- Qt 5.9/C++11 plus .NET Framework helper build and x86/x64 process boundary: Tasks 2, 5, and 9.
- Deployment, probe, clean-PATH package, privacy, real language smoke, visual scaling, and full suite: Tasks 9 and 10.
