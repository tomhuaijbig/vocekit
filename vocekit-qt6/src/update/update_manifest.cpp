#include "update_manifest.h"

#include "semantic_version.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace {

bool isSecureHttpUrl(const QUrl &url)
{
    return url.isValid()
        && url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0
        && !url.host().isEmpty()
        && url.userInfo().isEmpty();
}

QString versionWithoutPrefix(const QString &value)
{
    const QString trimmed = value.trimmed();
    return trimmed.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)
        ? trimmed.mid(1)
        : trimmed;
}

UpdateManifestParseResult invalidResult(const QString &error)
{
    UpdateManifestParseResult result;
    result.error = error;
    return result;
}

UpdateManifestParseResult validateManifest(UpdateManifest manifest)
{
    manifest.version = versionWithoutPrefix(manifest.version);
    manifest.sha256 = normalizedSha256(manifest.sha256);
    if (!SemanticVersion::parse(manifest.version).valid) {
        return invalidResult(QStringLiteral("更新清单中的版本号无效。"));
    }
    if (!isSecureHttpUrl(manifest.downloadUrl)) {
        return invalidResult(QStringLiteral("更新包必须使用有效的 HTTPS 地址。"));
    }
    if (manifest.sha256.isEmpty() && !isSecureHttpUrl(manifest.checksumUrl)) {
        return invalidResult(QStringLiteral("更新清单缺少 SHA-256 或校验文件。"));
    }
    if (!manifest.releasePageUrl.isEmpty()
        && !isSecureHttpUrl(manifest.releasePageUrl)) {
        return invalidResult(QStringLiteral("更新说明页面必须使用 HTTPS 地址。"));
    }
    UpdateManifestParseResult result;
    result.ok = true;
    result.manifest = manifest;
    return result;
}

UpdateManifestParseResult parseGenericManifest(const QJsonObject &root)
{
    UpdateManifest manifest;
    manifest.version = root.value(QStringLiteral("version")).toString();
    manifest.channel = root.value(QStringLiteral("channel"))
        .toString(QStringLiteral("stable"));
    manifest.releaseName = root.value(QStringLiteral("release_name")).toString();
    manifest.releaseNotes = root.value(QStringLiteral("release_notes")).toString();
    manifest.publishedAt = QDateTime::fromString(
        root.value(QStringLiteral("published_at")).toString(),
        Qt::ISODate
    );
    manifest.releasePageUrl = QUrl(
        root.value(QStringLiteral("release_page_url")).toString()
    );
    manifest.downloadUrl = QUrl(
        root.value(QStringLiteral("download_url")).toString()
    );
    manifest.checksumUrl = QUrl(
        root.value(QStringLiteral("checksum_url")).toString()
    );
    manifest.assetName = root.value(QStringLiteral("asset_name")).toString();
    manifest.sha256 = root.value(QStringLiteral("sha256")).toString();
    manifest.prerelease = root.value(QStringLiteral("prerelease")).toBool(false);
    return validateManifest(manifest);
}

UpdateManifestParseResult parseGitHubRelease(const QJsonObject &root)
{
    if (root.value(QStringLiteral("draft")).toBool(false)) {
        return invalidResult(QStringLiteral("草稿版本不能用于更新。"));
    }

    const QJsonArray assets = root.value(QStringLiteral("assets")).toArray();
    QJsonObject selected;
    for (const QJsonValue &value : assets) {
        const QJsonObject asset = value.toObject();
        if (asset.value(QStringLiteral("name")).toString()
            .compare(QStringLiteral("vocekit-qt6-portable.zip"), Qt::CaseInsensitive) == 0) {
            selected = asset;
            break;
        }
    }
    if (selected.isEmpty()) {
        for (const QJsonValue &value : assets) {
            const QJsonObject asset = value.toObject();
            const QString name = asset.value(QStringLiteral("name")).toString();
            if (name.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive)
                && name.contains(QStringLiteral("vocekit"), Qt::CaseInsensitive)
                && name.contains(QStringLiteral("qt6"), Qt::CaseInsensitive)) {
                selected = asset;
                break;
            }
        }
    }
    if (selected.isEmpty()) {
        return invalidResult(QStringLiteral("发布版本中没有找到 Qt 6 更新包。"));
    }

    UpdateManifest manifest;
    manifest.version = root.value(QStringLiteral("tag_name")).toString();
    manifest.channel = root.value(QStringLiteral("prerelease")).toBool(false)
        ? QStringLiteral("preview")
        : QStringLiteral("stable");
    manifest.releaseName = root.value(QStringLiteral("name")).toString();
    manifest.releaseNotes = root.value(QStringLiteral("body")).toString();
    manifest.publishedAt = QDateTime::fromString(
        root.value(QStringLiteral("published_at")).toString(),
        Qt::ISODate
    );
    manifest.releasePageUrl = QUrl(
        root.value(QStringLiteral("html_url")).toString()
    );
    manifest.assetName = selected.value(QStringLiteral("name")).toString();
    manifest.downloadUrl = QUrl(
        selected.value(QStringLiteral("browser_download_url")).toString()
    );
    const QString digest = selected.value(QStringLiteral("digest")).toString();
    if (digest.startsWith(QStringLiteral("sha256:"), Qt::CaseInsensitive)) {
        manifest.sha256 = digest.mid(7);
    }
    const QString checksumName = manifest.assetName + QStringLiteral(".sha256");
    for (const QJsonValue &value : assets) {
        const QJsonObject asset = value.toObject();
        if (asset.value(QStringLiteral("name")).toString()
            .compare(checksumName, Qt::CaseInsensitive) == 0) {
            manifest.checksumUrl = QUrl(
                asset.value(QStringLiteral("browser_download_url")).toString()
            );
            break;
        }
    }
    manifest.prerelease = root.value(QStringLiteral("prerelease")).toBool(false);
    return validateManifest(manifest);
}

} // namespace

QString normalizedSha256(const QString &value)
{
    QString normalized = value.trimmed().toLower();
    if (normalized.startsWith(QStringLiteral("sha256:"))) {
        normalized = normalized.mid(7);
    }
    static const QRegularExpression expression(QStringLiteral(R"(^[0-9a-f]{64}$)"));
    return expression.match(normalized).hasMatch() ? normalized : QString();
}

UpdateManifestParseResult parseUpdateManifest(const QByteArray &json)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return invalidResult(QStringLiteral("更新清单不是有效的 JSON：%1")
            .arg(parseError.errorString()));
    }
    const QJsonObject root = document.object();
    return root.contains(QStringLiteral("tag_name"))
        ? parseGitHubRelease(root)
        : parseGenericManifest(root);
}
