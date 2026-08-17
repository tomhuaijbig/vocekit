#include <QtTest>

#include "../../src/input/hotkey_definitions.h"

namespace {

QStringList idsOf(const QVector<HotkeyDef> &defs)
{
    QStringList ids;
    for (const HotkeyDef &def : defs) {
        ids << def.id;
    }
    return ids;
}

} // namespace

class HotkeyDefinitionsTests : public QObject
{
    Q_OBJECT

private slots:
    void containsAllDefaultHotkeys()
    {
        const QStringList ids = idsOf(hotkeyDefs());
        QCOMPARE(ids.size(), 6);
        QVERIFY(ids.contains(QStringLiteral("dictate")));
        QVERIFY(ids.contains(QStringLiteral("translate")));
        QVERIFY(ids.contains(QStringLiteral("ask")));
        QVERIFY(ids.contains(QStringLiteral("vocabulary_add")));
        QVERIFY(ids.contains(QStringLiteral("hub")));
        QVERIFY(ids.contains(QStringLiteral("selection_toolbar")));
    }

    void builtInFunctionTitlesUseChineseOnly()
    {
        const QVector<HotkeyDef> defs = coreFunctionDefs();
        QCOMPARE(defs.size(), 3);
        QCOMPARE(defs.at(0).title, QString::fromUtf8("听写"));
        QCOMPARE(defs.at(1).title, QString::fromUtf8("翻译"));
        QCOMPARE(defs.at(2).title, QString::fromUtf8("问答"));
    }

    void coreFunctionsExcludeUtilityHotkeys()
    {
        const QStringList ids = idsOf(coreFunctionDefs());
        QCOMPARE(ids, QStringList()
            << QStringLiteral("dictate")
            << QStringLiteral("translate")
            << QStringLiteral("ask"));
    }

    void exposesSelectionToolbarFallbackWithoutMakingItAFunction()
    {
        const QString id = QStringLiteral("selection_toolbar");
        QVERIFY(idsOf(hotkeyDefs()).contains(id));
        QVERIFY(!idsOf(coreFunctionDefs()).contains(id));

        HotkeyDef found;
        for (const HotkeyDef &def : hotkeyDefs()) {
            if (def.id == id) {
                found = def;
                break;
            }
        }
        QCOMPARE(found.title, QString::fromUtf8("选中文字工具条"));
        QCOMPARE(found.defaultValue, QStringLiteral("Ctrl+Alt+E"));
        QCOMPARE(
            found.hint,
            QString::fromUtf8("读取当前选中文字并显示快捷工具条")
        );
    }

    void returnsDefaultScreenshotShortcuts()
    {
        QCOMPARE(
            defaultScreenshotShortcutForFunction(QStringLiteral("dictate")),
            QStringLiteral("Ctrl+Alt+Shift+Space")
        );
        QCOMPARE(
            defaultScreenshotShortcutForFunction(QStringLiteral("translate")),
            QStringLiteral("Ctrl+Alt+Shift+T")
        );
        QCOMPARE(
            defaultScreenshotShortcutForFunction(QStringLiteral("ask")),
            QStringLiteral("Ctrl+Alt+Shift+Q")
        );
        QCOMPARE(
            defaultScreenshotShortcutForFunction(QStringLiteral("custom_1")),
            QStringLiteral("Ctrl+Alt+Shift+O")
        );
    }
};

QTEST_MAIN(HotkeyDefinitionsTests)
#include "hotkey_definitions_tests.moc"
