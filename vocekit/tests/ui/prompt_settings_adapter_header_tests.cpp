#include <QtTest>

#include "../../src/ui/prompt_settings_adapter.h"

#include <type_traits>
#include <utility>

class PromptSettingsAdapterHeaderTests : public QObject
{
    Q_OBJECT

private slots:
    void exposesAccessBasedTargetLookup();
};

void PromptSettingsAdapterHeaderTests::exposesAccessBasedTargetLookup()
{
    typedef decltype(sharedPromptTargets(
        std::declval<const PromptSettingsAccess &>())) ResultType;
    QVERIFY((std::is_same<ResultType, QVector<PromptTargetInfo>>::value));
}

QTEST_MAIN(PromptSettingsAdapterHeaderTests)

#include "prompt_settings_adapter_header_tests.moc"
