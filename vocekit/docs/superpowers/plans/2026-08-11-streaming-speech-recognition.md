# Streaming Speech Recognition Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add true during-recording streaming transcription for Xfyun and Baidu, show provisional/corrected text in the existing floating bar, and run the existing downstream workflow exactly once after a non-empty final transcript.

**Architecture:** Keep the existing synchronous `ISpeechProvider::recognize(...)` path as batch fallback. Add a callback-driven streaming session boundary, provider-specific Xfyun/Baidu sessions over a new live WebSocket transport, a pure transcript accumulator, and a non-blocking PCM tap from the recorder. `VoiceRecordingWorkflowController` owns one session per recording, gates callbacks by operation generation, and is the only component allowed to hand a final transcript to downstream processing.

**Tech Stack:** C++11, Qt 5.9 Core/Widgets/Multimedia/Network/WebSockets, qmake, MinGW 5.3, QtTest.

**Design:** `docs/superpowers/specs/2026-08-11-streaming-speech-recognition-design.md`

**Protocol references:**

- Xfyun WebSocket IAT: <https://www.xfyun.cn/doc/asr/voicedictation/API.html>
- Baidu real-time ASR WebSocket: <https://cloud.baidu.com/doc/SPEECH/s/jlbxejt2i>

---

## File map

### New production files

- `src/providers/streaming_speech_session.h` — provider-neutral request, snapshot, callbacks, state, and session interface.
- `src/providers/streaming_transcript_accumulator.h/.cpp` — pure committed/provisional transcript reducer.
- `src/providers/provider_streaming_websocket_transport.h/.cpp` — live text/binary WebSocket transport with injectable callbacks.
- `src/providers/xfyun_speech_protocol.h/.cpp` — shared Xfyun signing, request-frame creation, and response parsing used by batch and live paths.
- `src/providers/xfyun_streaming_speech_session.h/.cpp` — 40 ms Xfyun streaming pump, `wpgs` correction, and 55-second rotation.
- `src/providers/baidu_streaming_speech_session.h/.cpp` — Baidu START/binary/FINISH/CANCEL state machine and MID/FIN reducer.
- `src/providers/streaming_speech_session_factory.h/.cpp` — provider selection and configuration gating.

### Modified production files

- `src/config/app_settings_data.h`, `src/config/app_settings_json.cpp` — persisted `streamingSpeechRecognitionEnabled`, default true.
- `src/ui/basic_settings_section.h/.cpp`, `src/ui/settings_panel.cpp` — Voice settings toggle and snapshot wiring.
- `src/ui/api_settings_section.cpp` — explain that Baidu AppID is required for real-time mode but optional for batch REST.
- `src/recording/audio_recorder_legacy.h`, `src/recording/voice_audio_recorder_adapter.cpp`, `src/recording/voice_recording_capture.h` — live PCM listener that never blocks file writing.
- `src/controllers/voice_controller.cpp` — supply the production streaming-session factory to the recording workflow.
- `src/controllers/voice_recording_workflow_controller.h/.cpp` — create/finish/cancel/degrade sessions, long-recording behavior, five-second final timeout, batch fallback, and exactly-once downstream gate.
- `src/ui/floating_bar.h`, `src/ui/floating_bar_test_card.cpp` — expandable three-line committed/provisional preview and diagnostic states.
- `src/providers/xfyun_speech_provider.cpp` — use shared Xfyun protocol helpers without changing batch behavior.
- `vocekit.pro` — register new production files.

### Tests

- Create `tests/providers/streaming_transcript_accumulator_tests.cpp/.pro`.
- Create `tests/providers/provider_streaming_websocket_transport_tests.cpp/.pro`.
- Create `tests/providers/xfyun_streaming_speech_session_tests.cpp/.pro`.
- Create `tests/providers/baidu_streaming_speech_session_tests.cpp/.pro`.
- Create `tests/recording/voice_audio_recorder_stream_tests.cpp/.pro`.
- Create `tests/ui/floating_bar_streaming_tests.cpp/.pro`.
- Modify `tests/config/app_settings_json_tests.cpp`.
- Create `tests/ui/basic_settings_section_tests.cpp/.pro`.
- Modify `tests/controllers/voice_recording_workflow_controller_tests.cpp/.pro`.
- Modify provider test `.pro` files that compile `xfyun_speech_provider.cpp` so the shared protocol implementation is linked.

---

### Task 1: Persist and expose the real-time recognition switch

**Files:**

- Modify: `src/config/app_settings_data.h`
- Modify: `src/config/app_settings_json.cpp`
- Modify: `src/ui/basic_settings_section.h`
- Modify: `src/ui/basic_settings_section.cpp`
- Modify: `src/ui/settings_panel.cpp`
- Modify: `src/ui/api_settings_section.cpp`
- Test: `tests/config/app_settings_json_tests.cpp`
- Create: `tests/ui/basic_settings_section_tests.cpp`
- Create: `tests/ui/basic_settings_section_tests.pro`

