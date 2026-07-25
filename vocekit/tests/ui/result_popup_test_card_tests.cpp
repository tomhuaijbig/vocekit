#include <QtTest>

#include "../../src/ui/result_popup_test_card.h"

#include <QPushButton>

class ResultPopupTestCardTests : public QObject
{
    Q_OBJECT

private slots:
    void clickInvokesPreviewCallback();
};

void ResultPopupTestCardTests::clickInvokesPreviewCallback()
{
    int invocationCount = 0;
    QWidget *callbackSource = nullptr;
    ResultPopupTestCard card(
        [&](QWidget *source) {
            ++invocationCount;
            callbackSource = source;
        }
    );

    QPushButton *button = card.findChild<QPushButton *>();
    QVERIFY(button);
    QCOMPARE(button->text(), QString::fromUtf8("开始测试"));

    QTest::mouseClick(button, Qt::LeftButton);

    QCOMPARE(invocationCount, 1);
    QCOMPARE(callbackSource, static_cast<QWidget *>(&card));
}

QTEST_MAIN(ResultPopupTestCardTests)

#include "result_popup_test_card_tests.moc"
