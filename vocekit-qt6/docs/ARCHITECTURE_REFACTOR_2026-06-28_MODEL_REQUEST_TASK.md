# 架构拆分记录：ModelRequestTask

日期：2026-06-28

## 本次目标

继续把 `VoiceController` 里的业务执行逻辑往独立模块迁移。优先拆出大模型请求阶段，为后续拆 `VoiceController` 和完整功能执行管线做准备。

## 已完成

- 新增 `src/tasks/model_request_task.h/.cpp`。
- 将单次大模型请求的组装、取消令牌、提示词版本、耗时统计和错误返回集中到 `runModelRequestTask()`。
- `VoiceController::runModelRequest()` 只保留设置读取和 provider 创建，不再直接持有 `ModelRequest` 组装细节。
- 新增 `tests/tasks/model_request_task_tests.cpp/.pro`，覆盖：
  - 请求参数是否正确传给 provider。
  - 流式增量回调是否转发。
  - 取消令牌和执行编号是否生成。
  - 提示词版本和耗时是否返回。
  - provider 缺失时是否返回明确错误。

## 对架构目标的影响

- 目标 13：拆出语音任务控制器
  大模型阶段已从控制器中分出一层，后续可以继续把语音识别、OCR、输出阶段迁移到类似任务类。

- 目标 14：拆出功能执行管线
  当前已经形成了“模型请求任务”这个管线节点，后续可由 `VoiceRunExecutor` 串联输入、识别、模型和输出。

- 目标 16：统一任务取消接口
  大模型请求阶段已通过 `CancellationSource` 和 `CancellationToken` 执行，为后续外部取消接入保留入口。

## 后续建议

1. 继续抽 `VoiceRunExecutor`，接管 `runContext()` 到输出展示之间的主流程。
2. 给 `ModelRequestTaskRequest` 增加外部取消令牌入口，让结果小框的取消按钮可以真正中断生成。
3. 再把 OCR 和语音识别阶段统一成可取消的任务节点。
