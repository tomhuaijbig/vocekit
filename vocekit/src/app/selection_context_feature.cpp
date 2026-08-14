#include "selection_context_feature.h"

#include "../controllers/selection_context_action_controller.h"
#include "../controllers/selection_context_coordinator.h"
#include "../domain/selection_context_actions.h"
#include "../ui/selection_context_placement.h"
#include "../ui/selection_context_toolbar.h"
#include "../ui/selection_result_card.h"

#include <QApplication>
#include <QAbstractNativeEventFilter>
#include <QDesktopWidget>
#include <QMessageBox>
#include <QPointer>
#include <QSet>
#include <QToolButton>

#ifdef Q_OS_WIN
#include <windows.h>
#include <wtsapi32.h>
#endif

namespace {

QString text8(const char *value)
{
    return QString::fromUtf8(value);
}

QString normalizedExecutable(const QString &value)
{
    return value.trimmed().toLower();
}

class SelectionContextFeatureSession : public QObject
{
public:
    explicit SelectionContextFeatureSession(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    ~SelectionContextFeatureSession() override
    {
        if (card) {
            card->setCallbacks(SelectionResultCardCallbacks());
            card->hide();
            delete card.data();
        }
    }

    SelectionSnapshot snapshot;
    QPointer<SelectionResultCard> card;
    QPointer<SelectionContextModelRunner> runner;
    QPointer<SelectionContextActionController> action;
    bool pinned = false;
    bool closing = false;
};

class SelectionContextAvailabilityFilter : public QAbstractNativeEventFilter
{
public:
    std::function<void(bool)> availabilityChanged;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    bool nativeEventFilter(
        const QByteArray &,
        void *message,
        qintptr *) override
#else
    bool nativeEventFilter(
        const QByteArray &,
        void *message,
        long *) override
#endif
    {
#ifdef Q_OS_WIN
        const MSG *nativeMessage = static_cast<const MSG *>(message);
        if (!nativeMessage || !availabilityChanged) {
            return false;
        }
        if (nativeMessage->message == WM_WTSSESSION_CHANGE) {
            if (nativeMessage->wParam == WTS_SESSION_LOCK) {
                availabilityChanged(false);
            } else if (nativeMessage->wParam == WTS_SESSION_UNLOCK) {
                availabilityChanged(true);
            }
        } else if (nativeMessage->message == WM_POWERBROADCAST) {
            if (nativeMessage->wParam == PBT_APMSUSPEND) {
                availabilityChanged(false);
            } else if (nativeMessage->wParam == PBT_APMRESUMEAUTOMATIC) {
                availabilityChanged(true);
            }
        }
#else
        Q_UNUSED(message);
#endif
        return false;
    }
};

} // namespace

class SelectionContextFeature::Impl
{
public:
    Impl(
        SelectionContextFeature *owner,
        const SelectionContextFeatureAccess &requestedAccess,
        const SelectionContextFeatureDependencies &requestedDependencies)
        : q(owner),
          access(requestedAccess),
          dependencies(requestedDependencies),
          probeRunner(new SelectionProbeRunner(
              SelectionProbeRunnerAccess(),
              owner
          )),
          toolbar(new SelectionContextToolbar()),
          coordinator(nullptr)
    {
        toolbar->setProperty("selectionContextOwnedSurface", true);
        toolbar->setProperty(
            "selectionContextFeatureOwner",
            QVariant::fromValue<qulonglong>(quintptr(owner))
        );

        SelectionContextCoordinatorAccess coordinatorAccess;
        coordinatorAccess.settingsSnapshot = [this]() {
            return settings.selectionContext;
        };
        coordinatorAccess.strongSelectionEnabled = [this]() {
            return settings.strongSelectionEnabled;
        };
        coordinatorAccess.startProbe = [this](
            const SelectionProbeRequest &request,
            bool strong,
            quint64 generation,
            const SelectionProbeRunnerCallbacks &callbacks) {
            if (dependencies.startProbe) {
                dependencies.startProbe(
                    request,
                    strong,
                    generation,
                    callbacks
                );
            } else {
                probeRunner->start(request, strong, generation, callbacks);
            }
        };
        coordinatorAccess.cancelProbe = [this]() {
            cancelProbe();
        };
        coordinatorAccess.currentProcessId = [this]() {
            if (dependencies.currentProcessId) {
                return dependencies.currentProcessId();
            }
            return quint32(QCoreApplication::applicationPid());
        };
        coordinatorAccess.targetWindowValid = [this](
            SelectedTextNativeWindowHandle window) {
            if (dependencies.targetWindowValid) {
                return dependencies.targetWindowValid(window);
            }
            return ClipboardWriter::isUsableExternalWindow(window);
        };
        coordinatorAccess.currentForegroundWindow = [this]() {
            if (dependencies.currentForegroundWindow) {
                return dependencies.currentForegroundWindow();
            }
#ifdef Q_OS_WIN
            return reinterpret_cast<SelectedTextNativeWindowHandle>(
                GetForegroundWindow()
            );
#else
            return static_cast<SelectedTextNativeWindowHandle>(nullptr);
#endif
        };
        coordinatorAccess.showToolbar = [this](
            const SelectionSnapshot &snapshot,
            bool keyboardNavigationMode) {
            showToolbar(snapshot, keyboardNavigationMode);
        };
        coordinatorAccess.hideToolbar = [this]() {
            hideToolbar();
        };
        coordinatorAccess.closeUnpinnedResult = [this]() {
            closeCurrentSession();
        };
        coordinatorAccess.cancelActiveAction = [this]() {
            if (current && current->action) {
                current->action->cancel();
            }
        };
        coordinatorAccess.ownsSurfaceWindow = [this](
            SelectedTextNativeWindowHandle window) {
            return ownsSurfaceWindow(window);
        };
        coordinatorAccess.showManualFailure = [this](
            SelectionContextEligibility eligibility) {
            if (eligibility == SelectionContextEligibility::PermissionDenied) {
                QMessageBox::warning(
                    toolbar,
                    text8("无法读取选中文字"),
                    text8("当前应用不允许读取选区。请尝试以相同权限运行 vocekit。")
                );
            }
        };
        coordinatorAccess.logMetadata = [this](
            const QString &eventId,
            int textLength) {
            log(eventId, QString(), textLength, -1);
        };
        coordinator = new SelectionContextCoordinator(
            coordinatorAccess,
            owner
        );

        SelectionContextToolbarCallbacks toolbarCallbacks;
        toolbarCallbacks.actionRequested = [this](const QString &actionId) {
            routeToolbarAction(actionId);
        };
        toolbarCallbacks.closeRequested = [this]() {
            closeUnpinnedSurfaces();
        };
        toolbar->setCallbacks(toolbarCallbacks);

        QGuiApplication *guiApplication = qobject_cast<QGuiApplication *>(
            QCoreApplication::instance()
        );
        if (guiApplication) {
            QObject::connect(
                guiApplication,
                &QGuiApplication::applicationStateChanged,
                owner,
                [this](Qt::ApplicationState state) {
                    if (!started || state != Qt::ApplicationActive) {
                        return;
                    }
                    if (!systemAvailable) {
                        systemAvailable = true;
                        coordinator->start();
                        refresh();
                    }
                }
            );
        }
    }

