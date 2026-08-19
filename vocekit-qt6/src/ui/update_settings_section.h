#ifndef VOCEKIT_UPDATE_SETTINGS_SECTION_H
#define VOCEKIT_UPDATE_SETTINGS_SECTION_H

#include "../update/update_manifest.h"

#include <QWidget>
#include <functional>

class QLabel;
class QProgressBar;
class QPushButton;
class QTextBrowser;
class UpdateService;

class UpdateSettingsSection : public QWidget
{
public:
    explicit UpdateSettingsSection(
        const std::function<bool()> &useSystemProxyProvider,
        QWidget *parent = nullptr
    );

private:
    void setBusy(bool busy);
    bool useSystemProxy() const;
    void showManifest(const UpdateManifest &manifest);

    std::function<bool()> m_useSystemProxyProvider;
    UpdateService *m_service = nullptr;
    UpdateManifest m_availableManifest;
    QLabel *m_status = nullptr;
    QLabel *m_releaseTitle = nullptr;
    QTextBrowser *m_releaseNotes = nullptr;
    QProgressBar *m_progress = nullptr;
    QPushButton *m_checkButton = nullptr;
    QPushButton *m_installButton = nullptr;
    QPushButton *m_releasePageButton = nullptr;
};

#endif // VOCEKIT_UPDATE_SETTINGS_SECTION_H
