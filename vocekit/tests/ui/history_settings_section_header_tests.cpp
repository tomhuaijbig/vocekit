#include <QtTest>

#include "../../src/ui/history_settings_section.h"

#include <type_traits>

class HistorySettingsSectionHeaderTests : public QObject
{
    Q_OBJECT

private slots:
    void constructsFromCallbacksOnly();
};

void HistorySettingsSectionHeaderTests::constructsFromCallbacksOnly()
{
    QVERIFY((std::is_constructible<
        HistorySettingsSection,
        const HistorySettingsSection::Callbacks &,
        QWidget *
    >::value));
}

QTEST_MAIN(HistorySettingsSectionHeaderTests)

#include "history_settings_section_header_tests.moc"
