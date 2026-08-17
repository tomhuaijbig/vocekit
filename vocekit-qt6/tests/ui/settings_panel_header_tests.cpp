#include <QtTest>

#include "../../src/ui/settings_panel.h"

#include <type_traits>
#include <QFile>

class SettingsPanelHeaderTests : public QObject
{
    Q_OBJECT

private slots:
    void constructsFromTypedAccessOnly();
    void mapsFloatingPreferencesAndKeepsExactTabOrder();
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

void SettingsPanelHeaderTests::mapsFloatingPreferencesAndKeepsExactTabOrder()
{
    const QString sourcePath = QFINDTESTDATA("../../src/ui/settings_panel.cpp");
    QVERIFY(!sourcePath.isEmpty());
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QByteArray contents = source.readAll();
    QVERIFY(contents.contains("snapshot.floatingBarStyle = settings.floatingBarStyle"));
    QVERIFY(contents.contains("settings.floatingBarStyle = snapshot.floatingBarStyle"));
    QVERIFY(contents.contains("snapshot.writeFailurePopupFallbackEnabled"));
    QVERIFY(contents.contains("settings.writeFailurePopupFallbackEnabled"));

    const QList<QByteArray> titles = {
        "常用设置", "词库", "语音录音", "写入",
        "网络", "历史记录", "快捷键", "接口"
    };
    int position = -1;
    for (const QByteArray &title : titles) {
        const int next = contents.indexOf(title, position + 1);
        QVERIFY2(next > position, title.constData());
        position = next;
    }
}

QTEST_MAIN(SettingsPanelHeaderTests)

#include "settings_panel_header_tests.moc"
