#ifndef VOCEKIT_RUNTIME_CRASH_HANDLER_H
#define VOCEKIT_RUNTIME_CRASH_HANDLER_H

// 安装进程级崩溃日志钩子。只负责记录最后异常，不做界面弹窗，避免崩溃时再触发 UI 逻辑。
void installRuntimeCrashHandlers();

#endif // VOCEKIT_RUNTIME_CRASH_HANDLER_H
