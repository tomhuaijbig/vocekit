# 2026-06-28 接口客户端执行器接入记录

## 本次目标

继续收拢 `ApiClient` 里的网络请求实现，把普通 HTTP 请求迁移到统一的 `NetworkRequestExecutor`。

## 已完成

- `ApiClient::get()` 改为调用 `NetworkRequestExecutor::get()`。
- `ApiClient::postJson()` 改为调用 `NetworkRequestExecutor::postJson()`。
- `ApiClient::postJsonEventStream()` 改为调用 `NetworkRequestExecutor::postEventStream()`，并在 `ApiClient` 内继续按 `data:` 事件切分，保持现有模型流式解析行为。
- 删除旧的 `ApiClient::waitReply()`，避免普通 HTTP 请求存在两套事件循环、超时和错误处理实现。
- 删除 `ApiClient` 自己持有的 `QNetworkAccessManager` 成员，普通 HTTP 和流式 HTTP 都由统一执行器创建和管理。
- 保留 `ApiClient::networkProxyForUrl()`，原因是讯飞 WebSocket 还需要它。
- `tests/providers/api_client_provider_adapters_tests.pro` 已补入 `network_request_executor.cpp/.h`，保证 adapter 测试链接真实执行器实现。

## 验证

已通过：

- `qmake vocekit.pro`
- `mingw32-make -j2`
- `tests/api/release/api_client_utils_tests.exe`
- `tests/providers/release/api_client_provider_adapters_tests.exe`
- `tests/providers/release/provider_registry_tests.exe`
- `tests/providers/release/network_request_executor_tests.exe`

## 后续建议

- 讯飞 WebSocket 仍然是特殊协议，适合后续拆成独立 `XfyunSpeechProvider`，不要强塞进普通 HTTP 执行器。
