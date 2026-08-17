#include "processing_guard.h"

ProcessingGuard::ProcessingGuard(bool &flag)
    : m_flag(flag), m_previous(flag)
{
    m_flag = true;
}

ProcessingGuard::~ProcessingGuard()
{
    m_flag = m_previous;
}
