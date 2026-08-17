#ifndef VOCEKIT_CANCELLATION_TOKEN_H
#define VOCEKIT_CANCELLATION_TOKEN_H

#include "../domain/execution_types.h"

#include <QAtomicInt>
#include <QSharedPointer>

struct CancellationState
{
    QAtomicInt cancelled;

    CancellationState()
        : cancelled(0)
    {
    }
};

// 任务只持有令牌，用于读取取消状态和核对执行编号。
class CancellationToken
{
public:
    CancellationToken();

    bool isCancellationRequested() const;
    ExecutionId executionId() const;
    bool isValid() const;

private:
    CancellationToken(
        const QSharedPointer<CancellationState> &state,
        const ExecutionId &executionId
    );

    QSharedPointer<CancellationState> m_state;
    ExecutionId m_executionId;

    friend class CancellationSource;
};

// 发起方持有取消源；由它创建的所有令牌共享同一个线程安全状态。
class CancellationSource
{
public:
    CancellationSource();

    CancellationToken token() const;
    void cancel();
    bool isCancellationRequested() const;
    ExecutionId executionId() const;

private:
    QSharedPointer<CancellationState> m_state;
    ExecutionId m_executionId;
};

#endif // VOCEKIT_CANCELLATION_TOKEN_H
