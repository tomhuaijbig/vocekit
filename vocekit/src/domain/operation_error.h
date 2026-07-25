#ifndef VOCEKIT_OPERATION_ERROR_H
#define VOCEKIT_OPERATION_ERROR_H

#include <QString>

// 业务模块统一返回的错误数据。界面根据这些字段决定如何提示用户。
struct OperationError
{
    QString code;
    QString message;
    QString detail;
    bool retryable = false;

    bool isEmpty() const
    {
        return code.trimmed().isEmpty()
            && message.trimmed().isEmpty()
            && detail.trimmed().isEmpty();
    }
};

#endif // VOCEKIT_OPERATION_ERROR_H
