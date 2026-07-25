#ifndef PROCESSING_GUARD_H
#define PROCESSING_GUARD_H

// 处理状态守卫：在语音识别或模型处理期间防止快捷键重复进入，离开作用域后恢复原状态。
class ProcessingGuard
{
public:
    explicit ProcessingGuard(bool &flag);
    ~ProcessingGuard();

private:
    bool &m_flag;
    bool m_previous;
};

#endif // PROCESSING_GUARD_H
