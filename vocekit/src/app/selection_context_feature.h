#ifndef VOCEKIT_SELECTION_CONTEXT_FEATURE_H
#define VOCEKIT_SELECTION_CONTEXT_FEATURE_H

#include "../config/app_settings_data.h"
#include "../domain/prompt_runtime_library.h"
#include "../input/selection_observer.h"
#include "../input/selection_probe_runner.h"
#include "../output/clipboard_writer.h"
#include "../tasks/selection_context_model_runner.h"

#include <QObject>
#include <QRect>

#include <functional>

struct SelectionContextFeatureAccess
{
    std::function<AppSettingsData()> settingsSnapshot;
    std::function<PromptRuntimeSnapshot()> promptSnapshot;
    std::function<void(const QString &, const QString &)>
        openVocabularyEditor;
    std::function<bool(const QString &executable)> blockApplication;
    std::function<void()> openSettings;
    std::function<bool(
        const QString &actionId,
        const QString &modelId
    )> ensureNetworkConsent;
    std::function<void(
        const QString &eventId,
        const QString &actionId,
        int textLength,
        qint64 elapsedMs
    )> logMetadata;
};

// Platform and worker seams are grouped separately so the production feature
// owns the real observer/runners while focused tests remain deterministic.
struct SelectionContextFeatureDependencies
{
    std::function<bool(
        SelectedTextNativeWindowHandle notificationWindow,
        QString *error
    )> installObserver;
    std::function<void()> uninstallObserver;
    std::function<void(
        const std::function<void(const SelectionObservation &)> &callback
    )> setObservationCallback;
    std::function<void(bool)> setObserverAutomaticEnabled;
    std::function<void(bool)> setObserverKeyboardEnabled;
    std::function<void(bool)> setObserverPaused;
    std::function<void(bool)> setObserverSurfaceVisible;

    std::function<void(
        const SelectionProbeRequest &request,
        bool strongSelectionEnabled,
        quint64 generation,
        const SelectionProbeRunnerCallbacks &callbacks
    )> startProbe;
    std::function<void()> cancelProbe;
    std::function<void(
        SelectedTextNativeWindowHandle window,
        quint64 generation,
        const std::function<void(quint64, bool)> &completed
    )> validateSelectionAsync;

    std::function<quint32()> currentProcessId;
    std::function<bool(SelectedTextNativeWindowHandle)> targetWindowValid;
    std::function<SelectedTextNativeWindowHandle()> currentForegroundWindow;
    std::function<QRect(const QPoint &)> availableGeometry;
    std::function<bool(const QString &)> copyText;
    std::function<ClipboardWriteResult(
        const QString &,
        SelectedTextNativeWindowHandle
    )> replaceSelection;
    SelectionContextModelRunnerAccess modelRunnerAccess;
};

class SelectionContextFeature : public QObject
{
    Q_OBJECT

public:
    explicit SelectionContextFeature(
        const SelectionContextFeatureAccess &access,
        QObject *parent = nullptr
    );
    SelectionContextFeature(
        const SelectionContextFeatureAccess &access,
        const SelectionContextFeatureDependencies &dependencies,
        QObject *parent = nullptr
    );
    ~SelectionContextFeature() override;

    void start();
    void stop();
    void refresh();
    void triggerFallbackShortcut();
    void pauseForMinutes(int minutes);
    void resume();
    bool isPaused() const;
    bool isEnabled() const;
    bool hooksInstalled() const;
    int pinnedResultCount() const;

private:
    class Impl;
    Impl *m_impl = nullptr;
};

#endif // VOCEKIT_SELECTION_CONTEXT_FEATURE_H
