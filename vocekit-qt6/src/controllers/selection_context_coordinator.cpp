#include "selection_context_coordinator.h"

#include <QPointer>
#include <QTimer>

#include <limits>

SelectionContextCoordinator::SelectionContextCoordinator(
    const SelectionContextCoordinatorAccess &access,
    QObject *parent)
    : QObject(parent),
      m_access(access)
{
    m_debounceTimer = new QTimer(this);
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(160);
    connect(m_debounceTimer, &QTimer::timeout, this, [this]() {
        beginProbe(m_pendingObservation, false);
    });

    m_pauseTimer = new QTimer(this);
    m_pauseTimer->setSingleShot(true);
    connect(m_pauseTimer, &QTimer::timeout, this, [this]() {
        resume();
    });
}

SelectionContextCoordinator::~SelectionContextCoordinator()
{
    m_started = false;
    m_debounceTimer->stop();
    m_pauseTimer->stop();
    ++m_generation;
}

void SelectionContextCoordinator::start()
{
    if (m_started) {
        return;
    }
    const QPointer<SelectionContextCoordinator> guard(this);
    refreshSettings();
    if (!guard) {
        return;
    }
    m_started = true;
}

void SelectionContextCoordinator::stop()
{
    if (!m_started) {
        return;
    }
    m_started = false;
    m_debounceTimer->stop();
    m_pauseTimer->stop();
    ++m_generation;
    const bool hadCurrentSession = m_hasCurrentSession;
    const bool closeCurrentSession = !m_resultPinned;
    m_hasCurrentSession = false;
    m_resultPinned = false;
    m_lastSnapshot = SelectionSnapshot();
    QPointer<SelectionContextCoordinator> guard(this);
    if (m_access.cancelProbe) {
        const std::function<void()> cancelProbe = m_access.cancelProbe;
        cancelProbe();
        if (!guard) {
            return;
        }
    }
    if (closeCurrentSession) {
        if (m_access.hideToolbar) {
            const std::function<void()> hideToolbar = m_access.hideToolbar;
            hideToolbar();
            if (!guard) {
                return;
            }
        }
        if (hadCurrentSession && m_access.cancelActiveAction) {
            const std::function<void()> cancelAction =
                m_access.cancelActiveAction;
            cancelAction();
            if (!guard) {
                return;
            }
        }
        if (hadCurrentSession && m_access.closeUnpinnedResult) {
            const std::function<void()> closeResult =
                m_access.closeUnpinnedResult;
            closeResult();
        }
    }
}

void SelectionContextCoordinator::refreshSettings()
{
    if (m_access.settingsSnapshot) {
        const std::function<SelectionContextSettings()> snapshot =
            m_access.settingsSnapshot;
        const QPointer<SelectionContextCoordinator> guard(this);
        const SelectionContextSettings settings = snapshot();
        if (guard) {
            m_settings = settings;
        }
    }
}

void SelectionContextCoordinator::handleObservation(
    const SelectionObservation &observation)
{
    if (!m_started) {
        return;
    }
    const QPointer<SelectionContextCoordinator> guard(this);
    const bool owned = ownsSurface(observation.targetWindow);
    if (!guard || owned) {
        return;
    }
    switch (observation.reason) {
    case SelectionObservationReason::MouseSelection:
        if (m_settings.enabled && !m_paused) {
            scheduleAutomaticProbe(observation);
        }
        return;
    case SelectionObservationReason::KeyboardSelection:
        if (m_settings.enabled
            && m_settings.keyboardSelectionEnabled
            && !m_paused) {
            scheduleAutomaticProbe(observation);
        }
        return;
    case SelectionObservationReason::FallbackShortcut:
        beginProbe(observation, true);
        return;
    case SelectionObservationReason::OutsidePointerRelease:
    case SelectionObservationReason::EscapePressed:
    case SelectionObservationReason::ForegroundChanged:
        closeCurrentUnpinnedSession();
        return;
    case SelectionObservationReason::SystemUnavailable:
        closeCurrentUnpinnedSession();
        return;
    case SelectionObservationReason::SystemAvailable:
        return;
    }
}

