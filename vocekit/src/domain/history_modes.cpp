#include "history_modes.h"

namespace {

QString text(const char *value)
{
    return QString::fromUtf8(value);
}

} // namespace

QString historyBuiltinModeId(const QString &modeTitle)
{
    if (modeTitle == text("听写")) return QStringLiteral("dictate");
    if (modeTitle == text("翻译")) return QStringLiteral("translate");
    if (modeTitle == text("问答")) return QStringLiteral("ask");
    if (modeTitle == text("图片识别")) return QStringLiteral("ocr");
    return QString();
}

QString historyEntryEffectiveModeId(
    const HistoryEntry &entry,
    const QVector<CustomFunctionDef> &customFunctions
)
{
    if (!entry.modeId.trimmed().isEmpty()) {
        return entry.modeId;
    }

    const QString builtinId = historyBuiltinModeId(entry.mode);
    if (!builtinId.isEmpty()) {
        return builtinId;
    }

    for (const CustomFunctionDef &fn : customFunctions) {
        if (entry.mode == fn.name) {
            return fn.id;
        }
    }
    return QString();
}

bool historyEntryMatchesModeId(
    const HistoryEntry &entry,
    const QString &modeId,
    const QVector<CustomFunctionDef> &customFunctions
)
{
    if (modeId == QStringLiteral("__all")) {
        return true;
    }
    if (modeId == QStringLiteral("__favorite")) {
        return entry.favorite;
    }

    const QString favoriteFolderPrefix = QStringLiteral("__favorite_folder:");
    if (modeId.startsWith(favoriteFolderPrefix)) {
        return entry.favorite
            && entry.favoriteFolder.trimmed()
                == modeId.mid(favoriteFolderPrefix.size());
    }
    return historyEntryEffectiveModeId(entry, customFunctions) == modeId;
}

QVector<HistoryTabDef> buildHistoryTabModes(
    const QVector<CustomFunctionDef> &customFunctions
)
{
    QVector<HistoryTabDef> tabs;
    tabs.append({QStringLiteral("__all"), text("全部")});
    tabs.append({QStringLiteral("__favorite"), text("收藏")});
    tabs.append({QStringLiteral("dictate"), text("听写")});
    tabs.append({QStringLiteral("translate"), text("翻译")});
    tabs.append({QStringLiteral("ask"), text("问答")});
    tabs.append({QStringLiteral("ocr"), text("图片识别")});
    for (const CustomFunctionDef &fn : customFunctions) {
        const QString title = fn.name.trimmed().isEmpty()
            ? text("自定义功能")
            : fn.name.trimmed();
        tabs.append({fn.id, title});
    }
    return tabs;
}