    ~Impl()
    {
        stop();
        if (toolbar) {
            toolbar->setCallbacks(SelectionContextToolbarCallbacks());
            delete toolbar;
            toolbar = nullptr;
        }
    }

    void start()
    {
        if (started) {
            return;
        }
        started = true;
        systemAvailable = true;
        startAvailabilityMonitoring();
        loadSettings();
        coordinator->start();
        applyToolbarSettings();
        applyObserverSettings();
        if (settings.selectionContext.enabled) {
            ensureHooksInstalled();
        }
    }

    void stop()
    {
        if (!started) {
            return;
        }
        started = false;
        modalDepth = 0;
        coordinator->stop();
        cancelProbe();
        closeAllSessions();
        if (toolbar) {
            toolbar->hideToolbar();
        }
        updateSurfaceVisibility();
        uninstallHooks();
        stopAvailabilityMonitoring();
    }

    void refresh()
    {
        loadSettings();
        coordinator->refreshSettings();
        applyToolbarSettings();
        updatePinAvailability();
        applyObserverSettings();
        if (!started || !systemAvailable) {
            return;
        }
        if (settings.selectionContext.enabled || hasVisibleSurface()) {
            ensureHooksInstalled();
        } else {
            uninstallHooks();
        }
    }

    void triggerFallbackShortcut()
    {
        if (!started || !systemAvailable) {
            return;
        }
        coordinator->triggerFallbackShortcut();
    }