- [ ] **Step 1: Write failing persistence tests**

Add QtTest cases that prove missing JSON defaults to enabled and explicit false round-trips:

```cpp
void missingStreamingSpeechSettingDefaultsToEnabled()
{
    const AppSettingsData restored = appSettingsFromJson(QJsonObject());
    QVERIFY(restored.streamingSpeechRecognitionEnabled);
}

void streamingSpeechSettingRoundTripsFalse()
{
    AppSettingsData original;
    original.streamingSpeechRecognitionEnabled = false;
    const AppSettingsData restored = appSettingsFromJson(appSettingsToJson(original));
    QVERIFY(!restored.streamingSpeechRecognitionEnabled);
}
```

- [ ] **Step 2: Write a failing real-widget toggle test**

Instantiate `BasicSettingsSection(BasicSettingsSection::Voice, callbacks)`, find `QCheckBox#streamingSpeechRecognitionToggle`, click it, and assert `applySnapshot` receives false and `saveAndRefresh` runs once.

```cpp
QCheckBox *toggle = section.findChild<QCheckBox *>(
    QStringLiteral("streamingSpeechRecognitionToggle")
);
QVERIFY(toggle);
QVERIFY(toggle->isChecked());
QTest::mouseClick(toggle, Qt::LeftButton);
QVERIFY(!applied.streamingSpeechRecognitionEnabled);
QCOMPARE(saveCount, 1);
```

- [ ] **Step 3: Run both tests and verify RED**

Run the two QtTest projects with Qt 5.9 Release. Expected: compile failure because the setting and UI object do not exist.

- [ ] **Step 4: Add the setting and UI wiring**

Add the same defaulted field to both data types:

```cpp
bool streamingSpeechRecognitionEnabled = true;
```

Read and write JSON under `streamingSpeechRecognitionEnabled`, with `toBool(true)` for legacy files. Add a Voice settings row:

```cpp
QWidget *streamingRow = toggleRow(
    bssTr8("实时识别"),
    bssTr8("开启后，讯飞和百度会在录音时实时显示文字；不可用时自动使用停止后识别。"),
    current.streamingSpeechRecognitionEnabled,
    [this](bool enabled) {
        BasicSettingsSnapshot next = snapshot();
        next.streamingSpeechRecognitionEnabled = enabled;
        applyAndRefresh(next);
    }
);
streamingRow->findChild<QCheckBox *>()->setObjectName(
    QStringLiteral("streamingSpeechRecognitionToggle")
);
layout->addWidget(streamingRow);
```

Wire `SettingsPanel` snapshot/apply both directions. Change the Baidu AppID help text to “实时识别必填；仅使用整段识别时可留空” without making save reject an empty AppID.

- [ ] **Step 5: Run GREEN and regression tests**

Run `app_settings_json_tests`, `basic_settings_section_tests`, `api_settings_section_header_tests`, and `secret_config_tests`. Expected: all pass.

- [ ] **Step 6: Commit**

```powershell
git add src/config/app_settings_data.h src/config/app_settings_json.cpp src/ui/basic_settings_section.h src/ui/basic_settings_section.cpp src/ui/settings_panel.cpp src/ui/api_settings_section.cpp tests/config/app_settings_json_tests.cpp tests/ui/basic_settings_section_tests.cpp tests/ui/basic_settings_section_tests.pro
git commit -m "feat: add streaming speech setting"
```

### Task 2: Add provider-neutral streaming types and transcript reduction

**Files:**

- Create: `src/providers/streaming_speech_session.h`
- Create: `src/providers/streaming_transcript_accumulator.h`
- Create: `src/providers/streaming_transcript_accumulator.cpp`
- Create: `tests/providers/streaming_transcript_accumulator_tests.cpp`
- Create: `tests/providers/streaming_transcript_accumulator_tests.pro`

- [ ] **Step 1: Write failing reducer tests**

Cover append, one-based inclusive Xfyun range replacement, Baidu provisional replacement/commit, rotation sealing, invalid ranges, and snapshot monotonicity:

```cpp
StreamingTranscriptAccumulator text;
text.appendCommitted(1, QString::fromUtf8("今天"));
text.appendCommitted(2, QString::fromUtf8("天气"));
text.replaceCommittedRange(1, 2, 3, QString::fromUtf8("今天天气很好"));
QCOMPARE(text.snapshot().committedText, QString::fromUtf8("今天天气很好"));

text.setProvisional(QString::fromUtf8("北京天气怎"));
QCOMPARE(text.snapshot().provisionalText, QString::fromUtf8("北京天气怎"));
text.commitProvisional(QString::fromUtf8("北京天气怎么样"));
QCOMPARE(text.snapshot().committedText, QString::fromUtf8("北京天气怎么样"));
QVERIFY(text.snapshot().provisionalText.isEmpty());
```

