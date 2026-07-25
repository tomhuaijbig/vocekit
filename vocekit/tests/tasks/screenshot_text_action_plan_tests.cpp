#include <QtTest>

#include "../../src/config/app_settings_data.h"
#include "../../src/tasks/screenshot_text_action_plan.h"

namespace {

FunctionSettings functionWithModel(
    const QString &id,
    const QString &model)
{
    FunctionSettings function;
    function.id = id;
    function.builtIn = true;
    function.modelId = model;
    return function;
}

} // namespace

class ScreenshotTextActionPlanTests : public QObject
{
    Q_OBJECT

private slots:
    void selectsConfiguredModelForEachAction()
    {
        AppSettingsData settings;
        settings.functions
            << functionWithModel(
                QStringLiteral("dictate"),
                QStringLiteral("dictate-model")
            )
            << functionWithModel(
                QStringLiteral("translate"),
                QStringLiteral("translate-model")
            )
            << functionWithModel(
                QStringLiteral("ask"),
                QStringLiteral("ask-model")
            );

        QCOMPARE(
            buildScreenshotTextActionPlan(
                settings,
                QStringLiteral("organize")
            ).model,
            QStringLiteral("dictate-model")
        );
        QCOMPARE(
            buildScreenshotTextActionPlan(
                settings,
                QStringLiteral("translate")
            ).model,
            QStringLiteral("translate-model")
        );
        QCOMPARE(
            buildScreenshotTextActionPlan(
                settings,
                QStringLiteral("summarize")
            ).model,
            QStringLiteral("ask-model")
        );
    }

    void buildsActionSpecificPrompts()
    {
        const AppSettingsData settings;

        const ScreenshotTextActionPlan translate =
            buildScreenshotTextActionPlan(
                settings,
                QStringLiteral("translate")
            );
        const ScreenshotTextActionPlan polish =
            buildScreenshotTextActionPlan(
                settings,
                QStringLiteral("polish")
            );

        QVERIFY(translate.systemPrompt.contains(QString::fromUtf8("翻译")));
        QVERIFY(polish.systemPrompt.contains(QString::fromUtf8("润色")));
    }
};

QTEST_MAIN(ScreenshotTextActionPlanTests)
#include "screenshot_text_action_plan_tests.moc"
