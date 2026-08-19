#ifndef VOCEKIT_UPDATE_MANIFEST_H
#define VOCEKIT_UPDATE_MANIFEST_H

#include <QDateTime>
#include <QString>
#include <QUrl>

struct UpdateManifest
{
    QString version;
    QString channel = QStringLiteral("stable");
    QString releaseName;
    QString releaseNotes;
    QDateTime publishedAt;
    QUrl releasePageUrl;
    QUrl downloadUrl;
    QUrl checksumUrl;
    QString assetName;
    QString sha256;
    bool prerelease = false;
};

struct UpdateManifestParseResult
{
    bool ok = false;
    UpdateManifest manifest;
    QString error;
};

UpdateManifestParseResult parseUpdateManifest(const QByteArray &json);
QString normalizedSha256(const QString &value);

#endif // VOCEKIT_UPDATE_MANIFEST_H
