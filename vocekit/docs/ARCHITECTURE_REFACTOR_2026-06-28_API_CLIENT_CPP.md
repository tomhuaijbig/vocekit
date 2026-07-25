# 2026-06-28 接口客户端拆分记录

## 本次目标

把 `ApiClient` 从“头文件里包含大量实现”的浅模块，整理成更正常的头文件和源文件结构。

## 已完成

- `src/api/api_client.h` 现在只保留调用方需要知道的接口、成员类型和私有声明。
- `src/api/api_client.cpp` 承接百度、讯飞、自定义语音、DeepSeek、OpenAI、Claude、自定义大模型、网络请求和流式请求的具体实现。
- 只在 `.cpp` 中保留日志、错误文案、WebSocket、网络请求工具等实现依赖，减少其它文件包含 `api_client.h` 时受到的影响。
- `vocekit.pro` 已加入 `src/api/api_client.cpp`。
- `tests/providers/api_client_provider_adapters_tests.pro` 已加入 `../../src/api/api_client.cpp`，保证 provider adapter 测试链接真实实现。

## 验证

已通过：

- `qmake vocekit.pro`
- `mingw32-make -j2`
- `tests/api/release/api_client_utils_tests.exe`
- `tests/providers/release/api_client_provider_adapters_tests.exe`
- `tests/providers/release/provider_registry_tests.exe`
- `git diff --check`
- 窄范围 `cppcheck`：`src/api/api_client.cpp`、`src/api/api_client_utils.cpp`、`src/providers/api_client_provider_adapters.cpp`

## 后续建议

- 继续把 `VoiceController` 从 `voiceassistant.cpp` 拆出独立 `.h/.cpp`。
- 把 `ApiClient` 的具体请求进一步迁移到 `src/providers/`，让 provider seam 真正接管大模型和语音识别请求。
- 让功能执行流程形成独立管线：输入收集、语音识别/OCR、模型处理、结果输出。
