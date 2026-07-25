#include <QtTest>

#include "../../src/config/app_settings_data.h"
#include "../../src/domain/function_catalog.h"

class FunctionCatalogTests : public QObject
{
    Q_OBJECT

private slots:
    void findsFunctionFromTypedSettings()
    {
        AppSettingsData settings;
        FunctionSettings custom;
        custom.id = QStringLiteral("custom-1");
        custom.name = QStringLiteral("Rewrite");
        settings.functions.append(custom);

        bool found = false;
        const FunctionSettings actual =
            functionSettingsById(settings, QStringLiteral("custom-1"), &found);

        QCOMPARE(found, true);
        QCOMPARE(actual.name, QStringLiteral("Rewrite"));
    }

    void resolvesBuiltInAndCustomTitles()
    {
        AppSettingsData settings;
        FunctionSettings custom;
        custom.id = QStringLiteral("custom-1");
        custom.name = QStringLiteral(" Rewrite ");
        settings.functions.append(custom);

        QCOMPARE(
            functionDisplayTitle(settings, QStringLiteral("dictate")),
            QString::fromUtf8("听写（Dictate）")
        );
        QCOMPARE(
            functionDisplayTitle(settings, QStringLiteral("custom-1")),
            QStringLiteral("Rewrite")
        );
        QCOMPARE(
            functionDisplayTitle(
                settings,
                QStringLiteral("missing"),
                QStringLiteral("Fallback")
            ),
            QStringLiteral("Fallback")
        );
    }
};

QTEST_MAIN(FunctionCatalogTests)
#include "function_catalog_tests.moc"
