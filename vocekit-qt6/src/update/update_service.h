#ifndef VOCEKIT_UPDATE_SERVICE_H
#define VOCEKIT_UPDATE_SERVICE_H

#include "update_manifest.h"

#include <QCryptographicHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSaveFile>
#include <QTimer>
#include <memory>

class QNetworkReply;

class UpdateService : public QObject
{
    Q_OBJECT

public:
    explicit UpdateService(QObject *parent = nullptr);

    static QUrl defaultFeedUrl();
    static QString currentVersion();

    void checkForUpdates(bool useSystemProxy);
    void downloadAndInstall(
        const UpdateManifest &manifest,
        bool useSystemProxy
    );
    bool isBusy() const;

signals:
    void checkFinished(
        bool updateAvailable,
        const UpdateManifest &manifest,
        const QString &message
    );
    void downloadProgress(qint64 received, qint64 total);
    void operationFailed(const QString &message, const QString &detail);
    void updaterStarted(const QString &version);

private:
    enum class Operation
    {
        Idle,
        Feed,
        Checksum,
        Download
    };

    void startRequest(const QUrl &url, Operation operation, bool useSystemProxy);
    void finishFeed();
    void finishChecksum();
    void finishDownload();
    void fail(const QString &message, const QString &detail = QString());
    void resetReply();
    bool launchUpdater(const QString &packagePath, const UpdateManifest &manifest);

    QNetworkAccessManager m_manager;
    QNetworkReply *m_reply = nullptr;
    QTimer m_timeout;
    QByteArray m_responseBody;
    Operation m_operation = Operation::Idle;
    UpdateManifest m_manifest;
    bool m_useSystemProxy = false;
    std::unique_ptr<QSaveFile> m_downloadFile;
    std::unique_ptr<QCryptographicHash> m_downloadHash;
    QString m_downloadPath;
};

#endif // VOCEKIT_UPDATE_SERVICE_H