- [ ] **Step 2: Run RED**

Expected: compile failure because the types do not exist.

- [ ] **Step 3: Define exact provider-neutral contracts**

Create the interface with callback snapshots rather than provider deltas:

```cpp
enum class StreamingSpeechState { Idle, Connecting, Streaming, Finalizing, Completed, Degraded, Cancelled };

struct StreamingTranscriptSnapshot {
    quint64 revision = 0;
    QString committedText;
    QString provisionalText;
    QString displayText() const { return committedText + provisionalText; }
};

struct StreamingSpeechSessionRequest {
    QString provider;
    QString networkPolicy = QStringLiteral("inherit");
    bool useSystemProxy = false;
    int sampleRate = 16000;
    int channelCount = 1;
    int sampleSizeBits = 16;
};

struct StreamingSpeechCallbacks {
    std::function<void(const StreamingTranscriptSnapshot &)> transcriptUpdated;
    std::function<void(const QString &)> degraded;
    std::function<void(const QString &)> completed;
};

class IStreamingSpeechSession {
public:
    virtual ~IStreamingSpeechSession() {}
    virtual bool start(QString *error) = 0;
    virtual bool pushAudio(const QByteArray &pcm) = 0;
    virtual void finish() = 0;
    virtual void cancel() = 0;
    virtual StreamingSpeechState state() const = 0;
};
```

Implement the accumulator with a `QMap<int, QString>` for Xfyun numbered pieces, a sealed prefix for rotated sessions, and an independent provisional string. Every mutation increments `revision`; invalid replacement ranges return false and leave state unchanged.

- [ ] **Step 4: Run GREEN**

Expected: all reducer cases pass with no Qt network dependency.

- [ ] **Step 5: Commit**

```powershell
git add src/providers/streaming_speech_session.h src/providers/streaming_transcript_accumulator.h src/providers/streaming_transcript_accumulator.cpp tests/providers/streaming_transcript_accumulator_tests.cpp tests/providers/streaming_transcript_accumulator_tests.pro
git commit -m "feat: add streaming transcript reducer"
```

### Task 3: Add an asynchronous live WebSocket transport

**Files:**

- Create: `src/providers/provider_streaming_websocket_transport.h`
- Create: `src/providers/provider_streaming_websocket_transport.cpp`
- Create: `tests/providers/provider_streaming_websocket_transport_tests.cpp`
- Create: `tests/providers/provider_streaming_websocket_transport_tests.pro`

- [ ] **Step 1: Write failing lifecycle and frame tests**

Use a local `QWebSocketServer` on loopback. Assert open callback, text and binary sends, incoming text callback, normal close, cancellation, proxy/network option preservation, and a single terminal callback.

```cpp
transport->open(url, options, callbacks);
QTRY_COMPARE(openedCount, 1);
transport->sendText(QByteArrayLiteral("{\"type\":\"START\"}"));
transport->sendBinary(QByteArray::fromHex("00010203"));
QTRY_COMPARE(serverTextFrames.size(), 1);
QTRY_COMPARE(serverBinaryFrames.size(), 1);
transport->cancel();
QTRY_COMPARE(cancelledCount, 1);
QCOMPARE(failedCount, 0);
```

- [ ] **Step 2: Run RED**

Expected: compile failure because the transport does not exist.

- [ ] **Step 3: Implement the transport boundary**

Define one connection per transport instance:

```cpp
struct ProviderStreamingWebSocketCallbacks {
    std::function<void()> opened;
    std::function<void(const QByteArray &)> textMessage;
    std::function<void(const OperationError &)> failed;
    std::function<void(bool cancelled)> closed;
};

class IProviderStreamingWebSocketTransport {
public:
    virtual ~IProviderStreamingWebSocketTransport() {}
    virtual void open(const QUrl &, const NetworkRequestOptions &, const ProviderStreamingWebSocketCallbacks &) = 0;
    virtual void sendText(const QByteArray &) = 0;
    virtual void sendBinary(const QByteArray &) = 0;
    virtual void closeNormally() = 0;
    virtual void cancel() = 0;
};
```

The production implementation wraps `QWebSocket`, connects signals before `open()`, applies the same system/direct proxy policy as the existing provider transport, maps socket/SSL failures to `OperationError`, and uses a terminal flag so `failed`/`closed` cannot fire twice.

- [ ] **Step 4: Run GREEN and existing WebSocket regression**

Run the new transport test and `xfyun_speech_provider_tests`. Expected: all pass.

