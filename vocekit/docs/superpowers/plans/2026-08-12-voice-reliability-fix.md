# Voice Reliability Fix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the deployed `release` build use one stable configuration root and prevent hold-to-talk from finishing before the microphone produces usable PCM.

**Architecture:** Introduce a small configuration-path resolver shared by settings and secrets so running from `release/` still reads and writes the repository-level `config/`. Keep migration read-only until a verified atomic copy is needed. Track whether the active recording has received PCM; for hold-to-talk, defer a release that arrives during microphone warm-up until either the first PCM block arrives or a bounded timeout fires, then reuse the existing stop-and-process path exactly once.

**Tech Stack:** Qt 5.9, C++11, Qt Multimedia, Qt Test, qmake/MinGW.

---

### Task 1: Stable configuration root

**Files:**
- Create: `src/config/app_config_paths.h`
- Create: `src/config/app_config_paths.cpp`
- Modify: `src/config/app_settings_store.cpp`
- Modify: `src/config/secret_store.cpp`
- Modify: `vocekit.pro`
- Create: `tests/config/app_config_paths_tests.cpp`
- Create: `tests/config/app_config_paths_tests.pro`

- [ ] Write tests that pass an executable directory named `release` and require the returned settings and secrets paths to use the parent `config/`; require an ordinary package directory to keep its local `config/`.
- [ ] Run the new Qt test and verify RED because the shared resolver does not exist.
- [ ] Implement `appConfigBasePathForApplicationDir()` and `appConfigFilePathForApplicationDir()` without copying or deleting user data.
- [ ] Route `AppSettingsStore` and `SecretStore` default paths through the resolver.
- [ ] Run the new path tests plus `app_settings_json_tests` and `secret_config_tests`; require all pass.
- [ ] Commit the path change separately.

### Task 2: Hold-to-talk microphone warm-up

**Files:**
- Modify: `src/controllers/voice_recording_workflow_controller.cpp`
- Modify: `tests/controllers/voice_recording_workflow_controller_tests.cpp`

- [ ] Add a controller test where hold release arrives before any PCM and assert that capture does not stop immediately.
- [ ] Assert that the first later PCM block triggers exactly one stop and the normal recognition path.
- [ ] Add a second test for no PCM: release waits for a short injected grace period, stops once, and reports the existing empty-recording failure instead of hanging.
- [ ] Run the controller suite and verify RED on immediate stop.
- [ ] Implement `m_recordingReceivedPcm`, `m_holdReleasePending`, and a single-shot warm-up timer; update them only on the controller thread via the existing PCM event.
- [ ] Run the controller suite and require all tests pass without timer/thread warnings.
- [ ] Commit the recording fix separately.

### Task 3: Deployment and migration

**Files:**
- Build output: `release/vocekit.exe` and Qt/OpenSSL runtime DLLs
- User data: `config/settings.json`, `config/secrets.json`
- Backup output: `build/voice-reliability-backup-<timestamp>/`

- [ ] Run focused tests, then the complete `scripts/run-all-tests.ps1 -Configuration release` suite.
- [ ] Build the main Release target from merged sources.
- [ ] Stop only the currently running executable after verifying its exact path.
- [ ] Back up both existing configuration locations and hashes before changing deployment files.
- [ ] Promote the newer `release/config/settings.json` to root `config/settings.json` only when it is newer and valid JSON; preserve root `config/secrets.json` unchanged.
- [ ] Deploy the new executable, Qt WebSockets/Network/Multimedia libraries, MinGW runtime, plugins, and OpenSSL libraries to `release/`.
- [ ] Start `release/vocekit.exe`, confirm logs contain the new runtime and no missing-DLL error, and perform a two-second local recording smoke test.
- [ ] Recheck hashes and leave the backup path in the final report.