    void handleObservation(const SelectionObservation &observation)
    {
        if (!started) {
            return;
        }
        if (observation.reason == SelectionObservationReason::SystemUnavailable) {
            handleSystemAvailability(false);
            return;
        }
        if (observation.reason == SelectionObservationReason::SystemAvailable) {
            handleSystemAvailability(true);
            return;
        }
        if (modalDepth > 0) {
            return;
        }
        if (observation.reason
                == SelectionObservationReason::OutsidePointerRelease
            && !settings.selectionContext.closeOnOutsideClick) {
            return;
        }
        coordinator->handleObservation(observation);
    }

    void pauseForMinutes(int minutes)
    {
        coordinator->pauseForMinutes(minutes);
        applyObserverSettings();
    }

    void resume()
    {
        coordinator->resume();
        applyObserverSettings();
    }

    bool isPaused() const
    {
        return coordinator && coordinator->isPaused();
    }

    void handleSystemAvailability(bool available)
    {
        if (!started || available == systemAvailable) {
            return;
        }
        systemAvailable = available;
        if (!available) {
            coordinator->stop();
            cancelProbe();
            closeAllSessions();
            toolbar->hideToolbar();
            updateSurfaceVisibility();
            uninstallHooks();
            return;
        }
        coordinator->start();
        refresh();
    }

    void startAvailabilityMonitoring()
    {
#ifdef Q_OS_WIN
        if (availabilityFilter || !QCoreApplication::instance()) {
            return;
        }
        availabilityWindow = new QWidget();
        availabilityWindow->setObjectName(
            QStringLiteral("selectionContextAvailabilityWindow")
        );
        availabilityWindow->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
        availabilityWindow->resize(1, 1);
        const HWND nativeWindow = reinterpret_cast<HWND>(
            quintptr(availabilityWindow->winId())
        );
        availabilityRegistered = nativeWindow
            && WTSRegisterSessionNotification(
                nativeWindow,
                NOTIFY_FOR_THIS_SESSION
            );
        availabilityFilter = new SelectionContextAvailabilityFilter;
        availabilityFilter->availabilityChanged = [this](bool available) {
            handleSystemAvailability(available);
        };
        QCoreApplication::instance()->installNativeEventFilter(
            availabilityFilter
        );
#endif
    }

    void stopAvailabilityMonitoring()
    {
#ifdef Q_OS_WIN
        if (availabilityFilter) {
            availabilityFilter->availabilityChanged =
                std::function<void(bool)>();
            if (QCoreApplication::instance()) {
                QCoreApplication::instance()->removeNativeEventFilter(
                    availabilityFilter
                );
            }
            delete availabilityFilter;
            availabilityFilter = nullptr;
        }
        if (availabilityWindow) {
            if (availabilityRegistered) {
                WTSUnRegisterSessionNotification(
                    reinterpret_cast<HWND>(
                        quintptr(availabilityWindow->winId())
                    )
                );
            }
            availabilityRegistered = false;
            delete availabilityWindow;
            availabilityWindow = nullptr;
        }
#endif
    }

    void loadSettings()
    {
        settings = access.settingsSnapshot
            ? access.settingsSnapshot()
            : AppSettingsData();
        settings.selectionContext.minimumTextLength = qMax(
            1,
            settings.selectionContext.minimumTextLength
        );
        settings.selectionContext.pauseMinutes = qMax(
            1,
            settings.selectionContext.pauseMinutes
        );
        settings.selectionContext.actionOrder =
            normalizeSelectionContextActionOrder(
                settings.selectionContext.actionOrder
            );
        QStringList blocked;
        for (const QString &entry :
             settings.selectionContext.blockedApplications) {
            const QString executable = normalizedExecutable(entry);
            if (!executable.isEmpty() && !blocked.contains(executable)) {
                blocked.append(executable);
            }
        }
        settings.selectionContext.blockedApplications = blocked;
    }

