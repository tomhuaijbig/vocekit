#include "update_service.h"

#include "semantic_version.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QNetworkProxy>
#include <QNetworkProxyFactory>
#include <QNetworkProxyQuery>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

#ifndef VOCEKIT_UPDATE_FEED_URL
#define VOCEKIT_UPDATE_FEED_URL ""
#endif

namespace {

QString networkDetail(QNetworkReply *reply)
{
    if (!reply) {
        return QString();
    }
    const int status = reply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute
    ).toInt();
    return status > 0
        ? QStringLiteral("HTTP %1：%2").arg(status).arg(reply->errorString())
        : reply->errorString();
}

bool successfulHttpStatus(QNetworkReply *reply)
{
    const int status = reply
        ? reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()
        : 0;
    return status >= 200 && status < 300;
}

bool isSecureUpdateUrl(const QUrl &url)
{
    return url.isValid()
        && url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0
        && !url.host().isEmpty()
        && url.userInfo().isEmpty();
}

void applyProxy(
    QNetworkAccessManager *manager,
    const QUrl &url,
    bool useSystemProxy)
{
    if (!manager) {
        return;
    }
    if (!useSystemProxy) {
        manager->setProxy(QNetworkProxy::NoProxy);
        return;
    }
    const QList<QNetworkProxy> proxies =
        QNetworkProxyFactory::systemProxyForQuery(QNetworkProxyQuery(url));
    manager->setProxy(
        proxies.isEmpty() ? QNetworkProxy::NoProxy : proxies.constFirst()
    );
}

} // namespace

UpdateService::UpdateService(QObject *parent)
    : QObject(parent)
{
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(30000);
    connect(&m_timeout, &QTimer::timeout, this, [this]() {
        if (m_reply) {
            m_reply->disconnect(this);
            m_reply->abort();
        }
        fail(QStringLiteral("更新请求超时。"));
    });
}

QUrl UpdateService::defaultFeedUrl()
{
    return QUrl(QString::fromUtf8(VOCEKIT_UPDATE_FEED_URL));
}

bool UpdateService::updatesConfigured()
{
    return isSecureUpdateUrl(defaultFeedUrl());
}

QString UpdateService::currentVersion()
{
    const QString version = QCoreApplication::applicationVersion().trimmed();
    return version.isEmpty() ? QStringLiteral("0.0.0") : version;
}

bool UpdateService::isBusy() const
{
    return m_operation != Operation::Idle;
}

void UpdateService::checkForUpdates(bool useSystemProxy)
{
    if (isBusy()) {
        return;
    }
    if (!updatesConfigured()) {
        fail(
            QStringLiteral("此构建未配置公开更新源，在线更新不可用。"),
            QStringLiteral("发布构建必须通过 -UpdateFeedUrl 注入可公开访问的 HTTPS 地址。")
        );
        return;
    }
    m_manifest = UpdateManifest();
    startRequest(defaultFeedUrl(), Operation::Feed, useSystemProxy);
}

void UpdateService::downloadAndInstall(
    const UpdateManifest &manifest,
    bool useSystemProxy)
{
    if (isBusy()) {
        return;
    }
    if (normalizedSha256(manifest.sha256).isEmpty()) {
        fail(QStringLiteral("更新包缺少有效的 SHA-256，已拒绝下载。"));
        return;
    }
    if (!isVersionNewer(manifest.version, currentVersion())) {
        fail(QStringLiteral("目标版本不高于当前版本，已取消更新。"));
        return;
    }

    const QString root = QDir(
        QStandardPaths::writableLocation(QStandardPaths::TempLocation)
    ).filePath(QStringLiteral("VoceKit/updates/") + manifest.version);
    if (!QDir().mkpath(root)) {
        fail(QStringLiteral("无法创建更新下载目录。"), root);
        return;
    }
    m_downloadPath = QDir(root).filePath(QStringLiteral("vocekit-update.zip"));
    m_downloadFile.reset(new QSaveFile(m_downloadPath));
    if (!m_downloadFile->open(QIODevice::WriteOnly)) {
        fail(QStringLiteral("无法创建更新包文件。"), m_downloadFile->errorString());
        return;
    }
    m_downloadHash.reset(new QCryptographicHash(QCryptographicHash::Sha256));
    m_manifest = manifest;
    startRequest(manifest.downloadUrl, Operation::Download, useSystemProxy);
}

