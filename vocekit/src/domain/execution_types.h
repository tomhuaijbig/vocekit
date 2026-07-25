#ifndef VOCEKIT_EXECUTION_TYPES_H
#define VOCEKIT_EXECUTION_TYPES_H

#include "operation_error.h"

#include <QMetaType>
#include <QString>

#include <numeric>

// 各处理阶段耗时，供历史详情、浮动条和日志统一使用。
struct ExecutionTiming
{
    qint64 inputMs = -1;
    qint64 speechMs = -1;
    qint64 ocrMs = -1;
    qint64 modelMs = -1;
    qint64 writeMs = -1;
    qint64 totalMs = -1;

    qint64 totalFromStages() const
    {
        const qint64 values[] = {
            inputMs,
            speechMs,
            ocrMs,
            modelMs,
            writeMs
        };
        return std::accumulate(
            values,
            values + 5,
            qint64(0),
            [](qint64 total, qint64 value) {
                return value > 0 ? total + value : total;
            }
        );
    }
};

// 异步任务使用稳定编号，避免旧任务回调覆盖新任务状态。
struct ExecutionId
{
    QString value;

    bool isValid() const
    {
        return !value.trimmed().isEmpty();
    }

    bool operator==(const ExecutionId &other) const
    {
        return value == other.value;
    }

    bool operator!=(const ExecutionId &other) const
    {
        return !(*this == other);
    }
};

Q_DECLARE_METATYPE(ExecutionId)
Q_DECLARE_METATYPE(ExecutionTiming)

#endif // VOCEKIT_EXECUTION_TYPES_H
