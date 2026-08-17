#ifndef VOCEKIT_DIAGNOSTIC_TASK_RUNNER_H
#define VOCEKIT_DIAGNOSTIC_TASK_RUNNER_H

#include "cancellation_token.h"

#include <QObject>
#include <QStringList>

#include <functional>

template <typename T>
class QFutureWatcher;

// 统一管理诊断页后台任务，负责取消旧任务并屏蔽过期结果。
class DiagnosticTaskRunner : public QObject
{
public:
    typedef std::function<QStringList(const CancellationToken &)> Task;

    explicit DiagnosticTaskRunner(QObject *parent = nullptr);
    ~DiagnosticTaskRunner() override;

    bool isBusy() const;
    void start(const Task &task);
    void cancel();

    std::function<void(const QStringList &)> finishedCallback;

private:
    bool m_busy = false;
    quint64 m_generation = 0;
    CancellationSource m_cancellation;
    QFutureWatcher<QStringList> *m_watcher = nullptr;
};

#endif // VOCEKIT_DIAGNOSTIC_TASK_RUNNER_H