void UpdateService::startRequest(
    const QUrl &url,
    Operation operation,
    bool useSystemProxy)
{
    if (!isSecureUpdateUrl(url)) {
        fail(QStringLiteral("更新服务只允许 HTTPS 地址。"), url.toString());
        return;
    }
    m_operation = operation;
    m_useSystemProxy = useSystemProxy;
    m_responseBody.clear();
    applyProxy(&m_manager, url, useSystemProxy);

    QNetworkRequest request(url);
    request.setAttribute(
        QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::NoLessSafeRedirectPolicy
    );
    request.setRawHeader(
        QByteArrayLiteral("User-Agent"),
        QByteArrayLiteral("VoceKit/") + currentVersion().toUtf8()
    );
    request.setRawHeader(
        QByteArrayLiteral("Accept"),
        QByteArrayLiteral("application/vnd.github+json, application/json")
    );
    m_reply = m_manager.get(request);

    connect(m_reply, &QNetworkReply::readyRead, this, [this]() {
        if (!m_reply) {
            return;
        }
        const QByteArray chunk = m_reply->readAll();
        if (m_operation == Operation::Download) {
            if (!m_downloadFile || m_downloadFile->write(chunk) != chunk.size()) {
                const QString detail = m_downloadFile
                    ? m_downloadFile->errorString()
                    : QString();
                m_reply->disconnect(this);
                m_reply->abort();
                fail(QStringLiteral("更新包写入失败。"), detail);
                return;
            }
            m_downloadHash->addData(chunk);
        } else {
            const qsizetype responseLimit = m_operation == Operation::Feed
                ? qsizetype(2 * 1024 * 1024)
                : qsizetype(128 * 1024);
            if (m_responseBody.size() + chunk.size() > responseLimit) {
                m_reply->disconnect(this);
                m_reply->abort();
                fail(QStringLiteral("更新服务器响应过大，已停止读取。"));
                return;
            }
            m_responseBody.append(chunk);
        }
    });
    if (operation == Operation::Download) {
        connect(
            m_reply,
            &QNetworkReply::downloadProgress,
            this,
            &UpdateService::downloadProgress
        );
    }
    connect(m_reply, &QNetworkReply::finished, this, [this]() {
        if (!m_reply || m_operation == Operation::Idle) {
            return;
        }
        if (m_reply->error() != QNetworkReply::NoError
            || !successfulHttpStatus(m_reply)) {
            const int status = m_reply->attribute(
                QNetworkRequest::HttpStatusCodeAttribute
            ).toInt();
            if (m_operation == Operation::Feed && status == 404) {
                fail(
                    QStringLiteral("没有找到公开更新信息。"),
                    QStringLiteral("尚未发布正式版本，或更新仓库不能被未登录用户访问。")
                );
                return;
            }
            fail(QStringLiteral("更新服务器请求失败。"), networkDetail(m_reply));
            return;
        }
        switch (m_operation) {
        case Operation::Feed:
            finishFeed();
            break;
        case Operation::Checksum:
            finishChecksum();
            break;
        case Operation::Download:
            finishDownload();
            break;
        case Operation::Idle:
            break;
        }
    });
    m_timeout.start();
}

void UpdateService::finishFeed()
{
    const QByteArray body = m_responseBody + (m_reply ? m_reply->readAll() : QByteArray());
    const UpdateManifestParseResult parsed = parseUpdateManifest(body);
    if (!parsed.ok) {
        fail(QStringLiteral("无法读取更新信息。"), parsed.error);
        return;
    }
    m_manifest = parsed.manifest;
    if (m_manifest.sha256.isEmpty()) {
        resetReply();
        startRequest(m_manifest.checksumUrl, Operation::Checksum, m_useSystemProxy);
        return;
    }
    const bool available = isVersionNewer(m_manifest.version, currentVersion());
    resetReply();
    emit checkFinished(
        available,
        m_manifest,
        available
            ? QStringLiteral("发现新版本 %1。").arg(m_manifest.version)
            : QStringLiteral("当前已是最新版本。")
    );
}