    void applyToolbarSettings()
    {
        toolbar->setActionOrder(settings.selectionContext.actionOrder);
        QVector<SelectionContextMenuItem> items;
        QSet<QString> added;
        for (const FunctionSettings &function : settings.functions) {
            if (function.builtIn
                || function.executionMode != FunctionExecutionMode::Classic) {
                continue;
            }
            const QString actionId = selectionContextActionForFunction(
                function.id
            );
            if (actionId.isEmpty() || added.contains(actionId)) {
                continue;
            }
            added.insert(actionId);
            SelectionContextMenuItem item;
            item.actionId = actionId;
            item.title = function.name.trimmed().isEmpty()
                ? text8("自定义功能")
                : function.name.trimmed();
            items.append(item);
        }
        toolbar->setMoreActions(items);
    }

    void applyObserverSettings()
    {
        const bool paused = modalDepth > 0 || coordinator->isPaused();
        observer.setAutomaticEnabled(settings.selectionContext.enabled);
        observer.setKeyboardSelectionEnabled(
            settings.selectionContext.keyboardSelectionEnabled
        );
        observer.setPaused(paused);
        if (dependencies.setObserverAutomaticEnabled) {
            dependencies.setObserverAutomaticEnabled(
                settings.selectionContext.enabled
            );
        }
        if (dependencies.setObserverKeyboardEnabled) {
            dependencies.setObserverKeyboardEnabled(
                settings.selectionContext.keyboardSelectionEnabled
            );
        }
        if (dependencies.setObserverPaused) {
            dependencies.setObserverPaused(paused);
        }
    }

    bool ensureHooksInstalled()
    {
        if (hooks || !started || !systemAvailable) {
            return hooks;
        }
        const std::function<void(const SelectionObservation &)> callback =
            [this](const SelectionObservation &observation) {
                handleObservation(observation);
            };
        observer.setCallback(callback);
        if (dependencies.setObservationCallback) {
            dependencies.setObservationCallback(callback);
        }
        QString error;
        bool installed = false;
        if (dependencies.installObserver) {
            installed = dependencies.installObserver(
                reinterpret_cast<SelectedTextNativeWindowHandle>(
                    quintptr(toolbar->winId())
                ),
                &error
            );
        } else {
            installed = observer.install(
                reinterpret_cast<SelectedTextNativeWindowHandle>(
                    quintptr(toolbar->winId())
                ),
                &error
            );
        }
        hooks = installed;
        if (!installed) {
            log(
                QStringLiteral("selection.observer.install_failed"),
                error,
                0,
                -1
            );
        }
        return hooks;
    }

    void uninstallHooks()
    {
        if (!hooks) {
            return;
        }
        if (dependencies.uninstallObserver) {
            dependencies.uninstallObserver();
        } else {
            observer.uninstall();
        }
        hooks = false;
    }

    void cancelProbe()
    {
        if (dependencies.cancelProbe) {
            dependencies.cancelProbe();
        } else if (probeRunner) {
            probeRunner->cancel();
        }
    }

    QRect availableGeometry(const SelectionSnapshot &snapshot) const
    {
        const QPoint point = snapshot.anchorRect.isValid()
            ? snapshot.anchorRect.center()
            : snapshot.cursorPosition;
        if (dependencies.availableGeometry) {
            return dependencies.availableGeometry(point);
        }
        return QApplication::desktop()->availableGeometry(point);
    }

    void showToolbar(
        const SelectionSnapshot &snapshot,
        bool keyboardNavigationMode)
    {
        lastSnapshot = snapshot;
        applyToolbarSettings();
        toolbar->showForSnapshot(
            snapshot,
            availableGeometry(snapshot),
            keyboardNavigationMode
        );
        updateSurfaceVisibility();
        if (!settings.selectionContext.enabled) {
            ensureHooksInstalled();
        }
    }