- [ ] **Step 5: Commit**

```powershell
git add src/providers/provider_streaming_websocket_transport.h src/providers/provider_streaming_websocket_transport.cpp tests/providers/provider_streaming_websocket_transport_tests.cpp tests/providers/provider_streaming_websocket_transport_tests.pro
git commit -m "feat: add live websocket transport"
```

### Task 4: Implement Xfyun dynamic-correction streaming

**Files:**

- Create: `src/providers/xfyun_speech_protocol.h`
- Create: `src/providers/xfyun_speech_protocol.cpp`
- Create: `src/providers/xfyun_streaming_speech_session.h`
- Create: `src/providers/xfyun_streaming_speech_session.cpp`
- Modify: `src/providers/xfyun_speech_provider.cpp`
- Create: `tests/providers/xfyun_streaming_speech_session_tests.cpp`
- Create: `tests/providers/xfyun_streaming_speech_session_tests.pro`
- Modify: `tests/providers/xfyun_speech_provider_tests.pro`
- Modify: every test `.pro` that directly compiles `xfyun_speech_provider.cpp`

- [ ] **Step 1: Write failing Xfyun session tests**

Inject a fake live transport and fake timers/clock. Cover signed URL, first frame with `dwa=wpgs`, 1280-byte/40 ms frames, status 0/1/2, `apd`, `rpl` with `rg`, nonzero code degradation, queue overflow, finish, cancel, and 55-second rotation with sealed text.

```cpp
session.start(&error);
fake->emitOpened();
session.pushAudio(QByteArray(2560, char(7)));
clock.advance(40);
QCOMPARE(frameStatus(fake->textFrames.at(0)), 0);
QCOMPARE(frameAudioSize(fake->textFrames.at(0)), 1280);
QCOMPARE(frameBusiness(fake->textFrames.at(0)).value("dwa").toString(), QStringLiteral("wpgs"));

fake->emitText(xfyunResult(2, "rpl", qMakePair(1, 2), QString::fromUtf8("修正结果")));
QCOMPARE(lastSnapshot.displayText(), QString::fromUtf8("修正结果"));
```

- [ ] **Step 2: Run RED**

Expected: compile failure because protocol/session files do not exist.

- [ ] **Step 3: Extract shared Xfyun protocol helpers**

Move signing and frame construction out of the batch provider without changing its public API:

```cpp
QUrl xfyunSignedIatUrl(const SecretConfig &, const QDateTime &utcNow);
QByteArray xfyunAudioFrame(const SecretConfig &, int status, const QByteArray &audio, int sampleRate, bool dynamicCorrection);
XfyunRecognitionEvent parseXfyunRecognitionEvent(const QByteArray &message);
```

Keep existing batch tests green before adding session behavior.

- [ ] **Step 4: Implement the streaming state machine**

The session stores at most 64,000 PCM bytes (two seconds at 16 kHz mono 16-bit). A 40 ms timer sends exactly one 1280-byte frame. `finish()` drains queued audio, sends status 2, then waits for server data status 2. At 55 seconds it sends status 2, seals the accumulator when the server completes, opens a new signed URL, and sends buffered PCM as the new status-0 frame. Any handshake, parse, code, or overflow error calls `degraded` once.

- [ ] **Step 5: Run GREEN and batch regressions**

Run `xfyun_streaming_speech_session_tests`, `xfyun_speech_provider_tests`, `built_in_provider_factory_tests`, and `interface_self_check_task_tests`. Expected: all pass; old batch request output remains unchanged except shared helper placement.

- [ ] **Step 6: Commit**

```powershell
git add src/providers/xfyun_speech_protocol.h src/providers/xfyun_speech_protocol.cpp src/providers/xfyun_streaming_speech_session.h src/providers/xfyun_streaming_speech_session.cpp src/providers/xfyun_speech_provider.cpp tests/providers/xfyun_streaming_speech_session_tests.cpp tests/providers/xfyun_streaming_speech_session_tests.pro tests/providers/xfyun_speech_provider_tests.pro tests/providers/built_in_provider_factory_tests.pro tests/tasks/interface_self_check_task_tests.pro tests/tasks/model_request_task_tests.pro tests/controllers/function_flow_runtime_adapters_tests.pro
git commit -m "feat: stream xfyun speech corrections"
```

### Task 5: Implement Baidu real-time streaming and provider factory gating

**Files:**

- Create: `src/providers/baidu_streaming_speech_session.h`
- Create: `src/providers/baidu_streaming_speech_session.cpp`
- Create: `src/providers/streaming_speech_session_factory.h`
- Create: `src/providers/streaming_speech_session_factory.cpp`
- Create: `tests/providers/baidu_streaming_speech_session_tests.cpp`
- Create: `tests/providers/baidu_streaming_speech_session_tests.pro`