void SelectionContextCoordinator::triggerFallbackShortcut()
{
    if (!m_started) {
        return;
    }
    SelectionObservation manual;
    manual.reason = SelectionObservationReason::FallbackShortcut;
    if (m_access.currentForegroundWindow) {
        const std::function<SelectedTextNativeWindowHandle()> foreground =
            m_access.currentForegroundWindow;
        const QPointer<SelectionContextCoordinator> guard(this);
        manual.targetWindow = foreground();
        if (!guard) {
            return;
        }
    }
    beginProbe(manual, true);
}

void SelectionContextCoordinator::pauseForMinutes(int minutes)
{
    if (minutes <= 0) {
        resume();
        return;
    }
    m_paused = true;
    m_debounceTimer->stop();
    if (!invalidateProbe()) {
        return;
    }
    const qint64 duration = qint64(minutes) * 60 * 1000;
    m_pauseTimer->start(int(qMin<qint64>(
        duration,
        std::numeric_limits<int>::max()
    )));
}

void SelectionContextCoordinator::resume()
{
    m_paused = false;
    m_pauseTimer->stop();
}

bool SelectionContextCoordinator::isPaused() const
{
    return m_paused;
}

void SelectionContextCoordinator::setResultPinned(bool pinned)
{
    m_resultPinned = pinned;
}

void SelectionContextCoordinator::scheduleAutomaticProbe(
    const SelectionObservation &observation)
{
    m_pendingObservation = observation;
    m_debounceTimer->start(160);
}

void SelectionContextCoordinator::beginProbe(
    const SelectionObservation &observation,
    bool manual)
{
    if (!m_started || (!manual && (!m_settings.enabled || m_paused))) {
        return;
    }
    QPointer<SelectionContextCoordinator> guard(this);
    const bool owned = ownsSurface(observation.targetWindow);
    if (!guard || owned) {
        return;
    }
    if (!invalidateProbe()) {
        return;
    }
    const quint64 generation = m_generation;
    if (manual) {
        m_manualGeneration = generation;
        m_manualFailureShownGeneration = 0;
    }

    SelectionProbeRequest request;
    request.targetWindow = observation.targetWindow;
    request.cursorPhysicalPosition = observation.cursorPhysicalPosition;
    if (!request.targetWindow && m_access.currentForegroundWindow) {
        request.targetWindow = m_access.currentForegroundWindow();
    }
    SelectionProbeRunnerCallbacks callbacks;
    const QPointer<SelectionContextCoordinator> callbackGuard(this);
    callbacks.completed = [callbackGuard](
        quint64 deliveredGeneration,
        const SelectionSnapshot &snapshot) {
        if (callbackGuard) {
            callbackGuard->acceptProbeResult(deliveredGeneration, snapshot);
        }
    };
    callbacks.timedOut = [callbackGuard](quint64 deliveredGeneration) {
        if (callbackGuard) {
            callbackGuard->handleProbeTimeout(deliveredGeneration);
        }
    };
    if (m_access.startProbe) {
        bool strong = false;
        if (m_access.strongSelectionEnabled) {
            const std::function<bool()> strongSelectionEnabled =
                m_access.strongSelectionEnabled;
            QPointer<SelectionContextCoordinator> strongGuard(this);
            strong = strongSelectionEnabled();
            if (!strongGuard) {
                return;
            }
        }
        const std::function<void(
            const SelectionProbeRequest &,
            bool,
            quint64,
            const SelectionProbeRunnerCallbacks &)> startProbe =
                m_access.startProbe;
        QPointer<SelectionContextCoordinator> self(this);
        startProbe(request, strong, generation, callbacks);
        if (!self) {
            return;
        }
    }
}

