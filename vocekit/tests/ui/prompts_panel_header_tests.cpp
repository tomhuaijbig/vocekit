#include <QtTest>

#include "../../src/ui/prompts_panel.h"

#include <type_traits>

class PromptsPanelHeaderTests : public QObject
{
    Q_OBJECT

private slots:
    void constructsFromAccessOnly();
};

void PromptsPanelHeaderTests::constructsFromAccessOnly()
{
    QVERIFY((std::is_constructible<
        PromptsPanel,
        const PromptsPanelAccess &,
        const std::function<void()> &,
        QWidget *
    >::value));
}

QTEST_MAIN(PromptsPanelHeaderTests)

#include "prompts_panel_header_tests.moc"
