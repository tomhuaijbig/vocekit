#ifndef VOCEKIT_SELECTION_CONTEXT_COORDINATOR_H
#define VOCEKIT_SELECTION_CONTEXT_COORDINATOR_H

#include "selection_context_policy.h"
#include "../config/app_settings_data.h"
#include "../input/selection_observer.h"
#include "../input/selection_probe_runner.h"

#include <QObject>

#include <functional>

struct SelectionContextCoordinatorAccess
{
    std::function<SelectionContextSettings()> settingsSnapshot;
    std::function<bool()> strongSelectionEnabled;
    std::function<void(
        const SelectionProbeRequest &request,
        bool strongSelectionEnabled,
        quint64 generation,
        const SelectionProbeRunnerCallbacks &callbacks
    )> startProbe;
    std::function<void()> cancelProbe;
    std::function<quint32()> currentProcessId;
    std::function<bool(SelectedTextNativeWindowHandle)> targetWindowValid;
    std::function<SelectedTextNativeWindowHandle()> currentForegroundWindow;
    std::function<void(
        const SelectionSnapshot &,
        bool keyboardNavigationMode
    )> showToolbar;
    std::function<void()> hideToolbar;
    std::function<void()> closeUnpinnedResult;
    std::function<void()> cancelActiveAction;
    std::function<bool(SelectedTextNativeWindowHandle)> ownsSurfaceWindow;
    std::function<void(SelectionContextEligibility)> showManualFailure;
    std::function<void(const QString &eventId, int textLength)> logMetadata;
};

class QTimer;

class SelectionContextCoordinator : public QObject
{
    Q_OBJECT

public:
    explicit SelectionContextCoordinator(
        const SelectionContextCoordinatorAccess &access,
        QObject *parent = nullptr
    );
    ~SelectionContextCoordinator() override;

    void start();
    void stop();
    void refreshSettings();
    void handleObservation(const SelectionObservation &observation);
    void triggerFallbackShortcut();
    void pauseForMinutes(int minutes);
    void resume();
    bool isPaused() const;
    void setResultPinned(bool pinned);

private:
    void scheduleAutomaticProbe(const SelectionObservation &observation);
    void beginProbe(
        const SelectionObservation &observation,
        bool manual
    );
    void acceptProbeResult(
        quint64 generation,
        const SelectionSnapshot &snapshot
    );
    void handleProbeTimeout(quint64 generation);
    bool invalidateProbe();
    void closeCurrentUnpinnedSession();
    bool ownsSurface(SelectedTextNativeWindowHandle window) const;

    SelectionContextCoordinatorAccess m_access;
    SelectionContextSettings m_settings;
    QTimer *m_debounceTimer = nullptr;
    QTimer *m_pauseTimer = nullptr;
    SelectionObservation m_pendingObservation;
    SelectionSnapshot m_lastSnapshot;
    quint64 m_generation = 0;
    quint64 m_manualGeneration = 0;
    quint64 m_manualFailureShownGeneration = 0;
    bool m_started = false;
    bool m_paused = false;
    bool m_hasCurrentSession = false;
    bool m_resultPinned = false;
};

#endif // VOCEKIT_SELECTION_CONTEXT_COORDINATOR_H
