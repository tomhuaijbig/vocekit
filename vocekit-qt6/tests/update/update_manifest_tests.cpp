#include "../../src/update/semantic_version.h"
#include "../../src/update/update_manifest.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

class UpdateManifestTests : public QObject
{
    Q_OBJECT

private slots:
    void comparesSemanticVersions();
    void parsesGenericManifest();
    void parsesGitHubReleaseAndCompanionChecksum();
    void rejectsInsecureOrIncompleteManifest();
};

void UpdateManifestTests::comparesSemanticVersions()
{
    QVERIFY(isVersionNewer(QStringLiteral("1.2.4"), QStringLiteral("1.2.3")));
    QVERIFY(isVersionNewer(QStringLiteral("v2.0.0"), QStringLiteral("1.99.99")));
    QVERIFY(isVersionNewer(QStringLiteral("1.0.0"), QStringLiteral("1.0.0-rc.1")));
    QVERIFY(!isVersionNewer(QStringLiteral("1.0.0-rc.1"), QStringLiteral("1.0.0")));
    QVERIFY(!isVersionNewer(QStringLiteral("1.2.3"), QStringLiteral("1.2.3")));
    QVERIFY(!isVersionNewer(QStringLiteral("not-a-version"), QStringLiteral("1.2.3")));
}

void UpdateManifestTests::parsesGenericManifest()
{
    const QByteArray json = R"JSON({
        "version": "1.4.0",
        "channel": "stable",
        "release_name": "VoceKit 1.4.0",
        "release_notes": "修复与更新",
        "release_page_url": "https://example.com/releases/v1.4.0",
        "download_url": "https://example.com/releases/vocekit-qt6-portable.zip",
        "sha256": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    })JSON";

    const UpdateManifestParseResult parsed = parseUpdateManifest(json);
    QVERIFY2(parsed.ok, qPrintable(parsed.error));
    QCOMPARE(parsed.manifest.version, QStringLiteral("1.4.0"));
    QCOMPARE(parsed.manifest.channel, QStringLiteral("stable"));
    QCOMPARE(parsed.manifest.sha256, QString(64, QLatin1Char('a')));
    QCOMPARE(
        parsed.manifest.downloadUrl,
        QUrl(QStringLiteral("https://example.com/releases/vocekit-qt6-portable.zip"))
    );
}

void UpdateManifestTests::parsesGitHubReleaseAndCompanionChecksum()
{
    const QByteArray json = R"JSON({
        "tag_name": "v1.5.0",
        "name": "VoceKit 1.5.0",
        "body": "更新说明",
        "html_url": "https://github.com/example/vocekit/releases/tag/v1.5.0",
        "draft": false,
        "prerelease": false,
        "assets": [
            {
                "name": "vocekit-qt6-portable.zip",
                "browser_download_url": "https://github.com/example/vocekit/releases/download/v1.5.0/vocekit-qt6-portable.zip",
                "digest": null
            },
            {
                "name": "vocekit-qt6-portable.zip.sha256",
                "browser_download_url": "https://github.com/example/vocekit/releases/download/v1.5.0/vocekit-qt6-portable.zip.sha256"
            }
        ]
    })JSON";

    const UpdateManifestParseResult parsed = parseUpdateManifest(json);
    QVERIFY2(parsed.ok, qPrintable(parsed.error));
    QCOMPARE(parsed.manifest.version, QStringLiteral("1.5.0"));
    QVERIFY(parsed.manifest.sha256.isEmpty());
    QCOMPARE(
        parsed.manifest.checksumUrl.fileName(),
        QStringLiteral("vocekit-qt6-portable.zip.sha256")
    );
}

void UpdateManifestTests::rejectsInsecureOrIncompleteManifest()
{
    const QByteArray insecure = R"JSON({
        "version": "1.4.0",
        "download_url": "http://example.com/vocekit.zip",
        "sha256": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    })JSON";
    QVERIFY(!parseUpdateManifest(insecure).ok);

    const QByteArray missingChecksum = R"JSON({
        "version": "1.4.0",
        "download_url": "https://example.com/vocekit.zip"
    })JSON";
    QVERIFY(!parseUpdateManifest(missingChecksum).ok);
}

QTEST_APPLESS_MAIN(UpdateManifestTests)

#include "update_manifest_tests.moc"