- [ ] **Step 1: Write failing Baidu protocol tests**

Cover the `wss://vop.baidu.com/realtime_asr?sn=...` URL, START JSON, `appid` integer conversion, `appkey`, `dev_pid=15372`, `cuid`, PCM/16000, 5120-byte binary frames at 160 ms, MID replacement, FIN commit, HEARTBEAT ignore, FINISH, CANCEL, nonzero `err_no`, overflow, and exactly one terminal callback.

```cpp
fake->emitOpened();
const QJsonObject start = QJsonDocument::fromJson(fake->textFrames.first()).object();
QCOMPARE(start.value("type").toString(), QStringLiteral("START"));
QCOMPARE(start.value("data").toObject().value("dev_pid").toInt(), 15372);
QCOMPARE(start.value("data").toObject().value("sample").toInt(), 16000);

fake->emitText(baiduResult("MID_TEXT", QString::fromUtf8("北京天气怎")));
fake->emitText(baiduResult("FIN_TEXT", QString::fromUtf8("北京天气怎么样")));
QCOMPARE(lastSnapshot.committedText, QString::fromUtf8("北京天气怎么样"));
QVERIFY(lastSnapshot.provisionalText.isEmpty());
```

- [ ] **Step 2: Write failing factory tests**

Assert Xfyun requires AppID/API Key/API Secret; Baidu live requires AppID/API Key; missing Baidu AppID returns a documented unavailable result rather than constructing a socket; custom/unknown providers remain batch-only.

- [ ] **Step 3: Run RED**

Expected: compile failure because Baidu session/factory do not exist.

- [ ] **Step 4: Implement Baidu session**

On open, send:

```json
{"type":"START","data":{"appid":10500017,"appkey":"key","dev_pid":15372,"cuid":"vocekit-desktop","format":"pcm","sample":16000}}
```

Aggregate PCM into 5120-byte frames and send one per 160 ms. `MID_TEXT` calls `setProvisional(result)`; successful `FIN_TEXT` calls `commitProvisional(result)`. `finish()` drains audio then sends `{"type":"FINISH"}`. `cancel()` sends `{"type":"CANCEL"}` and closes. A nonzero `err_no` degrades the session; HEARTBEAT is ignored.

- [ ] **Step 5: Implement the default factory**

Return a structured decision:

```cpp
struct StreamingSpeechSessionCreation {
    QSharedPointer<IStreamingSpeechSession> session;
    QString unavailableReason;
};

StreamingSpeechSessionCreation createStreamingSpeechSession(
    const StreamingSpeechSessionRequest &request,
    const StreamingSpeechCallbacks &callbacks,
    const SecretConfig &secrets
);
StreamingSpeechSessionCreation createDefaultStreamingSpeechSession(
    const StreamingSpeechSessionRequest &request,
    const StreamingSpeechCallbacks &callbacks
);
```

The explicit overload is deterministic for tests; the default overload loads secrets once and delegates. Do not log or return secret values. Missing configuration, including a non-numeric Baidu AppID, produces no network object and allows the controller to enter batch-only mode.

- [ ] **Step 6: Run GREEN and Baidu batch regression**

Run `baidu_streaming_speech_session_tests`, `baidu_speech_provider_tests`, `secret_config_tests`, and factory tests. Expected: all pass; REST behavior remains unchanged.

- [ ] **Step 7: Commit**

```powershell
git add src/providers/baidu_streaming_speech_session.h src/providers/baidu_streaming_speech_session.cpp src/providers/streaming_speech_session_factory.h src/providers/streaming_speech_session_factory.cpp tests/providers/baidu_streaming_speech_session_tests.cpp tests/providers/baidu_streaming_speech_session_tests.pro
git commit -m "feat: stream baidu speech results"
```

### Task 6: Expose live PCM without changing recorded audio

**Files:**

- Modify: `src/recording/audio_recorder_legacy.h`
- Modify: `src/recording/voice_audio_recorder_adapter.cpp`
- Modify: `src/recording/voice_recording_capture.h`
- Create: `tests/recording/voice_audio_recorder_stream_tests.cpp`
- Create: `tests/recording/voice_audio_recorder_stream_tests.pro`

- [ ] **Step 1: Write failing capture-device tests**

Construct `AudioCaptureDevice` with a temporary file and listener, call `write()` from the test, and assert the file bytes and listener bytes are identical. Replace and clear the listener; assert no stale callback. Provider session tests separately prove `pushAudio()` only appends under a short mutex and returns without performing socket I/O.

```cpp
QByteArray observed;
capture.setPcmListener([&observed](const QByteArray &pcm) { observed += pcm; });
QCOMPARE(capture.write(QByteArray::fromHex("00010203")), qint64(4));
QCOMPARE(observed, QByteArray::fromHex("00010203"));
file.seek(0);
QCOMPARE(file.readAll(), observed);
```

