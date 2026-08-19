#include "update_settings_section.h"

#include "ui_style.h"
#include "../update/update_service.h"

#include <QtWidgets>

namespace {

QString updateTr8(const char *text)
{
    return QString::fromUtf8(text);
}

QFrame *newCard(QWidget *parent)
{
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("card"));
    card->setStyleSheet(cardStyle());
    return card;
}

} // namespace

UpdateSettingsSection::UpdateSettingsSection(
    const std::function<bool()> &useSystemProxyProvider,
    QWidget *parent)
    : QWidget(parent),
      m_useSystemProxyProvider(useSystemProxyProvider),
      m_service(new UpdateService(this))
{
    setObjectName(QStringLiteral("updateSettingsSection"));
    setFont(appFont());

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *scrollArea = new QScrollArea(this);
    scrollArea->setObjectName(QStringLiteral("updateSettingsScrollArea"));
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    auto *content = new QWidget(scrollArea);
    auto *root = new QVBoxLayout(content);
    root->setContentsMargins(18, 18, 18, 18);
    root->setSpacing(14);

    QFrame *versionCard = newCard(this);
    auto *versionLayout = new QVBoxLayout(versionCard);
    versionLayout->setContentsMargins(18, 16, 18, 16);
    versionLayout->setSpacing(8);

    auto *heading = new QLabel(updateTr8("软件更新"), versionCard);
    heading->setFont(appFont(18, QFont::DemiBold));
    versionLayout->addWidget(heading);

    auto *version = new QLabel(
        updateTr8("当前版本：") + UpdateService::currentVersion(),
        versionCard
    );
    version->setObjectName(QStringLiteral("currentVersionLabel"));
    versionLayout->addWidget(version);

    auto *channel = new QLabel(
        UpdateService::updatesConfigured()
            ? updateTr8("更新通道：稳定版（已配置公开更新源）")
            : updateTr8("更新通道：未配置（当前构建不可联网更新）"),
        versionCard
    );
    channel->setObjectName(QStringLiteral("updateChannelLabel"));
    channel->setStyleSheet(QStringLiteral("color: #667085;"));
    versionLayout->addWidget(channel);

    m_status = new QLabel(
        UpdateService::updatesConfigured()
            ? updateTr8("尚未检查更新。")
            : updateTr8("这是开发或内部测试构建，发布者尚未配置公开更新源。"),
        versionCard
    );
    m_status->setObjectName(QStringLiteral("updateStatusLabel"));
    m_status->setWordWrap(true);
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    versionLayout->addWidget(m_status);

    m_progress = new QProgressBar(versionCard);
    m_progress->setObjectName(QStringLiteral("updateDownloadProgress"));
    m_progress->setRange(0, 100);
    m_progress->hide();
    versionLayout->addWidget(m_progress);

    auto *actions = new QHBoxLayout;
    m_checkButton = new QPushButton(updateTr8("检查更新"), versionCard);
    m_checkButton->setObjectName(QStringLiteral("checkForUpdatesButton"));
    m_checkButton->setStyleSheet(
        buttonStyle(QStringLiteral("#111827"))
        + QStringLiteral(
            "QPushButton:disabled {"
            "  background: #d0d5dd;"
            "  color: #667085;"
            "}"
        )
    );
    m_checkButton->setEnabled(UpdateService::updatesConfigured());
    m_installButton = new QPushButton(updateTr8("下载并安装"), versionCard);
    m_installButton->setObjectName(QStringLiteral("downloadAndInstallButton"));
    m_installButton->setStyleSheet(
        buttonStyle(QStringLiteral("#2563eb"))
        + QStringLiteral(
            "QPushButton:disabled {"
            "  background: #d0d5dd;"
            "  color: #667085;"
            "}"
        )
    );
    m_installButton->setEnabled(false);
    m_releasePageButton = new QPushButton(updateTr8("查看发布页"), versionCard);
    m_releasePageButton->setObjectName(QStringLiteral("openReleasePageButton"));
    m_releasePageButton->setStyleSheet(
        buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#344054"))
    );
    m_releasePageButton->setVisible(false);
    actions->addWidget(m_checkButton);
    actions->addWidget(m_installButton);
    actions->addWidget(m_releasePageButton);
    actions->addStretch();
    versionLayout->addLayout(actions);
    root->addWidget(versionCard);

    QFrame *notesCard = newCard(this);
    auto *notesLayout = new QVBoxLayout(notesCard);
    notesLayout->setContentsMargins(18, 16, 18, 16);
    notesLayout->setSpacing(8);
    m_releaseTitle = new QLabel(updateTr8("更新说明"), notesCard);
    m_releaseTitle->setFont(appFont(16, QFont::DemiBold));
    notesLayout->addWidget(m_releaseTitle);
    m_releaseNotes = new QTextBrowser(notesCard);
    m_releaseNotes->setObjectName(QStringLiteral("updateReleaseNotes"));
    m_releaseNotes->setOpenExternalLinks(true);
    m_releaseNotes->setMinimumHeight(150);
    m_releaseNotes->setPlainText(updateTr8("检查到新版本后，这里会显示发布说明。"));
    notesLayout->addWidget(m_releaseNotes);
    root->addWidget(notesCard, 1);

    auto *security = new QLabel(
        updateTr8(
            "安全策略：仅接受 HTTPS 更新源；下载后必须通过 SHA-256 校验；"
            "主程序退出后才替换运行文件；config、prompts、records、logs 等用户数据不会被覆盖；"
            "替换失败会自动恢复旧文件。正式对外发布前仍应配置代码签名证书。"
        ),
        this
    );
    security->setObjectName(QStringLiteral("updateSecurityNotice"));
    security->setWordWrap(true);
    security->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    security->setStyleSheet(QStringLiteral("color: #667085;"));
    root->addWidget(security);
    scrollArea->setWidget(content);
    outer->addWidget(scrollArea);

    connect(m_checkButton, &QPushButton::clicked, this, [this]() {
        setBusy(true);
        m_status->setText(updateTr8("正在连接更新服务器……"));
        m_availableManifest = UpdateManifest();
        m_installButton->setEnabled(false);
        m_service->checkForUpdates(useSystemProxy());
    });
    connect(m_installButton, &QPushButton::clicked, this, [this]() {
        if (m_availableManifest.version.isEmpty()) {
            return;
        }
        const QMessageBox::StandardButton choice = QMessageBox::question(
            this,
            updateTr8("安装更新"),
            updateTr8("更新包下载并校验后，VoceKit 会自动退出、更新并重新启动。现在继续吗？"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
        );
        if (choice != QMessageBox::Yes) {
            return;
        }
        setBusy(true);
        m_progress->setValue(0);
        m_progress->show();
        m_status->setText(updateTr8("正在下载并校验更新包……"));
        m_service->downloadAndInstall(m_availableManifest, useSystemProxy());
    });
    connect(m_releasePageButton, &QPushButton::clicked, this, [this]() {
        if (!m_availableManifest.releasePageUrl.isEmpty()) {
            QDesktopServices::openUrl(m_availableManifest.releasePageUrl);
        }
    });
    connect(
        m_service,
        &UpdateService::checkFinished,
        this,
        [this](bool available, const UpdateManifest &manifest, const QString &message) {
            setBusy(false);
            m_status->setText(message);
            showManifest(manifest);
            if (available) {
                m_availableManifest = manifest;
                m_installButton->setEnabled(true);
            }
        }
    );
    connect(
        m_service,
        &UpdateService::downloadProgress,
        this,
        [this](qint64 received, qint64 total) {
            if (total <= 0) {
                m_progress->setRange(0, 0);
                return;
            }
            m_progress->setRange(0, 100);
            m_progress->setValue(int((received * 100) / total));
        }
    );
    connect(
        m_service,
        &UpdateService::operationFailed,
        this,
        [this](const QString &message, const QString &detail) {
            setBusy(false);
            m_progress->hide();
            m_status->setText(
                detail.trimmed().isEmpty()
                    ? message
                    : message + QStringLiteral("\n") + detail
            );
        }
    );
    connect(
        m_service,
        &UpdateService::updaterStarted,
        this,
        [this](const QString &version) {
            m_status->setText(
                updateTr8("已验证版本 %1，正在退出并交给独立更新程序处理……")
                    .arg(version)
            );
        }
    );
}

void UpdateSettingsSection::setBusy(bool busy)
{
    m_checkButton->setEnabled(!busy && UpdateService::updatesConfigured());
    m_installButton->setEnabled(
        !busy && !m_availableManifest.version.isEmpty()
    );
}

bool UpdateSettingsSection::useSystemProxy() const
{
    return m_useSystemProxyProvider ? m_useSystemProxyProvider() : false;
}

void UpdateSettingsSection::showManifest(const UpdateManifest &manifest)
{
    const QString title = manifest.releaseName.trimmed().isEmpty()
        ? updateTr8("版本 %1").arg(manifest.version)
        : manifest.releaseName.trimmed();
    m_releaseTitle->setText(updateTr8("更新说明 · ") + title);
    m_releaseNotes->setPlainText(
        manifest.releaseNotes.trimmed().isEmpty()
            ? updateTr8("该版本没有提供更新说明。")
            : manifest.releaseNotes
    );
    m_releasePageButton->setVisible(!manifest.releasePageUrl.isEmpty());
}