void UpdateService::finishChecksum()
{
    const QByteArray body = m_responseBody + (m_reply ? m_reply->readAll() : QByteArray());
    static const QRegularExpression hashPattern(QStringLiteral(R"(([0-9A-Fa-f]{64}))"));
    const QRegularExpressionMatch match = hashPattern.match(QString::fromUtf8(body));
    m_manifest.sha256 = match.hasMatch()
        ? normalizedSha256(match.captured(1))
        : QString();
    if (m_manifest.sha256.isEmpty()) {
        fail(QStringLiteral("发布版本的 SHA-256 校验文件无效。"));
        return;
    }
    const bool available = isVersionNewer(m_manifest.version, currentVersion());
    resetReply();
    emit checkFinished(
        available,
        m_manifest,
        available
            ? QStringLiteral("发现新版本 %1。").arg(m_manifest.version)
            : QStringLiteral("当前已是最新版本。")
    );
}

void UpdateService::finishDownload()
{
    if (m_reply) {
        const QByteArray finalChunk = m_reply->readAll();
        if (!finalChunk.isEmpty()) {
            if (!m_downloadFile
                || m_downloadFile->write(finalChunk) != finalChunk.size()) {
                fail(QStringLiteral("更新包写入失败。"));
                return;
            }
            m_downloadHash->addData(finalChunk);
        }
    }
    const QString actual = QString::fromLatin1(m_downloadHash->result().toHex());
    const QString expected = normalizedSha256(m_manifest.sha256);
    if (actual != expected) {
        if (m_downloadFile) {
            m_downloadFile->cancelWriting();
        }
        fail(
            QStringLiteral("更新包完整性校验失败，已拒绝安装。"),
            QStringLiteral("期望 %1，实际 %2").arg(expected, actual)
        );
        return;
    }
    if (!m_downloadFile || !m_downloadFile->commit()) {
        fail(
            QStringLiteral("无法保存已验证的更新包。"),
            m_downloadFile ? m_downloadFile->errorString() : QString()
        );
        return;
    }
    const QString packagePath = m_downloadPath;
    const UpdateManifest manifest = m_manifest;
    resetReply();
    m_downloadFile.reset();
    m_downloadHash.reset();
    if (!launchUpdater(packagePath, manifest)) {
        return;
    }
    emit updaterStarted(manifest.version);
}

void UpdateService::fail(const QString &message, const QString &detail)
{
    if (m_downloadFile) {
        m_downloadFile->cancelWriting();
    }
    resetReply();
    m_downloadFile.reset();
    m_downloadHash.reset();
    emit operationFailed(message, detail);
}

void UpdateService::resetReply()
{
    m_timeout.stop();
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    m_operation = Operation::Idle;
    m_responseBody.clear();
}

bool UpdateService::launchUpdater(
    const QString &packagePath,
    const UpdateManifest &manifest)
{
    const QString installDir = QCoreApplication::applicationDirPath();
    const QString script = QDir(installDir).filePath(
        QStringLiteral("updater/vocekit-update.ps1")
    );
    const QString portableMarker = QDir(installDir).filePath(
        QStringLiteral(".vocekit-portable")
    );
    if (!QFileInfo::exists(script) || !QFileInfo::exists(portableMarker)) {
        fail(
            QStringLiteral("当前是开发构建，不能直接覆盖安装。"),
            QStringLiteral("请先运行 deploy.ps1 生成带独立更新程序的便携版。")
        );
        return false;
    }

    const QString executable = QDir(installDir).filePath(QStringLiteral("vocekit.exe"));
    const QStringList arguments = {
        QStringLiteral("-NoProfile"),
        QStringLiteral("-NonInteractive"),
        QStringLiteral("-ExecutionPolicy"),
        QStringLiteral("Bypass"),
        QStringLiteral("-WindowStyle"),
        QStringLiteral("Hidden"),
        QStringLiteral("-File"),
        script,
        QStringLiteral("-PackagePath"),
        packagePath,
        QStringLiteral("-InstallDir"),
        installDir,
        QStringLiteral("-ExpectedSha256"),
        manifest.sha256,
        QStringLiteral("-TargetVersion"),
        manifest.version,
        QStringLiteral("-WaitForProcessId"),
        QString::number(QCoreApplication::applicationPid()),
        QStringLiteral("-RestartExecutable"),
        executable
    };
    qint64 processId = 0;
    if (!QProcess::startDetached(
            QStringLiteral("powershell.exe"),
            arguments,
            installDir,
            &processId)) {
        fail(QStringLiteral("无法启动独立更新程序。"), script);
        return false;
    }
    QTimer::singleShot(0, QCoreApplication::instance(), []() {
        QCoreApplication::quit();
    });
    return true;
}
