#ifndef VOCEKIT_VOICE_CONTROLLER_H
#define VOCEKIT_VOICE_CONTROLLER_H

#include "../config/app_settings_data.h"
#include "../domain/prompt_runtime_library.h"

#include <QObject>
#include <QSet>
#include <QString>

#include <functional>

class FloatingBar;
class VoiceControllerHost;
struct VocabularySuggestion;

// 控制器只读取运行快照；少量窗口状态通过统一回调保存。
struct VoiceControllerAccess
{
    std::function<AppSettingsData()> settingsSnapshotProvider;
    std::function<PromptRuntimeSnapshot()> promptSnapshotProvider;
    std::function<bool(const AppSettingsData &)> applyAndSave;
};

// 语音业务控制器的公开外壳。
// 具体实现放在 voice_controller.cpp 的 Impl 中，主窗口只依赖这组稳定方法。
class VoiceController : public QObject
{
public:
    VoiceController(
        const VoiceControllerAccess &access,
        FloatingBar *bar,
        VoiceControllerHost *host,
        QObject *parent = nullptr
    );
    ~VoiceController();

    void reload();
    void setActiveHoldFunctions(const QSet<QString> &ids);
    void handleHotkeyReleased(const QString &id);
    void handleScreenshotTrigger(const QString &id);
    VocabularySuggestion suggestVocabularyEntry(
        const QString &sourceText,
        const QString &scopeId,
        QString *error,
        const QString &editedText = QString(),
        const QString &extraContext = QString()
    );
    void handleHotkey(const QString &id);

private:
    class Impl;
    Impl *d;
};

#endif // VOCEKIT_VOICE_CONTROLLER_H
