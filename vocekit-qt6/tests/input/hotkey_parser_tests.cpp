#include <QtTest>

#include "../../src/input/hotkey_parser.h"

class HotkeyParserTests : public QObject
{
    Q_OBJECT

private slots:
    void parsesCommonShortcuts()
    {
        NativeHotkey hotkey;
        QVERIFY(parseNativeHotkey(QStringLiteral("Ctrl+Alt+X"), &hotkey));
        QVERIFY(hotkey.isValid());
        QCOMPARE(hotkey.modifiers, uint(0x0002 | 0x0001));
        QCOMPARE(hotkey.key, uint('X'));

        QVERIFY(parseNativeHotkey(QStringLiteral("Ctrl+Alt+1"), &hotkey));
        QCOMPARE(hotkey.key, uint('1'));

        QVERIFY(parseNativeHotkey(QStringLiteral("Ctrl+Shift+F5"), &hotkey));
        QCOMPARE(hotkey.modifiers, uint(0x0002 | 0x0004));
        QCOMPARE(hotkey.key, uint(0x70 + 4));
    }

    void parsesSpecialKeys()
    {
        NativeHotkey hotkey;
        QVERIFY(parseNativeHotkey(QStringLiteral("Ctrl+Alt+Space"), &hotkey));
        QCOMPARE(hotkey.key, uint(0x20));

        QVERIFY(parseNativeHotkey(QStringLiteral("Ctrl+Alt+Return"), &hotkey));
        QCOMPARE(hotkey.key, uint(0x0d));

        QVERIFY(parseNativeHotkey(QStringLiteral("Ctrl+Alt+Tab"), &hotkey));
        QCOMPARE(hotkey.key, uint(0x09));
    }

    void rejectsInvalidShortcuts()
    {
        NativeHotkey hotkey;
        QVERIFY(!parseNativeHotkey(QString(), &hotkey));
        QVERIFY(!hotkey.isValid());

        QVERIFY(!parseNativeHotkey(QStringLiteral("X"), &hotkey));
        QVERIFY(!parseNativeHotkey(QStringLiteral("Ctrl+Alt+;"), &hotkey));
    }
};

QTEST_MAIN(HotkeyParserTests)
#include "hotkey_parser_tests.moc"
