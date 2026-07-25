#include <QtTest>

#include "../../src/ui/settings_panel.h"

#include <type_traits>

class SettingsPanelHeaderTests : public QObject
{
    Q_OBJECT

private slots:
    void constructsFromTypedAccessOnly();
};

void SettingsPanelHeaderTests::constructsFromTypedAccessOnly()
{
    QVERIFY((std::is_constructible<
        SettingsPanel,
        const SettingsPanelAccess &,
        const std::function<void()> &,
        QWidget *,
        int
    >::value));
}

QTEST_MAIN(SettingsPanelHeaderTests)

#include "settings_panel_header_tests.moc"
