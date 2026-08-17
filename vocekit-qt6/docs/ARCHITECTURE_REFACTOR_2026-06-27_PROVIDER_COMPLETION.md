# 2026-06-27 Provider 抽象收尾

本轮目标是把已经存在但主要停留在测试层的 Provider 抽象推进到真实业务路径。

## 完成内容

1. 新增 `src/api/api_client_utils.h/.cpp`
   - 集中放置 `ApiClient` 依赖的 URL 规范化、JSON 路径取值、日志目标压缩和 HMAC-SHA256 工具。
   - 从 `voiceassistant.cpp` 移除对应静态函数，降低 `ApiClient` 对主文件前置代码的隐式依赖。

2. 新增 `src/providers/api_client_provider_adapters.h/.cpp`
   - 把现有 `ApiClient` 包装为 `ISpeechProvider` 和 `IModelProvider` adapter。
   - 注册百度语音、讯飞语音、自定义语音、DeepSeek、OpenAI、Claude 和自定义大模型。
   - 支持接口自检时的系统代理选项。
   - 支持任务开始前的取消令牌检查，避免取消任务继续进入网络请求。

3. 接入真实业务路径
   - “测试工具 -> 接口自检”改为通过 `ProviderRegistry` 检查语音和大模型接口。
   - 语音输入结束后的识别阶段改为通过 `ISpeechProvider` 执行。
   - 大模型请求统一入口 `runModelRequest()` 改为通过 `IModelProvider` 执行。

4. 新增测试
   - `tests/api/api_client_utils_tests.cpp`
   - `tests/providers/api_client_provider_adapters_tests.cpp`

## 当前收益

- Provider seam 不再只是测试用接口，已经覆盖接口自检、语音识别和模型生成三个真实路径。
- 后续可以把百度、讯飞、DeepSeek、OpenAI、Claude 的具体网络实现逐个从 `ApiClient` 迁移到 `src/providers/`，不需要再改主执行流程。
- `ApiClient` 的隐式依赖少了一层，后续把它从头文件改成真正 `.h/.cpp` 会更容易。

## 仍未完成

- `ApiClient` 仍是大头文件，具体请求实现还没有完全迁移到独立 Provider 实现。
- Provider 目前还是 adapter 套旧实现，不是最终结构。
- `voiceassistant.cpp` 仍包含 HubWindow、VoiceController 和大量页面 glue code，需要继续拆。
- 每个功能独立网络策略目前在主执行路径中仍使用 `inherit`，后续应从功能配置里读取。

## 验证

- 主程序：`qmake vocekit.pro && mingw32-make` 通过。
- `api_client_utils_tests`：6 项通过。
- `api_client_provider_adapters_tests`：6 项通过。
- `provider_registry_tests`：6 项通过。
- cppcheck：本轮新增 Provider 和 Api 工具模块通过。