- [ ] **Step 2: Run RED**

Expected: compile failure because the listener API does not exist.

- [ ] **Step 3: Add the listener through all recording boundaries**

Add to capture handlers:

```cpp
std::function<void(const std::function<void(const QByteArray &)> &)> setPcmListener;
```

`AudioCaptureDevice::writeData` must write the file first, then copy exactly the accepted bytes to the listener. `VoiceAudioRecorderAdapter::Impl` stores the listener and applies it to every newly created `AudioCaptureDevice`, including long-recording segment rotation. Clearing uses an empty `std::function`.

- [ ] **Step 4: Run GREEN and recorder regressions**

Run `voice_audio_recorder_stream_tests`, `voice_recording_capture_tests`, `voice_recording_coordinator_tests`, and `voice_recording_workflow_controller_tests`. Expected: identical existing PCM/WAV behavior.

- [ ] **Step 5: Commit**

```powershell
git add src/recording/audio_recorder_legacy.h src/recording/voice_audio_recorder_adapter.cpp src/recording/voice_recording_capture.h tests/recording/voice_audio_recorder_stream_tests.cpp tests/recording/voice_audio_recorder_stream_tests.pro
git commit -m "feat: expose live recorder pcm"
```

### Task 7: Orchestrate streaming, timeout, cancellation, long recordings, and batch fallback

**Files:**

- Modify: `src/controllers/voice_recording_workflow_controller.h`
- Modify: `src/controllers/voice_recording_workflow_controller.cpp`
- Modify: `src/controllers/voice_controller.cpp`
- Modify: `tests/controllers/voice_recording_workflow_controller_tests.cpp`
- Modify: `tests/controllers/voice_recording_workflow_controller_tests.pro`

- [ ] **Step 1: Extend fakes and write failing controller tests**

Add a fake session and injected factory to `VoiceRecordingWorkflowAccess`. Cover:

1. enabled Xfyun/Baidu starts one session and forwards PCM;
2. disabled or unavailable starts batch-only;
3. snapshots update only the preview;
4. successful non-empty final skips batch and calls downstream once;
5. empty final, degraded, queue overflow, or five-second timeout runs batch once;
6. late callbacks after fallback/cancel are ignored;
7. cancellation closes the session and never calls batch/downstream;
8. long recording does not schedule per-segment recognition while streaming succeeds;
9. degraded long recording recognizes all saved segments through the existing coordinator;
10. flow and classic paths preserve their existing result payloads.

```cpp
QCOMPARE(fakeSession->startCount, 1);
recorder.emitPcm(QByteArrayLiteral("live"));
QCOMPARE(fakeSession->pushedAudio, QList<QByteArray>() << QByteArrayLiteral("live"));
fakeSession->emitCompleted(QStringLiteral("final text"));
QCOMPARE(batchRecognitionCount, 0);
QCOMPARE(processedTexts, QStringList() << QStringLiteral("final text"));
fakeSession->emitCompleted(QStringLiteral("late duplicate"));
QCOMPARE(processedTexts.size(), 1);
```

- [ ] **Step 2: Run RED**

Expected: compile failure because controller streaming access/state do not exist.

- [ ] **Step 3: Add injectable creation and session lifecycle**

Add to `VoiceRecordingWorkflowAccess`:

```cpp
std::function<StreamingSpeechSessionCreation(
    const StreamingSpeechSessionRequest &,
    const StreamingSpeechCallbacks &
)> createStreamingSpeechSession;
```

`VoiceController` supplies a factory callback that delegates to `createDefaultStreamingSpeechSession(...)`. Controller unit tests inject a fake factory; if no factory is supplied the workflow deliberately stays batch-only, so the controller test target does not need to link real Provider implementations. At recording start, create a session only when the setting is enabled and provider is Xfyun/Baidu, using the frozen flow provider/network policy for a canvas run and current settings for a classic run. Register the PCM listener before capture begins and clear it during every stop/cancel/destructor path. Every callback captures `generation` and posts to the controller thread before reading controller state.

- [ ] **Step 4: Implement exactly-once completion and fallback**

Use an explicit per-recording state and one final gate:

```cpp
void completeStreamingOnce(quint64 generation, const QString &text)
{
    if (generation != m_operationGeneration || m_streamingTerminalHandled) return;
    const QString finalText = text.trimmed();
    if (finalText.isEmpty()) { beginBatchFallback(); return; }
    m_streamingTerminalHandled = true;
    processRecognizedSpeech(m_modeId, finalText);
}
```