    void hideToolbar()
    {
        toolbar->hideToolbar();
        updateSurfaceVisibility();
        uninstallIdleManualHooks();
    }

    bool hasVisibleSurface() const
    {
        if (toolbar && toolbar->isVisible()) {
            return true;
        }
        if (current && current->card && current->card->isVisible()) {
            return true;
        }
        for (const QPointer<SelectionContextFeatureSession> &session : pinned) {
            if (session && session->card && session->card->isVisible()) {
                return true;
            }
        }
        return false;
    }

    void updateSurfaceVisibility()
    {
        const bool visible = hasVisibleSurface();
        observer.setSurfaceVisible(visible);
        if (dependencies.setObserverSurfaceVisible) {
            dependencies.setObserverSurfaceVisible(visible);
        }
    }

    void uninstallIdleManualHooks()
    {
        if (!settings.selectionContext.enabled && !hasVisibleSurface()) {
            uninstallHooks();
        }
    }

    bool ownsSurfaceWindow(SelectedTextNativeWindowHandle window) const
    {
        if (toolbar && toolbar->ownsNativeWindow(window)) {
            return true;
        }
        if (current && current->card
            && current->card->ownsNativeWindow(window)) {
            return true;
        }
        for (const QPointer<SelectionContextFeatureSession> &session : pinned) {
            if (session && session->card
                && session->card->ownsNativeWindow(window)) {
                return true;
            }
        }
        return false;
    }

    SelectionContextFeatureSession *ensureCurrentSession()
    {
        if (current) {
            return current.data();
        }
        if (!lastSnapshot.isUsable()) {
            return nullptr;
        }
        SelectionContextFeatureSession *session =
            new SelectionContextFeatureSession(q);
        session->snapshot = lastSnapshot;
        session->card = new SelectionResultCard();
        session->card->setProperty(
            "selectionContextFeatureOwner",
            QVariant::fromValue<qulonglong>(quintptr(q))
        );
        session->runner = new SelectionContextModelRunner(
            dependencies.modelRunnerAccess,
            session
        );

        SelectionContextActionAccess actionAccess;
        actionAccess.copyText = [this](const QString &text) {
            return dependencies.copyText
                ? dependencies.copyText(text)
                : ClipboardWriter::copyText(text);
        };
        actionAccess.saveVocabulary = [this](const QString &text) {
            runSuppressed([this, text]() {
                if (access.saveVocabulary) {
                    access.saveVocabulary(text);
                }
            });
        };
        actionAccess.validateSelectionAsync = [this](
            SelectedTextNativeWindowHandle window,
            quint64 generation,
            const std::function<void(quint64, bool)> &completed) {
            if (dependencies.validateSelectionAsync) {
                dependencies.validateSelectionAsync(
                    window,
                    generation,
                    completed
                );
            } else {
                probeRunner->validateSelectionAsync(
                    window,
                    generation,
                    completed
                );
            }
        };
        actionAccess.replaceSelection = [this](
            const QString &text,
            SelectedTextNativeWindowHandle window) {
            return dependencies.replaceSelection
                ? dependencies.replaceSelection(text, window)
                : ClipboardWriter::pasteTextToWindowChecked(
                    text,
                    window,
                    true,
                    true
                );
        };
        actionAccess.settingsSnapshot = [this]() {
            return access.settingsSnapshot
                ? access.settingsSnapshot()
                : settings;
        };
        actionAccess.promptSnapshot = [this]() {
            return access.promptSnapshot
                ? access.promptSnapshot()
                : PromptRuntimeSnapshot();
        };
        actionAccess.ensureNetworkConsent = [this](
            const QString &actionId,
            const QString &modelId) {
            bool accepted = false;
            const QPointer<SelectionContextFeature> guard(q);
            runSuppressed([this, actionId, modelId, &accepted]() {
                accepted = access.ensureNetworkConsent
                    && access.ensureNetworkConsent(actionId, modelId);
            });
            if (guard && accepted) {
                refresh();
            }
            return accepted;
        };
        const QPointer<SelectionContextFeatureSession> sessionGuard(session);
        actionAccess.renderResult = [this, sessionGuard](
            const SelectionResultCardState &state) {
            if (sessionGuard) {
                renderSession(sessionGuard.data(), state);
            }
        };
        actionAccess.closeToolbar = [this]() {
            hideToolbar();
        };
        actionAccess.logMetadata = [this](
            const QString &eventId,
            const QString &actionId,
            int textLength,
            qint64 elapsedMs) {
            log(eventId, actionId, textLength, elapsedMs);
        };
        session->action = new SelectionContextActionController(
            session->runner,
            actionAccess,
            session
        );
        session->action->setSelection(session->snapshot);

        SelectionResultCardCallbacks cardCallbacks;
        cardCallbacks.cancelRequested = [sessionGuard]() {
            if (sessionGuard && sessionGuard->action) {
                sessionGuard->action->cancel();
            }
        };
        cardCallbacks.copyRequested = [sessionGuard]() {
            if (sessionGuard && sessionGuard->action) {
                sessionGuard->action->copyResult();
            }
        };
        cardCallbacks.replaceRequested = [sessionGuard]() {
            if (sessionGuard && sessionGuard->action) {
                sessionGuard->action->replaceResult();
            }
        };
        cardCallbacks.regenerateRequested = [sessionGuard]() {
            if (sessionGuard && sessionGuard->action) {
                sessionGuard->action->regenerate();
            }
        };
        cardCallbacks.pinChanged = [this, sessionGuard](bool pinnedValue) {
            if (sessionGuard) {
                setSessionPinned(sessionGuard.data(), pinnedValue);
            }
        };
        cardCallbacks.closeRequested = [this, sessionGuard]() {
            if (sessionGuard) {
                requestCloseSession(sessionGuard.data());
            }
        };
        cardCallbacks.followUpRequested = [sessionGuard](
            const QString &question) {
            if (sessionGuard && sessionGuard->action) {
                sessionGuard->action->submitFollowUp(question);
            }
        };
        cardCallbacks.processFullTextRequested = [sessionGuard]() {
            if (sessionGuard && sessionGuard->action) {
                sessionGuard->action->processFullTextConfirmed();
            }
        };
        session->card->setCallbacks(cardCallbacks);
        current = session;
        updatePinAvailability();
        return session;
    }

