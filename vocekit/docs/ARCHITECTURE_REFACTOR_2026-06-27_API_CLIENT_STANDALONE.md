# 2026-06-27 ApiClient 自包含化

## 本次目标

把 `src/api/api_client.h` 从依赖包含方隐式环境的旧头文件，改成可以独立包含的接口模块，为后续把网络实现迁移到真正的 `.cpp` 和 `providers/` 目录做准备。

## 改动内容

- 为 `ApiClient` 增加 include guard，避免重复包含。
- 在 `ApiClient` 头文件内显式声明它需要的 Qt、配置、密钥、网络错误、运行日志和诊断工具依赖。
- 把 `ApiClient` 内部的 `tr8()` 调用改为本模块自己的 `apiClientTr8()`，不再依赖 `voiceassistant.cpp` 或其他包含方提前定义同名函数。
- 保留原有对外行为，Provider adapter、主程序和接口自检仍使用同一套请求逻辑。

## 验证结果

- 主程序：`qmake vocekit.pro` 和 `mingw32-make -j2` 通过。
- 单元测试：
  - `tests/api/api_client_utils_tests` 通过，6 项。
  - `tests/providers/api_client_provider_adapters_tests` 通过，6 项。
  - `tests/providers/provider_registry_tests` 通过，6 项。
- 单独包含探针：临时文件只包含 `src/api/api_client.h`，不定义外部 `tr8()`，编译通过。
- `cppcheck`：
  - 不传 Qt 系统 include、只检查项目源码边界时通过。
  - 传入 Qt 5.9 系统 include 时会进入 Qt 自身模板和头文件实现，输出大量第三方告警和解析噪声，不作为本次项目源码问题。

## 后续

- 下一步可以继续把 `ApiClient` 的内联实现拆出到 `api_client.cpp`。
- Provider 抽象已经能包住旧 `ApiClient`，后续可以逐个把 DeepSeek、OpenAI、Claude、百度、讯飞和自定义接口迁移到 `src/providers/` 下的独立实现。