`stop` calls `session->finish()`, displays finalizing state, and arms a single-shot 5000 ms timer. Degrade/timeout/empty final enters fallback once. Cancel stops the timer, clears the PCM listener, calls `session->cancel()`, and sets the terminal gate without fallback.

- [ ] **Step 5: Integrate long recordings**

Continue rotating and saving WAV/PCM segments. While the stream is healthy, do not call `startNextSegmentRecognition()`. On success, build the existing long-recording audio metadata, save the complete audio, and use the stream final text. On degrade or timeout, schedule recognition for every captured segment through the existing `VoiceLongRecordingRecognitionCoordinator`, then reuse `finishLongRecordingRecognition()`.

- [ ] **Step 6: Run GREEN plus controller/task regressions**

Run `voice_recording_workflow_controller_tests`, `voice_speech_recognition_executor_tests`, all long-recording task tests, and flow runtime adapter tests. Expected: all pass; each success path has one downstream call.

- [ ] **Step 7: Commit**

```powershell
git add src/controllers/voice_controller.cpp src/controllers/voice_recording_workflow_controller.h src/controllers/voice_recording_workflow_controller.cpp tests/controllers/voice_recording_workflow_controller_tests.cpp tests/controllers/voice_recording_workflow_controller_tests.pro
git commit -m "feat: orchestrate streaming speech workflow"
```

### Task 8: Expand the floating bar for committed and provisional text

**Files:**

- Modify: `src/ui/floating_bar.h`
- Modify: `src/ui/floating_bar_test_card.cpp`
- Create: `tests/ui/floating_bar_streaming_tests.cpp`
- Create: `tests/ui/floating_bar_streaming_tests.pro`

- [ ] **Step 1: Write failing real-widget tests**

Instantiate the actual `FloatingBar` offscreen and assert:

- base size remains 720x76 when preview is clear;
- `setStreamingTranscript(committed, provisional)` expands height and exposes object names;
- committed text uses normal color and provisional text uses blue;
- display is read-only, wraps, and is capped at three lines;
- `setStreamingFinalizing()` and `setStreamingFallback()` show exact status text;
- `clearStreamingTranscript()` returns to base height;
- 150% font and long Chinese/English do not clip the existing action buttons or waveform; stop/cancel keyboard handling remains available through the controller.

```cpp
bar.setStreamingTranscript(QString::fromUtf8("已经确认，"), QString::fromUtf8("正在识别"));
QVERIFY(bar.height() > 76);
QCOMPARE(bar.findChild<QLabel *>("streamingCommittedText")->text(), QString::fromUtf8("已经确认，"));
QCOMPARE(bar.findChild<QLabel *>("streamingProvisionalText")->text(), QString::fromUtf8("正在识别"));
bar.clearStreamingTranscript();
QCOMPARE(bar.size(), QSize(720, 76));
```

- [ ] **Step 2: Run RED**

Expected: compile failure because streaming UI methods do not exist.

- [ ] **Step 3: Implement the expandable layout**

Replace the fixed root geometry with a vertical outer layout while preserving the existing 720x76 header row. Add a preview frame below it, maximum three font lines, and two labels with object names `streamingCommittedText` and `streamingProvisionalText`. Use `setFixedWidth(720)` and two explicit heights rather than an unbounded `adjustSize()`. Preserve drag hit targets, waveform, saved-position clamping, copy/undo/retry behavior, and keep action buttons visible.

Expose:

```cpp
void setStreamingTranscript(const QString &committed, const QString &provisional);
void setStreamingFinalizing();
void setStreamingFallback();
void clearStreamingTranscript();
```

Coalesce controller updates to the newest revision at no more than one UI update per 50 ms.

- [ ] **Step 4: Update diagnostic preview and run GREEN**

Add recording-stream, correction, finalizing, and fallback choices to `FloatingBarTestCard`. Run `floating_bar_streaming_tests` offscreen at normal and enlarged font. Expected: all geometry and text assertions pass.

- [ ] **Step 5: Commit**

```powershell
git add src/ui/floating_bar.h src/ui/floating_bar_test_card.cpp tests/ui/floating_bar_streaming_tests.cpp tests/ui/floating_bar_streaming_tests.pro
git commit -m "feat: show streaming text in floating bar"
```

### Task 9: Register files, verify the complete product, and package a runnable build

**Files:**

- Modify: `vocekit.pro`
- Modify: affected test `.pro` files only as required by direct source compilation.
- Verification output: `build/streaming-speech-release/`

- [ ] **Step 1: Register every new source/header**

Add all new production `.cpp` files to `SOURCES` and `.h` files to `HEADERS`. Add `websockets` only to test projects that compile the live transport/session. Ensure every test that compiles `xfyun_speech_provider.cpp` also compiles `xfyun_speech_protocol.cpp`.