    void renderSession(
        SelectionContextFeatureSession *session,
        const SelectionResultCardState &state)
    {
        if (!session || session->closing || !session->card) {
            return;
        }
        session->card->setState(state);
        updatePinAvailability();
        const QRect screen = availableGeometry(session->snapshot);
        const SelectionSurfacePlacement placement = placeSelectionSurfaces(
            session->snapshot.anchorRect,
            session->snapshot.cursorPosition,
            toolbar->size(),
            session->card->size(),
            screen,
            8
        );
        session->card->showAt(placement.cardTopLeft, screen);
        updateSurfaceVisibility();
        if (!settings.selectionContext.enabled) {
            ensureHooksInstalled();
        }
    }

    void updatePinAvailability()
    {
        const int pinnedCount = livePinnedCount();
        const QList<QPointer<SelectionContextFeatureSession>> sessions =
            allSessions();
        for (const QPointer<SelectionContextFeatureSession> &session : sessions) {
            if (!session || !session->card) {
                continue;
            }
            QToolButton *pin = session->card->findChild<QToolButton *>(
                QStringLiteral("selectionResultPinButton")
            );
            if (pin) {
                pin->setEnabled(
                    settings.selectionContext.pinEnabled
                    && (session->pinned || pinnedCount < 3)
                );
            }
        }
    }

    QList<QPointer<SelectionContextFeatureSession>> allSessions() const
    {
        QList<QPointer<SelectionContextFeatureSession>> result = pinned;
        if (current && !result.contains(current)) {
            result.append(current);
        }
        return result;
    }

