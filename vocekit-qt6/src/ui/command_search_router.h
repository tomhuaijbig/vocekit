#ifndef VOCEKIT_COMMAND_SEARCH_ROUTER_H
#define VOCEKIT_COMMAND_SEARCH_ROUTER_H

#include <QString>
#include <QStringList>
#include <QVector>

enum class CommandSearchTargetType
{
    None,
    Function,
    Page
};

struct CommandSearchEntry
{
    QString id;
    QString title;
    QStringList aliases;
};

struct CommandSearchResult
{
    CommandSearchTargetType type = CommandSearchTargetType::None;
    QString id;
    QString query;

    bool isValid() const;
};

// 统一解析顶部搜索框的功能和页面目标，主窗口不再维护关键词判断。
class CommandSearchRouter
{
public:
    static CommandSearchResult resolve(
        const QString &query,
        const QVector<CommandSearchEntry> &functions,
        const QVector<CommandSearchEntry> &pages
    );
    static QVector<CommandSearchEntry> defaultPages();

private:
    static bool matches(const QString &query, const CommandSearchEntry &entry);
};

#endif // VOCEKIT_COMMAND_SEARCH_ROUTER_H
