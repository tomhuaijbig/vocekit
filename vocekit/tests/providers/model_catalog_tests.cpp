#include "../../src/providers/model_catalog.h"

#include <QtTest>

class ModelCatalogTests : public QObject
{
    Q_OBJECT

private slots:
    void displayTextHandlesEmptyBuiltInAndUnknownIds();
};

void ModelCatalogTests::displayTextHandlesEmptyBuiltInAndUnknownIds()
{
    QCOMPARE(
        modelDisplayText(QString()),
        QString::fromUtf8("\u672a\u8c03\u7528\u5927\u6a21\u578b")
    );
    QCOMPARE(
        modelDisplayText(QStringLiteral("deepseek-v4-flash")),
        QStringLiteral("deepseek-v4-flash")
    );
    QCOMPARE(
        modelDisplayText(QStringLiteral("unknown-model")),
        QStringLiteral("unknown-model")
    );
}

QTEST_MAIN(ModelCatalogTests)

#include "model_catalog_tests.moc"