    int livePinnedCount() const
    {
        int count = 0;
        for (const QPointer<SelectionContextFeatureSession> &session : pinned) {
            if (session && session->pinned && !session->closing) {
                ++count;
            }
        }
        return count;
    }

    void setSessionPinned(
        SelectionContextFeatureSession *session,
        bool pinnedValue)
    {
        if (!session || session->closing || !session->action) {
            return;
        }
        if (pinnedValue) {
            if (!settings.selectionContext.pinEnabled
                || livePinnedCount() >= 3) {
                session->action->setPinned(false);
                updatePinAvailability();
                return;
            }
            session->pinned = true;
            session->action->setPinned(true);
            if (!pinned.contains(session)) {
                pinned.append(session);
            }
            if (current == session) {
                current.clear();
            }
            coordinator->setResultPinned(true);
        } else {
            session->pinned = false;
            session->action->setPinned(false);
            pinned.removeAll(session);
            if (!current) {
                current = session;
                coordinator->setResultPinned(false);
            } else if (current != session) {
                removeSession(session);
                return;
            }
        }
        updatePinAvailability();
        updateSurfaceVisibility();
    }

    void routeToolbarAction(const QString &actionId)
    {
        if (actionId == selectionContextMenuBlockApplication()) {
            blockCurrentApplication();
            return;
        }
        if (actionId == selectionContextMenuOpenSettings()) {
            const QPointer<SelectionContextFeature> guard(q);
            runSuppressed([this]() {
                if (access.openSettings) {
                    access.openSettings();
                }
            });
            if (guard) {
                closeUnpinnedSurfaces();
            }
            return;
        }
        SelectionContextFeatureSession *session = ensureCurrentSession();
        if (!session || !session->action) {
            return;
        }
        session->action->triggerAction(actionId);
        if (actionId == selectionContextActionCopy()
            || actionId == selectionContextActionSave()) {
            closeUnpinnedSurfaces();
        }
    }

    void blockCurrentApplication()
    {
        const QString executable = normalizedExecutable(
            lastSnapshot.targetExecutable
        );
        if (executable.isEmpty() || !access.blockApplication) {
            return;
        }
        bool saved = false;
        const QPointer<SelectionContextFeature> guard(q);
        runSuppressed([this, executable, &saved]() {
            saved = access.blockApplication(executable);
        });
        if (!guard || !saved) {
            return;
        }
        refresh();
        closeUnpinnedSurfaces();
    }

    void closeUnpinnedSurfaces()
    {
        SelectionObservation observation;
        observation.reason = SelectionObservationReason::EscapePressed;
        coordinator->handleObservation(observation);
        if (toolbar->isVisible()) {
            hideToolbar();
        }
        updateSurfaceVisibility();
        uninstallIdleManualHooks();
    }

    void closeCurrentSession()
    {
        if (current && !current->pinned) {
            removeSession(current.data());
        }
    }

    void requestCloseSession(SelectionContextFeatureSession *session)
    {
        if (!session || session->closing) {
            return;
        }
        if (current == session && !session->pinned) {
            closeUnpinnedSurfaces();
            return;
        }
        const bool activePinnedSession = session->pinned
            && !current
            && selectionSnapshotsEquivalent(
                session->snapshot,
                lastSnapshot
            );
        if (activePinnedSession) {
            pinned.removeAll(session);
            session->pinned = false;
            current = session;
            if (session->action) {
                session->action->setPinned(false);
            }
            coordinator->setResultPinned(false);
            closeUnpinnedSurfaces();
            return;
        }
        removeSession(session);
    }

    void removeSession(SelectionContextFeatureSession *session)
    {
        if (!session || session->closing) {
            return;
        }
        session->closing = true;
        if (session->action) {
            session->action->cancel();
        }
        if (session->runner) {
            session->runner->cancel();
        }
        if (session->card) {
            session->card->setCallbacks(SelectionResultCardCallbacks());
            session->card->hide();
        }
        if (current == session) {
            current.clear();
        }
        pinned.removeAll(session);
        session->deleteLater();
        updatePinAvailability();
        updateSurfaceVisibility();
        uninstallIdleManualHooks();
    }

