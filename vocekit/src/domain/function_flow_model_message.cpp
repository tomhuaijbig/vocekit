#include "function_flow_model_message.h"

#include <algorithm>

QString buildFunctionFlowUserPrompt(
    const QList<FunctionFlowValue> &values)
{
    struct OrderedValue
    {
        FunctionFlowValue value;
        int originalIndex = 0;
    };

    QVector<OrderedValue> ordered;
    ordered.reserve(values.size());
    for (int index = 0; index < values.size(); ++index) {
        if (values.at(index).text.trimmed().isEmpty()) {
            continue;
        }
        OrderedValue item;
        item.value = values.at(index);
        item.originalIndex = index;
        ordered.append(item);
    }

    std::stable_sort(
        ordered.begin(),
        ordered.end(),
        [](const OrderedValue &left, const OrderedValue &right) {
            if (left.value.sequence != right.value.sequence) {
                return left.value.sequence < right.value.sequence;
            }
            if (left.value.sourceNodeId != right.value.sourceNodeId) {
                return left.value.sourceNodeId
                    < right.value.sourceNodeId;
            }
            return left.originalIndex < right.originalIndex;
        }
    );

    QStringList blocks;
    for (const OrderedValue &item : ordered) {
        const QString role = item.value.role.trimmed().isEmpty()
            ? QString::fromUtf8("输入")
            : item.value.role.trimmed();
        blocks.append(
            QStringLiteral("[")
                + role
                + QStringLiteral("]\n")
                + item.value.text.trimmed()
        );
    }
    return blocks.join(QStringLiteral("\n\n"));
}
