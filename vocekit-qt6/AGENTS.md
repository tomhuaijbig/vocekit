# VoceKit Qt 6 development rules

## Baseline

- Develop and test this directory with Qt 6.11.1, MinGW 13.1 64-bit, and C++17.
- Do not add Qt 5.9, MinGW 5.3, 32-bit, OpenSSL 1.0, or C++11 compatibility branches.
- Use `scripts/build.ps1`, `scripts/run-all-tests.ps1`, `scripts/deploy.ps1`, and `scripts/package-test.ps1` as the canonical entry points.

## Architecture

- Keep UI code separate from controllers, providers, storage, recording, OCR, and network implementations.
- Keep `FunctionCanvasView` and canvas widgets out of runtime orchestration responsibilities.
- Function-flow edits change draft state; publish only validated graphs. Preserve the classic-flow fallback when no valid published graph exists.
- Do not restore deleted monolithic `.inc` or page-method architecture.

## Completion checks

- Build the affected target and both main configurations when changing shared production code.
- Run the affected tests; run `scripts/run-all-tests.ps1` before a release or broad refactor.
- For Chinese Qt UI changes, inspect normal and maximized windows, Windows font scaling, long text, and dense states. Compilation alone is not sufficient.
- For deployment changes, verify the `.qt6-deploy` runtime and start it with Qt and MinGW development paths removed from `PATH`.

## Safety

- Preserve local secrets, settings, records, logs, screenshots, build output, and unrelated user work.
- Never copy real `config/secrets.json`, `config/settings.json`, records, or logs into a portable test package.
