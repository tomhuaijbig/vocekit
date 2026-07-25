#include "cancellation_token.h"

#include <QUuid>

namespace {

ExecutionId newExecutionId()
{
    ExecutionId id;
    id.value = QUuid::createUuid().toString();
    id.value.remove(QLatin1Char('{'));
    id.value.remove(QLatin1Char('}'));
    return id;
}

} // namespace

CancellationToken::CancellationToken()
{
}

CancellationToken::CancellationToken(
    const QSharedPointer<CancellationState> &state,
    const ExecutionId &executionId)
    : m_state(state),
      m_executionId(executionId)
{
}

bool CancellationToken::isCancellationRequested() const
{
    return !m_state.isNull() && m_state->cancelled.loadAcquire() != 0;
}

ExecutionId CancellationToken::executionId() const
{
    return m_executionId;
}

bool CancellationToken::isValid() const
{
    return !m_state.isNull() && m_executionId.isValid();
}

CancellationSource::CancellationSource()
    : m_state(new CancellationState),
      m_executionId(newExecutionId())
{
}

CancellationToken CancellationSource::token() const
{
    return CancellationToken(m_state, m_executionId);
}

void CancellationSource::cancel()
{
    m_state->cancelled.storeRelease(1);
}

bool CancellationSource::isCancellationRequested() const
{
    return m_state->cancelled.loadAcquire() != 0;
}

ExecutionId CancellationSource::executionId() const
{
    return m_executionId;
}
