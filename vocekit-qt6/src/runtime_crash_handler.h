#ifndef VOCEKIT_RUNTIME_CRASH_HANDLER_H
#define VOCEKIT_RUNTIME_CRASH_HANDLER_H

// 安装进程级崩溃钩子。Windows 下生成 minidump 与脱敏元数据，不在
// 异常上下文里弹窗，避免崩溃时再次触发 UI 逻辑。
void installRuntimeCrashHandlers();

#endif // VOCEKIT_RUNTIME_CRASH_HANDLER_H
