#ifndef VOCEKIT_HUB_WINDOW_H
#define VOCEKIT_HUB_WINDOW_H

#include "../controllers/voice_controller_host.h"
#include "../domain/app_legacy_types.h"

#include <QMainWindow>

#include <functional>

class ApplicationEvents;
class FloatingBar;
struct HubWindowAccess;

// 主窗口公开接口只保留程序装配层需要的操作，页面构建和业务细节隐藏在实现文件中。
class HubWindow : public QMainWindow, public VoiceControllerHost
{
public:
    ~HubWindow() override;

    virtual void showSettingsPage(int initialTab = 0) = 0;
    virtual void setApplicationEvents(ApplicationEvents *events) = 0;
    virtual void openFaqById(const QString &faqId) = 0;

protected:
    explicit HubWindow(QWidget *parent = nullptr);
};

HubWindow *createHubWindow(
    const HubWindowAccess &settingsAccess,
    FloatingBar *floatingBar = nullptr,
    const std::function<void()> &onSettingsChanged = std::function<void()>(),
    const VocabularyAiCallback &onVocabularyAi = VocabularyAiCallback(),
    QWidget *parent = nullptr
);

#endif // VOCEKIT_HUB_WINDOW_H
