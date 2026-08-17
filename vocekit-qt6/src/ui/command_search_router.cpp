#include "command_search_router.h"

#include <algorithm>

namespace {

QString uiText(const char *text)
{
    return QString::fromUtf8(text);
}

CommandSearchEntry pageEntry(
    const char *id,
    const char *title,
    const QStringList &aliases = QStringList())
{
    CommandSearchEntry entry;
    entry.id = QString::fromLatin1(id);
    entry.title = uiText(title);
    entry.aliases = aliases;
    return entry;
}

} // namespace

bool CommandSearchResult::isValid() const
{
    return type != CommandSearchTargetType::None && !id.trimmed().isEmpty();
}

CommandSearchResult CommandSearchRouter::resolve(
    const QString &query,
    const QVector<CommandSearchEntry> &functions,
    const QVector<CommandSearchEntry> &pages)
{
    const QString keyword = query.trimmed();
    if (keyword.isEmpty()) {
        return CommandSearchResult();
    }

    const auto function = std::find_if(
        functions.constBegin(), functions.constEnd(),
        [&keyword](const CommandSearchEntry &entry) {
            return matches(keyword, entry);
        }
    );
    if (function != functions.constEnd()) {
        CommandSearchResult result;
        result.type = CommandSearchTargetType::Function;
        result.id = function->id;
        result.query = keyword;
        return result;
    }

    const auto page = std::find_if(
        pages.constBegin(), pages.constEnd(),
        [&keyword](const CommandSearchEntry &entry) {
            return matches(keyword, entry);
        }
    );
    if (page != pages.constEnd()) {
        CommandSearchResult result;
        result.type = CommandSearchTargetType::Page;
        result.id = page->id;
        result.query = keyword;
        return result;
    }
    return CommandSearchResult();
}

QVector<CommandSearchEntry> CommandSearchRouter::defaultPages()
{
    return {
        pageEntry("history", "历史记录", QStringList() << uiText("历史")),
        pageEntry("vocabulary", "词库"),
        pageEntry("ocr", "图片识别", QStringList() << QStringLiteral("OCR") << uiText("截图识别")),
        pageEntry("prompts", "提示词"),
        pageEntry("diagnostics", "测试工具", QStringList() << uiText("测试")),
        pageEntry("logs", "日志"),
        pageEntry("settings", "设置"),
        pageEntry("faq", "常见问题", QStringList() << uiText("帮助"))
    };
}

bool CommandSearchRouter::matches(const QString &query, const CommandSearchEntry &entry)
{
    if (entry.id.trimmed().isEmpty()) {
        return false;
    }
    const auto matchesText = [&query](const QString &text) {
        const QString candidate = text.trimmed();
        return !candidate.isEmpty()
            && (candidate.contains(query, Qt::CaseInsensitive)
                || query.contains(candidate, Qt::CaseInsensitive));
    };
    if (matchesText(entry.title)) {
        return true;
    }
    return std::any_of(
        entry.aliases.constBegin(), entry.aliases.constEnd(),
        matchesText
    );
}
