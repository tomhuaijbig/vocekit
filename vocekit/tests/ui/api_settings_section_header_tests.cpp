#include <QtTest>

#include "../../src/ui/api_settings_section.h"

#include <type_traits>

class ApiSettingsSectionHeaderTests : public QObject
{
    Q_OBJECT

private slots:
    void constructsFromCallbacksOnly();
};

void ApiSettingsSectionHeaderTests::constructsFromCallbacksOnly()
{
    QVERIFY((std::is_constructible<
        ApiSettingsSection,
        const ApiSettingsSection::Callbacks &,
        QWidget *
    >::value));
}

QTEST_MAIN(ApiSettingsSectionHeaderTests)

#include "api_settings_section_header_tests.moc"