- [ ] **Step 2: Run focused tests from fresh build directories**

Use:

```powershell
$env:PATH='D:\QQQQQT0001\5.9\mingw53_32\bin;D:\QQQQQT0001\Tools\mingw530_32\bin;' + $env:PATH
$focusedProjects = @(
    'tests/providers/streaming_transcript_accumulator_tests.pro',
    'tests/providers/provider_streaming_websocket_transport_tests.pro',
    'tests/providers/xfyun_streaming_speech_session_tests.pro',
    'tests/providers/baidu_streaming_speech_session_tests.pro',
    'tests/recording/voice_audio_recorder_stream_tests.pro',
    'tests/controllers/voice_recording_workflow_controller_tests.pro',
    'tests/ui/basic_settings_section_tests.pro',
    'tests/ui/floating_bar_streaming_tests.pro'
)
foreach ($project in $focusedProjects) {
    $name = [IO.Path]::GetFileNameWithoutExtension($project)
    $dir = Join-Path 'build/streaming-speech-tests' $name
    New-Item -ItemType Directory -Force $dir | Out-Null
    Push-Location $dir
    & 'D:\QQQQQT0001\5.9\mingw53_32\bin\qmake.exe' (Resolve-Path "../../../$project") -spec win32-g++ CONFIG+=release
    & 'D:\QQQQQT0001\Tools\mingw530_32\bin\mingw32-make.exe' -j2
    $env:QT_QPA_PLATFORM='offscreen'
    & (Join-Path (Get-Location) "release/$name.exe") -maxwarnings 0
    Pop-Location
}
```

Expected: all new tests and directly affected regressions pass with zero failures and no real network calls.

- [ ] **Step 3: Run the full suite**

```powershell
powershell -ExecutionPolicy Bypass -File scripts/run-all-tests.ps1 -Configuration release
```

Expected: `Failed=0`, `Skipped=0`, `InfrastructureFailures=0`.

- [ ] **Step 4: Build the application from a clean Release directory**

```powershell
New-Item -ItemType Directory -Force build/streaming-speech-release | Out-Null
Push-Location build/streaming-speech-release
& 'D:\QQQQQT0001\5.9\mingw53_32\bin\qmake.exe' ../../vocekit.pro -spec win32-g++ CONFIG+=release
& 'D:\QQQQQT0001\Tools\mingw530_32\bin\mingw32-make.exe' -j2
Pop-Location
```

Expected: clean compile and link of `release/vocekit.exe`.

- [ ] **Step 5: Package and launch without developer PATH**

Use the existing deployment script or `windeployqt` to copy Qt5Core/Gui/Widgets/Network/WebSockets/Multimedia and platform plugins beside the executable. Start the packaged executable with Qt/MinGW bins removed from PATH and verify no missing-DLL dialog appears.

- [ ] **Step 6: Perform visual and interaction checks**

Use the real floating-bar diagnostic card and capture normal, provisional, corrected, finalizing, fallback, 150%-font, and long-text states. Confirm three-line cap, no Chinese clipping, action buttons visible, and return to 720x76 after clear.

- [ ] **Step 7: Review safety and diffs**

Run `git diff --check`, search logs for secret values/auth URLs/full transcripts, confirm no tests call real hosts, and inspect `git status --short` so pre-existing untracked build artifacts are not staged.

- [ ] **Step 8: Commit project wiring and verification fixes**

```powershell
git add vocekit.pro tests/providers/streaming_transcript_accumulator_tests.pro tests/providers/provider_streaming_websocket_transport_tests.pro tests/providers/xfyun_streaming_speech_session_tests.pro tests/providers/baidu_streaming_speech_session_tests.pro tests/recording/voice_audio_recorder_stream_tests.pro tests/controllers/voice_recording_workflow_controller_tests.pro tests/ui/basic_settings_section_tests.pro tests/ui/floating_bar_streaming_tests.pro
git commit -m "build: register streaming speech components"
```

---

## Completion checklist

- [ ] Xfyun shows `wpgs` corrections during recording and rotates before 60 seconds.
- [ ] Baidu sends START/text, 5120-byte binary audio, FINISH/CANCEL and renders MID/FIN results.
- [ ] Recorder files are byte-for-byte unchanged while live PCM is forwarded without network I/O on the capture path.
- [ ] Streaming disabled/unavailable uses existing batch behavior.
- [ ] Runtime failure, empty final, overflow, or five-second timeout falls back exactly once.
- [ ] Cancel performs no fallback and no downstream work.
- [ ] Classic, long-recording, and flow success paths call downstream exactly once.
- [ ] Floating bar is readable at 100%, 125%, 150%, long Chinese, and long English.
- [ ] All tests run without real credentials or network.
- [ ] Release package starts without relying on Qt being present in PATH.
