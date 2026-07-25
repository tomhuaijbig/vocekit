# 2026-06-26 Command Center Style Refactor

## What Changed

- Converted `src/ui/command_center_shell.h` from a header-only implementation into a normal declaration header.
- Added `src/ui/command_center_shell.cpp` for the actual command-center style implementations.
- Kept the public function names unchanged:
  - `commandCenterSidebarStyle`
  - `commandCenterFunctionButtonStyle`
  - `commandCenterToolButtonStyle`
  - `commandCenterSearchStyle`
  - `commandCenterSectionStyle`
- Updated `vocekit.pro` so the new implementation file is built.

## Why

The command-center shell styles are UI implementation details. Keeping them as `static` functions in a header made the file behave like inline implementation code. Moving them into a `.cpp` file makes the UI shell easier to maintain and keeps future visual edits local to one implementation file.

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

The remaining high-value work is to reduce the direct coupling between `HubWindow` and page implementations. The next practical extraction should be a small page widget or a small dialog that has limited callbacks.
