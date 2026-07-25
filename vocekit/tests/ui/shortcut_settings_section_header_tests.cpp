#include <QtTest>

#include "../../src/ui/shortcut_settings_section.h"

#include <type_traits>

class ShortcutSettingsSectionHeaderTests : public QObject
{
    Q_OBJECT

private slots:
    void constructsFromCallbacksOnly();
};

void ShortcutSettingsSectionHeaderTests::constructsFromCallbacksOnly()
{
    QVERIFY((std::is_constructible<
        ShortcutSettingsSection,
        const ShortcutSettingsSection::Callbacks &,
        QWidget *
    >::value));
}

QTEST_MAIN(ShortcutSettingsSectionHeaderTests)

#include "shortcut_settings_section_header_tests.moc"
