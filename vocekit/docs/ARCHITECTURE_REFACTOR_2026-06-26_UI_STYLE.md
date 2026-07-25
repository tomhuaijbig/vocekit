# 2026-06-26 UI Style Refactor

## What Changed

- Moved common UI style helpers out of `src/voiceassistant.cpp`.
- Added `src/ui/ui_style.h` and `src/ui/ui_style.cpp`.
- The extracted helpers are:
  - `appFont`
  - `cardStyle`
  - `buttonStyle`
  - `compactButtonStyle`
- Updated `vocekit.pro` so the style module is compiled with the app.

## Why

The same font and button/card styles are used by the main page, history page, vocabulary page, settings page, test page and dialogs. Keeping these helpers in the large application file made global visual changes harder to review. The new module gives UI styling a smaller and clearer place to live.

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

Continue extracting UI-only helpers before moving larger pages. Good candidates are small dialog/menu builders that do not own task state. Avoid extracting the result generation or voice execution path until the surrounding data contracts are smaller.
