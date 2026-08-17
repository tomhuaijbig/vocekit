# 2026-06-26 Toggle Switch Refactor

## What Changed

- Moved the global QCheckBox switch painting logic out of `src/voiceassistant.cpp`.
- Added `src/ui/toggle_switch.h` and `src/ui/toggle_switch.cpp`.
- Kept the existing `ToggleSwitchStyle` and `labeledSwitch` interfaces so current pages do not need behavior changes.
- Updated `vocekit.pro` so the new UI module is built with the app.

## Why

The switch UI is reused by the function editor and settings pages. Keeping it inside the main application file made small visual changes risky, because future edits had to touch the large `voiceassistant.cpp` file. The new UI module keeps this code local to the switch behavior.

## Verification

- `qmake vocekit.pro`
- `mingw32-make`
- `tests/domain/release/domain_types_tests.exe`
- `tests/config/release/app_settings_json_tests.exe`
- `tests/config/release/app_settings_defaults_tests.exe`
- `tests/input/release/hotkey_parser_tests.exe`
- `tests/input/release/hotkey_definitions_tests.exe`
- `tests/capture/release/screenshot_core_tests.exe`
- `cppcheck --language=c++ --enable=warning,performance,portability`
- `git diff --check`

## Next Safe Step

The next low-risk extraction is to move small UI helpers that are still static functions in `voiceassistant.cpp`, such as card/button styling helpers. Larger pages such as history, vocabulary, and settings should be split after their shared callbacks are reduced.
