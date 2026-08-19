#include <QtTest>

#include "../../src/ui/faq_panel.h"

#include <QElapsedTimer>
#include <QLabel>
#include <QPushButton>

class FaqPanelPerformanceTests : public QObject
{
    Q_OBJECT

private slots:
    void constructsCompleteFaqWithoutBlockingTheUi();
    void keepsDeferredEntriesSearchableAndAddressable();
    void hidesInternalIdsAndHasNoManualLoadMoreButton();
};

void FaqPanelPerformanceTests::constructsCompleteFaqWithoutBlockingTheUi()
{
    QElapsedTimer timer;
    timer.start();
    FaqPanel panel;
    const qint64 elapsedMs = timer.elapsed();

    QVERIFY(panel.matchCount(QStringLiteral("API")) > 0);
    QVERIFY2(
        elapsedMs < 5000,
        qPrintable(
            QStringLiteral("FAQ construction took %1 ms")
                .arg(elapsedMs)
        )
    );
}

void FaqPanelPerformanceTests::keepsDeferredEntriesSearchableAndAddressable()
{
    FaqPanel panel;

    QCOMPARE(panel.matchCount(QStringLiteral("function-flow-history")), 1);
    QCOMPARE(panel.matchCount(QString::fromUtf8("接口页只显示当前语音服务的密钥")), 1);

    panel.showFaqId(QString::fromUtf8("接口页只显示当前语音服务的密钥"));

    int materializedCards = 0;
    int shownMatches = 0;
    const QList<QWidget *> widgets = panel.findChildren<QWidget *>();
    for (QWidget *widget : widgets) {
        if (widget->property("faqMaterialized").toBool()) {
            ++materializedCards;
        }
        const QString searchText = widget->property("faqSearchText").toString();
        if (!searchText.isEmpty()
            && searchText.contains(
                QString::fromUtf8("接口页只显示当前语音服务的密钥"),
                Qt::CaseInsensitive
            )
            && !widget->isHidden()) {
            ++shownMatches;
        }
    }

    QCOMPARE(shownMatches, 1);
    QVERIFY2(materializedCards < 49, "FAQ search should not eagerly materialize every card");
}

void FaqPanelPerformanceTests::hidesInternalIdsAndHasNoManualLoadMoreButton()
{
    FaqPanel panel;
    panel.showFaqId(QStringLiteral("function-flow-schema"));

    for (QLabel *label : panel.findChildren<QLabel *>()) {
        QVERIFY2(
            label->text() != QStringLiteral("function-flow-schema"),
            "Internal FAQ identifiers must stay searchable but not visible."
        );
    }
    for (QPushButton *button : panel.findChildren<QPushButton *>()) {
        QVERIFY2(
            !button->text().contains(QString::fromUtf8("显示更多")),
            "FAQ pagination must load automatically while scrolling."
        );
    }
}

QTEST_MAIN(FaqPanelPerformanceTests)

#include "faq_panel_performance_tests.moc"
