#include "../../src/domain/history_modes.h"

#include <QtTest>

namespace {

HistoryEntry historyEntry(const QString &modeId, const QString &mode)
{
    HistoryEntry entry;
    entry.modeId = modeId;
    entry.mode = mode;
    return entry;
}

CustomFunctionDef customFunction(const QString &id, const QString &name)
{
    CustomFunctionDef fn;
    fn.id = id;
    fn.name = name;
    return fn;
}

} // namespace

class HistoryModesTests : public QObject
{
    Q_OBJECT

private slots:
    void mapsBuiltInModeTitles();
    void explicitModeIdTakesPriority();
    void mapsCustomFunctionNames();
    void matchesFavoriteFolders();
    void buildsTabsWithCustomFunctions();
};

void HistoryModesTests::mapsBuiltInModeTitles()
{
    QCOMPARE(historyBuiltinModeId(QStringLiteral("听写")), QStringLiteral("dictate"));
    QCOMPARE(historyBuiltinModeId(QStringLiteral("翻译")), QStringLiteral("translate"));
    QCOMPARE(historyBuiltinModeId(QStringLiteral("问答")), QStringLiteral("ask"));
    QCOMPARE(historyBuiltinModeId(QStringLiteral("图片识别")), QStringLiteral("ocr"));
    QVERIFY(historyBuiltinModeId(QStringLiteral("未知")).isEmpty());
}

void HistoryModesTests::explicitModeIdTakesPriority()
{
    const HistoryEntry entry = historyEntry(QStringLiteral("custom_saved"), QStringLiteral("听写"));
    QCOMPARE(
        historyEntryEffectiveModeId(entry, QVector<CustomFunctionDef>()),
        QStringLiteral("custom_saved")
    );
}

void HistoryModesTests::mapsCustomFunctionNames()
{
    QVector<CustomFunctionDef> functions;
    functions.append(customFunction(QStringLiteral("custom_polish"), QStringLiteral("论文润色")));

    const HistoryEntry entry = historyEntry(QString(), QStringLiteral("论文润色"));
    QCOMPARE(
        historyEntryEffectiveModeId(entry, functions),
        QStringLiteral("custom_polish")
    );
}

void HistoryModesTests::matchesFavoriteFolders()
{
    HistoryEntry entry = historyEntry(QStringLiteral("dictate"), QStringLiteral("听写"));
    entry.favorite = true;
    entry.favoriteFolder = QStringLiteral("工作");

    QVERIFY(historyEntryMatchesModeId(entry, QStringLiteral("__favorite"), QVector<CustomFunctionDef>()));
    QVERIFY(historyEntryMatchesModeId(entry, QStringLiteral("__favorite_folder:工作"), QVector<CustomFunctionDef>()));
    QVERIFY(!historyEntryMatchesModeId(entry, QStringLiteral("__favorite_folder:学习"), QVector<CustomFunctionDef>()));
}

void HistoryModesTests::buildsTabsWithCustomFunctions()
{
    QVector<CustomFunctionDef> functions;
    functions.append(customFunction(QStringLiteral("custom_1"), QStringLiteral("会议整理")));
    functions.append(customFunction(QStringLiteral("custom_2"), QString()));

    const QVector<HistoryTabDef> tabs = buildHistoryTabModes(functions);
    QVERIFY(tabs.size() >= 8);
    QCOMPARE(tabs.at(0).id, QStringLiteral("__all"));
    QCOMPARE(tabs.at(1).id, QStringLiteral("__favorite"));
    QCOMPARE(tabs.at(6).id, QStringLiteral("custom_1"));
    QCOMPARE(tabs.at(6).title, QStringLiteral("会议整理"));
    QCOMPARE(tabs.at(7).id, QStringLiteral("custom_2"));
    QCOMPARE(tabs.at(7).title, QStringLiteral("自定义功能"));
}

QTEST_MAIN(HistoryModesTests)

#include "history_modes_tests.moc"
