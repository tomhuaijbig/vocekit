#include <QtTest>

#include "../../src/config/prompt_save_route.h"

class PromptSaveRouteTests : public QObject
{
    Q_OBJECT

private slots:
    void routesEachPromptTargetToItsStorage()
    {
        PromptTargetInfo target;
        target.custom = true;
        QCOMPARE(
            promptSaveDestination(target),
            PromptSaveDestination::FunctionSettings
        );

        target.custom = false;
        target.library = true;
        QCOMPARE(
            promptSaveDestination(target),
            PromptSaveDestination::PromptLibrary
        );

        target.library = false;
        QCOMPARE(
            promptSaveDestination(target),
            PromptSaveDestination::PromptFile
        );
    }
};

QTEST_MAIN(PromptSaveRouteTests)
#include "prompt_save_route_tests.moc"
