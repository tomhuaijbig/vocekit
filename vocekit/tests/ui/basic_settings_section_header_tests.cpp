#include <QtTest>

#include "../../src/ui/basic_settings_section.h"

#include <type_traits>

class BasicSettingsSectionHeaderTests : public QObject
{
    Q_OBJECT

private slots:
    void constructsFromCallbacksOnly();
};

void BasicSettingsSectionHeaderTests::constructsFromCallbacksOnly()
{
    QVERIFY((std::is_constructible<
        BasicSettingsSection,
        BasicSettingsSection::Kind,
        const BasicSettingsSection::Callbacks &,
        QWidget *
    >::value));
}

QTEST_MAIN(BasicSettingsSectionHeaderTests)

#include "basic_settings_section_header_tests.moc"