void SelectionContextCoordinator::acceptProbeResult(
    quint64 generation,
    const SelectionSnapshot &snapshot)
{
    if (!m_started || generation != m_generation) {
        return;
    }
    SelectionContextPolicyInput input;
    input.snapshot = snapshot;
    QPointer<SelectionContextCoordinator> guard(this);
    if (m_access.targetWindowValid) {
        const std::function<bool(SelectedTextNativeWindowHandle)> valid =
            m_access.targetWindowValid;
        input.targetWindowValid = valid(snapshot.targetWindow);
        if (!guard) {
            return;
        }
    }
    if (m_access.currentForegroundWindow) {
        const std::function<SelectedTextNativeWindowHandle()> foreground =
            m_access.currentForegroundWindow;
        input.targetWindowForeground =
            foreground() == snapshot.targetWindow;
        if (!guard) {
            return;
        }
    }
    input.minimumTextLength = m_settings.minimumTextLength;
    if (m_access.currentProcessId) {
        const std::function<quint32()> processId =
            m_access.currentProcessId;
        input.currentProcessId = processId();
        if (!guard) {
            return;
        }
    }
    input.blockedApplications = m_settings.blockedApplications;
    const SelectionContextEligibility eligibility =
        selectionContextEligibility(input);
    const bool manual = generation == m_manualGeneration;
    if (eligibility != SelectionContextEligibility::Eligible) {
        if (manual
            && eligibility == SelectionContextEligibility::PermissionDenied
            && m_manualFailureShownGeneration != generation) {
            m_manualFailureShownGeneration = generation;
            if (m_access.showManualFailure) {
                const std::function<void(SelectionContextEligibility)>
                    showFailure = m_access.showManualFailure;
                QPointer<SelectionContextCoordinator> guard(this);
                showFailure(eligibility);
                if (!guard) {
                    return;
                }
            }
        }
        if (eligibility == SelectionContextEligibility::InvalidTargetWindow
            || eligibility == SelectionContextEligibility::StaleForeground) {
            closeCurrentUnpinnedSession();
        }
        return;
    }

    if (m_hasCurrentSession
        && selectionSnapshotsEquivalent(m_lastSnapshot, snapshot)) {
        return;
    }
    if (m_hasCurrentSession && !m_resultPinned) {
        if (m_access.cancelActiveAction) {
            const std::function<void()> cancelAction =
                m_access.cancelActiveAction;
            cancelAction();
            if (!guard) {
                return;
            }
        }
        if (m_access.closeUnpinnedResult) {
            const std::function<void()> closeResult =
                m_access.closeUnpinnedResult;
            closeResult();
            if (!guard) {
                return;
            }
        }
    }
    m_resultPinned = false;
    m_hasCurrentSession = true;
    m_lastSnapshot = snapshot;
    if (m_access.logMetadata) {
        const std::function<void(const QString &, int)> log =
            m_access.logMetadata;
        log(
            QStringLiteral("selection.context.shown"),
            snapshot.text.size()
        );
        if (!guard) {
            return;
        }
    }
    if (m_access.showToolbar) {
        const std::function<void(const SelectionSnapshot &, bool)> show =
            m_access.showToolbar;
        QPointer<SelectionContextCoordinator> guard(this);
        show(snapshot, manual);
        if (!guard) {
            return;
        }
    }
}

void SelectionContextCoordinator::handleProbeTimeout(quint64 generation)
{
    if (!m_started || generation != m_generation) {
        return;
    }
    if (m_access.logMetadata) {
        m_access.logMetadata(QStringLiteral("selection.probe.timeout"), 0);
    }
}

bool SelectionContextCoordinator::invalidateProbe()
{
    ++m_generation;
    if (m_access.cancelProbe) {
        const std::function<void()> cancelProbe = m_access.cancelProbe;
        const QPointer<SelectionContextCoordinator> guard(this);
        cancelProbe();
        return bool(guard);
    }
    return true;
}

void SelectionContextCoordinator::closeCurrentUnpinnedSession()
{
    if (m_resultPinned) {
        return;
    }
    m_debounceTimer->stop();
    if (!invalidateProbe()) {
        return;
    }
    QPointer<SelectionContextCoordinator> guard(this);
    const bool hadCurrentSession = m_hasCurrentSession;
    m_hasCurrentSession = false;
    m_lastSnapshot = SelectionSnapshot();
    if (m_access.hideToolbar) {
        const std::function<void()> hideToolbar = m_access.hideToolbar;
        hideToolbar();
        if (!guard) {
            return;
        }
    }
    if (hadCurrentSession && m_access.cancelActiveAction) {
        const std::function<void()> cancelAction =
            m_access.cancelActiveAction;
        cancelAction();
        if (!guard) {
            return;
        }
    }
    if (hadCurrentSession && m_access.closeUnpinnedResult) {
        const std::function<void()> closeResult =
            m_access.closeUnpinnedResult;
        closeResult();
    }
}

bool SelectionContextCoordinator::ownsSurface(
    SelectedTextNativeWindowHandle window) const
{
    return window
        && m_access.ownsSurfaceWindow
        && m_access.ownsSurfaceWindow(window);
}