    void closeAllSessions()
    {
        const QList<QPointer<SelectionContextFeatureSession>> sessions =
            allSessions();
        current.clear();
        pinned.clear();
        for (const QPointer<SelectionContextFeatureSession> &session : sessions) {
            if (!session || session->closing) {
                continue;
            }
            session->closing = true;
            if (session->action) {
                session->action->cancel();
            }
            if (session->runner) {
                session->runner->cancel();
            }
            if (session->card) {
                session->card->setCallbacks(SelectionResultCardCallbacks());
                session->card->hide();
            }
            delete session.data();
        }
    }

    void beginSuppression()
    {
        if (++modalDepth != 1) {
            return;
        }
        pausedBeforeModal = coordinator->isPaused();
        if (!pausedBeforeModal) {
            coordinator->pauseForMinutes(1);
        } else {
            cancelProbe();
        }
        applyObserverSettings();
    }

    void endSuppression()
    {
        if (modalDepth <= 0 || --modalDepth != 0) {
            return;
        }
        if (!pausedBeforeModal) {
            coordinator->resume();
        }
        applyObserverSettings();
    }

    void runSuppressed(const std::function<void()> &callback)
    {
        beginSuppression();
        const QPointer<SelectionContextFeature> guard(q);
        if (callback) {
            callback();
        }
        if (guard) {
            endSuppression();
        }
    }

    void log(
        const QString &eventId,
        const QString &actionId,
        int textLength,
        qint64 elapsedMs)
    {
        if (access.logMetadata) {
            access.logMetadata(
                eventId,
                actionId,
                textLength,
                elapsedMs
            );
        }
    }

    SelectionContextFeature *q = nullptr;
    SelectionContextFeatureAccess access;
    SelectionContextFeatureDependencies dependencies;
    AppSettingsData settings;
    SelectionSnapshot lastSnapshot;
    SelectionObserver observer;
    QPointer<SelectionProbeRunner> probeRunner;
    SelectionContextToolbar *toolbar = nullptr;
    QPointer<SelectionContextCoordinator> coordinator;
    QPointer<SelectionContextFeatureSession> current;
    QList<QPointer<SelectionContextFeatureSession>> pinned;
    bool started = false;
    bool hooks = false;
    bool systemAvailable = true;
    int modalDepth = 0;
    bool pausedBeforeModal = false;
    QWidget *availabilityWindow = nullptr;
    SelectionContextAvailabilityFilter *availabilityFilter = nullptr;
    bool availabilityRegistered = false;
};

SelectionContextFeature::SelectionContextFeature(
    const SelectionContextFeatureAccess &access,
    QObject *parent)
    : SelectionContextFeature(
        access,
        SelectionContextFeatureDependencies(),
        parent
    )
{
}

SelectionContextFeature::SelectionContextFeature(
    const SelectionContextFeatureAccess &access,
    const SelectionContextFeatureDependencies &dependencies,
    QObject *parent)
    : QObject(parent),
      m_impl(new Impl(this, access, dependencies))
{
}

SelectionContextFeature::~SelectionContextFeature()
{
    delete m_impl;
    m_impl = nullptr;
}

void SelectionContextFeature::start()
{
    m_impl->start();
}

void SelectionContextFeature::stop()
{
    m_impl->stop();
}

void SelectionContextFeature::refresh()
{
    m_impl->refresh();
}

void SelectionContextFeature::triggerFallbackShortcut()
{
    m_impl->triggerFallbackShortcut();
}

void SelectionContextFeature::pauseForMinutes(int minutes)
{
    m_impl->pauseForMinutes(minutes);
}

void SelectionContextFeature::resume()
{
    m_impl->resume();
}

bool SelectionContextFeature::isPaused() const
{
    return m_impl->isPaused();
}

bool SelectionContextFeature::isEnabled() const
{
    return m_impl->settings.selectionContext.enabled;
}

bool SelectionContextFeature::hooksInstalled() const
{
    return m_impl->hooks;
}

int SelectionContextFeature::pinnedResultCount() const
{
    return m_impl->livePinnedCount();
}
