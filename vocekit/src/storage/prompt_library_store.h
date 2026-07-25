#ifndef VOCEKIT_PROMPT_LIBRARY_STORE_H
#define VOCEKIT_PROMPT_LIBRARY_STORE_H

#include "../domain/app_legacy_types.h"
#include "../domain/operation_error.h"

#include <QString>
#include <QVector>

// 独立保存自定义提示词，避免主窗口自行解析 prompts.json。
class PromptLibraryStore
{
public:
    explicit PromptLibraryStore(const QString &path = QString());

    bool load(OperationError *error = nullptr);
    bool save(
        const QVector<PromptLibraryItem> &items,
        OperationError *error = nullptr
    );

    const QVector<PromptLibraryItem> &items() const;
    QString path() const;

private:
    QString m_path;
    QVector<PromptLibraryItem> m_items;
};

#endif // VOCEKIT_PROMPT_LIBRARY_STORE_H
