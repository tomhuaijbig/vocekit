#include "../../src/config/app_paths.h"

#include <QDir>
#include <QTemporaryDir>
#include <QtTest>

class AppPathsTests : public QObject
{
    Q_OBJECT

private slots:
    void buildDirectoryUsesParentConfiguration();
    void portableDirectoryKeepsLocalConfiguration();
};

void AppPathsTests::buildDirectoryUsesParentConfiguration()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());

    QDir root(temporary.path());
    QVERIFY(root.mkpath(QStringLiteral("vocekit/release")));
    QVERIFY(root.mkpath(QStringLiteral("vocekit/debug")));
    const QString applicationRoot = root.filePath(QStringLiteral("vocekit"));

    QCOMPARE(
        appBasePathForApplicationDir(
            root.filePath(QStringLiteral("vocekit/release"))
        ),
        QDir(applicationRoot).absolutePath()
    );
    QCOMPARE(
        appConfigFilePathForApplicationDir(
            root.filePath(QStringLiteral("vocekit/release")),
            QStringLiteral("settings.json")
        ),
        QDir(applicationRoot).filePath(QStringLiteral("config/settings.json"))
    );
    QCOMPARE(
        appConfigFilePathForApplicationDir(
            root.filePath(QStringLiteral("vocekit/debug")),
            QStringLiteral("secrets.json")
        ),
        QDir(applicationRoot).filePath(QStringLiteral("config/secrets.json"))
    );
}

void AppPathsTests::portableDirectoryKeepsLocalConfiguration()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());

    QDir root(temporary.path());
    QVERIFY(root.mkpath(QStringLiteral("vocekit-package")));
    const QString package = root.filePath(QStringLiteral("vocekit-package"));

    QCOMPARE(
        appBasePathForApplicationDir(package),
        QDir(package).absolutePath()
    );
    QCOMPARE(
        appConfigFilePathForApplicationDir(
            package,
            QStringLiteral("settings.json")
        ),
        QDir(package).filePath(QStringLiteral("config/settings.json"))
    );
}

QTEST_APPLESS_MAIN(AppPathsTests)

#include "app_paths_tests.moc"
