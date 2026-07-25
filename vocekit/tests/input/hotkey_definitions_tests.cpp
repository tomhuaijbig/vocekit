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
        QCOMPARE(ids.size(), 5);
        QVERIFY(ids.contains(QStringLiteral("dictate")));
        QVERIFY(ids.contains(QStringLiteral("translate")));
        QVERIFY(ids.contains(QStringLiteral("ask")));
        QVERIFY(ids.contains(QStringLiteral("vocabulary_add")));
        QVERIFY(ids.contains(QStringLiteral("hub")));
    }

    void coreFunctionsExcludeUtilityHotkeys()
    {
        const QStringList ids = idsOf(coreFunctionDefs());
        QCOMPARE(ids, QStringList()
            << QStringLiteral("dictate")
            << QStringLiteral("translate")
            << QStringLiteral("ask"));
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
