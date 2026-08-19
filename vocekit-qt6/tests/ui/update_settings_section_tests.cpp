#include "../../src/ui/update_settings_section.h"

#include <QCoreApplication>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QTextBrowser>
#include <QtTest>

class UpdateSettingsSectionTests : public QObject
{
    Q_OBJECT

private slots:
    void exposesSafeIdleStateWithoutClippingNotice();
};

void UpdateSettingsSectionTests::exposesSafeIdleStateWithoutClippingNotice()
{
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    UpdateSettingsSection section([]() { return false; });
    section.resize(980, 720);
    section.show();
    QCoreApplication::processEvents();

    auto *version = section.findChild<QLabel *>(
        QStringLiteral("currentVersionLabel")
    );
    auto *status = section.findChild<QLabel *>(
        QStringLiteral("updateStatusLabel")
    );
    auto *security = section.findChild<QLabel *>(
        QStringLiteral("updateSecurityNotice")
    );
    auto *check = section.findChild<QPushButton *>(
        QStringLiteral("checkForUpdatesButton")
    );
    auto *install = section.findChild<QPushButton *>(
        QStringLiteral("downloadAndInstallButton")
    );
    auto *notes = section.findChild<QTextBrowser *>(
        QStringLiteral("updateReleaseNotes")
    );
    auto *progress = section.findChild<QProgressBar *>(
        QStringLiteral("updateDownloadProgress")
    );

    QVERIFY(version);
    QVERIFY(version->text().contains(QStringLiteral("0.1.0")));
    QVERIFY(status);
    QVERIFY(status->wordWrap());
    QVERIFY(security);
    QVERIFY(security->wordWrap());
    QVERIFY(section.findChild<QScrollArea *>(
        QStringLiteral("updateSettingsScrollArea")
    ));
    QVERIFY(check && check->isEnabled());
    QVERIFY(install && !install->isEnabled());
    QVERIFY(install->styleSheet().contains(QStringLiteral("QPushButton:disabled")));
    QVERIFY(notes && notes->toPlainText().contains(QStringLiteral("检查")));
    QVERIFY(progress && progress->isHidden());
}

QTEST_MAIN(UpdateSettingsSectionTests)

#include "update_settings_section_tests.moc"
