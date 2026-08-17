#include <QtTest>

#include "../../src/ui/custom_model_dialog_support.h"

#include <QFont>
#include <QFontMetrics>
#include <QPushButton>

class CustomModelDialogSupportTests : public QObject
{
    Q_OBJECT

private slots:
    void buttonMinimumHeightHasFortyPixelFloor();
    void buttonMinimumHeightGrowsWithLargeFonts();
    void sizingUsesMinimumHeightAndNoVerticalPadding();
    void sizingUsesPolishedMicrosoftYaHeiMetrics();
    void finalEndpointPreviewExplainsEmptyAndInvalidValues();
    void finalEndpointPreviewNormalizesSupportedUrlForms();
};

void CustomModelDialogSupportTests::buttonMinimumHeightHasFortyPixelFloor()
{
    QFont font;
    font.setPixelSize(10);
    const QFontMetrics metrics(font);

    QCOMPARE(customModelDialogButtonMinimumHeight(metrics), 40);
}

void CustomModelDialogSupportTests::buttonMinimumHeightGrowsWithLargeFonts()
{
    QFont font;
    font.setPixelSize(50);
    const QFontMetrics metrics(font);

    QVERIFY(metrics.height() + 16 > 40);
    QCOMPARE(
        customModelDialogButtonMinimumHeight(metrics),
        metrics.height() + 16
    );
}

void CustomModelDialogSupportTests::sizingUsesMinimumHeightAndNoVerticalPadding()
{
    QPushButton button(QString::fromUtf8("测试"));
    QFont font = button.font();
    font.setPixelSize(30);
    button.setFont(font);

    applyCustomModelDialogButtonSizing(
        &button,
        QStringLiteral("#ffffff"),
        QStringLiteral("#111827")
    );
    button.show();
    QCoreApplication::processEvents();

    QCOMPARE(
        button.minimumHeight(),
        customModelDialogButtonMinimumHeight(button.fontMetrics())
    );
    QCOMPARE(button.maximumHeight(), QWIDGETSIZE_MAX);
    QVERIFY(button.property("customModelDialogActionButton").toBool());
    QVERIFY(button.styleSheet().contains(QStringLiteral("padding: 0px 12px")));
    QVERIFY(!button.styleSheet().contains(QStringLiteral("padding: 8px 12px")));
}

void CustomModelDialogSupportTests::sizingUsesPolishedMicrosoftYaHeiMetrics()
{
    QPushButton button(QString::fromUtf8("新增模型"));
    QFont font(QStringLiteral("Microsoft YaHei"));
    font.setPointSize(18);
    button.setFont(font);

    applyCustomModelDialogButtonSizing(&button);
    button.show();
    QCoreApplication::processEvents();

    QVERIFY(
        button.minimumHeight()
        >= customModelDialogButtonMinimumHeight(button.fontMetrics())
    );
}

void CustomModelDialogSupportTests::finalEndpointPreviewExplainsEmptyAndInvalidValues()
{
    QCOMPARE(
        customModelFinalEndpointPreview(QString()),
        QString::fromUtf8("填写后显示最终请求地址")
    );
    QCOMPARE(
        customModelFinalEndpointPreview(QStringLiteral("   ")),
        QString::fromUtf8("填写后显示最终请求地址")
    );
    QCOMPARE(
        customModelFinalEndpointPreview(QStringLiteral("not a host")),
        QString::fromUtf8("地址无效")
    );
}

void CustomModelDialogSupportTests::finalEndpointPreviewNormalizesSupportedUrlForms()
{
    QCOMPARE(
        customModelFinalEndpointPreview(QStringLiteral("https://api.example.com")),
        QStringLiteral("https://api.example.com/v1/chat/completions")
    );
    QCOMPARE(
        customModelFinalEndpointPreview(QStringLiteral("https://api.example.com/v1")),
        QStringLiteral("https://api.example.com/v1/chat/completions")
    );
    QCOMPARE(
        customModelFinalEndpointPreview(QStringLiteral("https://api.example.com/v1/chat/completions")),
        QStringLiteral("https://api.example.com/v1/chat/completions")
    );
    QCOMPARE(
        customModelFinalEndpointPreview(QStringLiteral("https://api.example.com/service")),
        QStringLiteral("https://api.example.com/service/v1/chat/completions")
    );
}

QTEST_MAIN(CustomModelDialogSupportTests)

#include "custom_model_dialog_support_tests.moc"
