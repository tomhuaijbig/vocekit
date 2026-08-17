#include "selection_observer.h"

#include <QAbstractNativeEventFilter>
#include <QCoreApplication>
#include <QSharedPointer>
#include <QTimer>
#include <QWeakPointer>

#ifdef Q_OS_WIN
#include <windows.h>
#include <wtsapi32.h>
#endif

namespace {

const unsigned int kKeyShift = 0x10;
const unsigned int kKeyLeftShift = 0xa0;
const unsigned int kKeyRightShift = 0xa1;
const unsigned int kKeyControl = 0x11;
const unsigned int kKeyLeftControl = 0xa2;
const unsigned int kKeyRightControl = 0xa3;
const unsigned int kKeyEscape = 0x1b;
const unsigned int kKeyLeft = 0x25;
const unsigned int kKeyUp = 0x26;
const unsigned int kKeyRight = 0x27;
const unsigned int kKeyDown = 0x28;
const unsigned int kKeyPrior = 0x21;
const unsigned int kKeyNext = 0x22;
const unsigned int kKeyEnd = 0x23;
const unsigned int kKeyHome = 0x24;
const unsigned int kKeyA = 'A';
const unsigned int kMessageKeyDown = 0x0100;
const unsigned int kMessageKeyUp = 0x0101;
const unsigned int kMessageSystemKeyDown = 0x0104;
const unsigned int kMessageSystemKeyUp = 0x0105;
const unsigned int kMessageLeftButtonDown = 0x0201;
const unsigned int kMessageLeftButtonUp = 0x0202;
const int kCandidateDelayMs = 170;

bool isShiftKey(unsigned int key)
{
    return key == kKeyShift
        || key == kKeyLeftShift
        || key == kKeyRightShift;
}

bool isControlKey(unsigned int key)
{
    return key == kKeyControl
        || key == kKeyLeftControl
        || key == kKeyRightControl;
}

bool isSelectionKey(unsigned int key)
{
    return key == kKeyLeft
        || key == kKeyRight
        || key == kKeyUp
        || key == kKeyDown
        || key == kKeyHome
        || key == kKeyEnd
        || key == kKeyPrior
        || key == kKeyNext;
}

bool isRelevantKey(unsigned int key)
{
    return isShiftKey(key)
        || isControlKey(key)
        || isSelectionKey(key)
        || key == kKeyA
        || key == kKeyEscape;
}

struct SelectionObserverDispatchState
{
    std::function<void(const SelectionObservation &)> callback;
    quint64 epoch = 1;
    bool installed = false;
};

#ifdef Q_OS_WIN
SelectedTextNativeWindowHandle normalizedRootWindow(
    SelectedTextNativeWindowHandle value)
{
    HWND window = static_cast<HWND>(value);
    if (window && IsWindow(window)) {
        const HWND root = GetAncestor(window, GA_ROOT);
        if (root) {
            window = root;
        }
    }
    return reinterpret_cast<SelectedTextNativeWindowHandle>(window);
}

SelectedTextNativeWindowHandle rootWindowAt(const QPoint &point)
{
    POINT nativePoint;
    nativePoint.x = point.x();
    nativePoint.y = point.y();
    return normalizedRootWindow(
        reinterpret_cast<SelectedTextNativeWindowHandle>(
            WindowFromPoint(nativePoint)
        )
    );
}

QPoint currentPhysicalCursorPosition()
{
    POINT point;
    if (GetCursorPos(&point)) {
        return QPoint(point.x, point.y);
    }
    return QPoint();
}
#else
SelectedTextNativeWindowHandle rootWindowAt(const QPoint &)
{
    return nullptr;
}

SelectedTextNativeWindowHandle normalizedRootWindow(
    SelectedTextNativeWindowHandle value)
{
    return value;
}

QPoint currentPhysicalCursorPosition()
{
    return QPoint();
}
#endif

} // namespace

void SelectionObservationMatcher::setAutomaticEnabled(bool enabled)
{
    m_automaticEnabled = enabled;
    if (!enabled) {
        m_mousePressed = false;
        m_shiftPressed = false;
        m_controlPressed = false;
        m_implicitShiftReleaseEmitted = false;
        m_ctrlAArmed = false;
        m_armedSelectionKey = 0;
    }
}

void SelectionObservationMatcher::setKeyboardSelectionEnabled(bool enabled)
{
    m_keyboardSelectionEnabled = enabled;
    if (!enabled) {
        m_shiftPressed = false;
        m_controlPressed = false;
        m_implicitShiftReleaseEmitted = false;
        m_ctrlAArmed = false;
        m_armedSelectionKey = 0;
    }
}

void SelectionObservationMatcher::setPaused(bool paused)
{
    m_paused = paused;
    if (paused) {
        m_mousePressed = false;
        m_shiftPressed = false;
        m_controlPressed = false;
        m_implicitShiftReleaseEmitted = false;
        m_ctrlAArmed = false;
        m_armedSelectionKey = 0;
    }
}

void SelectionObservationMatcher::setSurfaceVisible(bool visible)
{
    m_surfaceVisible = visible;
}

void SelectionObservationMatcher::mousePressed(const QPoint &point)
{
    m_mousePressed = true;
    m_mousePressPoint = point;
}

bool SelectionObservationMatcher::mouseReleased(const QPoint &point)
{
    Q_UNUSED(point);
    m_mousePressed = false;
    return m_automaticEnabled && !m_paused;
}

void SelectionObservationMatcher::keyPressed(unsigned int nativeKey)
{
    if (m_paused
        || !m_automaticEnabled
        || !m_keyboardSelectionEnabled) {
        return;
    }
    if (isShiftKey(nativeKey)) {
        m_shiftPressed = true;
        m_implicitShiftReleaseEmitted = false;
        m_armedSelectionKey = 0;
        return;
    }
    if (isControlKey(nativeKey)) {
        m_controlPressed = true;
        m_ctrlAArmed = false;
        return;
    }
    if (m_shiftPressed && isSelectionKey(nativeKey)) {
        m_armedSelectionKey = nativeKey;
        return;
    }
    if (m_controlPressed && nativeKey == kKeyA) {
        m_ctrlAArmed = true;
    }
}

bool SelectionObservationMatcher::keyReleased(unsigned int nativeKey)
{
    if (isShiftKey(nativeKey)) {
        m_shiftPressed = false;
        m_armedSelectionKey = 0;
        m_implicitShiftReleaseEmitted = false;
        return false;
    }
    if (isControlKey(nativeKey)) {
        m_controlPressed = false;
        m_ctrlAArmed = false;
        return false;
    }
    if (m_paused
        || !m_automaticEnabled
        || !m_keyboardSelectionEnabled) {
        return false;
    }
    if (nativeKey == kKeyA && m_controlPressed && m_ctrlAArmed) {
        m_ctrlAArmed = false;
        return true;
    }
    if (isSelectionKey(nativeKey) && m_shiftPressed) {
        const bool armed = m_armedSelectionKey == nativeKey;
        const bool implicit = !m_implicitShiftReleaseEmitted;
        m_armedSelectionKey = 0;
        if (armed || implicit) {
            m_implicitShiftReleaseEmitted = true;
            return true;
        }
    }
    return false;
}

bool SelectionObservationMatcher::fallbackShortcutReleased() const
{
    return true;
}

bool SelectionObservationMatcher::escapeReleased()
{
    if (!m_surfaceVisible) {
        return false;
    }
    m_surfaceVisible = false;
    return true;
}

void SelectionObservationMatcher::reset()
{
    m_surfaceVisible = false;
    m_mousePressed = false;
    m_shiftPressed = false;
    m_controlPressed = false;
    m_implicitShiftReleaseEmitted = false;
    m_ctrlAArmed = false;
    m_armedSelectionKey = 0;
    m_mousePressPoint = QPoint();
}

bool SelectionObservationMatcher::isPaused() const
{
    return m_paused;
}

namespace {

SelectionObserver *g_selectionObserver = nullptr;

#ifdef Q_OS_WIN
LRESULT CALLBACK selectionMouseHook(
    int code,
    WPARAM message,
    LPARAM data)
{
    if (code >= 0 && g_selectionObserver
        && (message == WM_LBUTTONDOWN || message == WM_LBUTTONUP)) {
        const MSLLHOOKSTRUCT *event =
            reinterpret_cast<const MSLLHOOKSTRUCT *>(data);
        if (event) {
            g_selectionObserver->processNativeMouse(
                unsigned(message),
                QPoint(event->pt.x, event->pt.y)
            );
        }
    }
    return CallNextHookEx(nullptr, code, message, data);
}

LRESULT CALLBACK selectionKeyboardHook(
    int code,
    WPARAM message,
    LPARAM data)
{
    if (code >= 0 && g_selectionObserver
        && (message == WM_KEYDOWN
            || message == WM_KEYUP
            || message == WM_SYSKEYDOWN
            || message == WM_SYSKEYUP)) {
        const KBDLLHOOKSTRUCT *event =
            reinterpret_cast<const KBDLLHOOKSTRUCT *>(data);
        if (event && isRelevantKey(event->vkCode)) {
            g_selectionObserver->processNativeKey(
                unsigned(message),
                unsigned(event->vkCode)
            );
        }
    }
    return CallNextHookEx(nullptr, code, message, data);
}

void CALLBACK selectionForegroundHook(
    HWINEVENTHOOK,
    DWORD event,
    HWND window,
    LONG,
    LONG,
    DWORD,
    DWORD)
{
    if (event == EVENT_SYSTEM_FOREGROUND && g_selectionObserver) {
        g_selectionObserver->processForegroundWindowChanged(
            reinterpret_cast<SelectedTextNativeWindowHandle>(window)
        );
    }
}
#endif

} // namespace

class SelectionObserverNativeEventFilter : public QAbstractNativeEventFilter
{
public:
    explicit SelectionObserverNativeEventFilter(SelectionObserver *observer)
        : m_observer(observer)
    {
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    bool nativeEventFilter(
        const QByteArray &eventType,
        void *message,
        qintptr *result) override
#else
    bool nativeEventFilter(
        const QByteArray &eventType,
        void *message,
        long *result) override
#endif
    {
        Q_UNUSED(eventType);
        Q_UNUSED(result);
#ifdef Q_OS_WIN
        const MSG *nativeMessage = static_cast<const MSG *>(message);
        if (!nativeMessage || !m_observer) {
            return false;
        }
        if (nativeMessage->message == WM_WTSSESSION_CHANGE) {
            if (nativeMessage->wParam == WTS_SESSION_LOCK) {
                m_observer->processSystemAvailabilityChanged(false);
            } else if (nativeMessage->wParam == WTS_SESSION_UNLOCK) {
                m_observer->processSystemAvailabilityChanged(true);
            }
        } else if (nativeMessage->message == WM_POWERBROADCAST) {
            if (nativeMessage->wParam == PBT_APMSUSPEND) {
                m_observer->processSystemAvailabilityChanged(false);
            } else if (nativeMessage->wParam == PBT_APMRESUMEAUTOMATIC) {
                m_observer->processSystemAvailabilityChanged(true);
            }
        }
#else
        Q_UNUSED(message);
#endif
        return false;
    }

private:
    SelectionObserver *m_observer = nullptr;
};

class SelectionObserver::Impl
{
public:
    explicit Impl(SelectionObserver *owner)
        : q(owner),
          dispatch(new SelectionObserverDispatchState)
    {
    }

    void enqueue(const SelectionObservation &observation, int delayMs)
    {
        QCoreApplication *application = QCoreApplication::instance();
        if (!application || !dispatch->installed) {
            return;
        }
        const quint64 expectedEpoch = dispatch->epoch;
        const QWeakPointer<SelectionObserverDispatchState> weak(dispatch);
        QTimer::singleShot(
            delayMs,
            application,
            [weak, expectedEpoch, observation]() {
                const QSharedPointer<SelectionObserverDispatchState> state =
                    weak.toStrongRef();
                if (!state
                    || !state->installed
                    || state->epoch != expectedEpoch
                    || !state->callback) {
                    return;
                }
                const std::function<void(const SelectionObservation &)>
                    callback = state->callback;
                callback(observation);
            }
        );
    }

    void enqueueLifecycle(const SelectionObservation &observation)
    {
        QCoreApplication *application = QCoreApplication::instance();
        if (!application || !dispatch->installed) {
            return;
        }
        const QWeakPointer<SelectionObserverDispatchState> weak(dispatch);
        QTimer::singleShot(
            0,
            application,
            [weak, observation]() {
                const QSharedPointer<SelectionObserverDispatchState> state =
                    weak.toStrongRef();
                if (!state || !state->installed || !state->callback) {
                    return;
                }
                const std::function<void(const SelectionObservation &)>
                    callback = state->callback;
                callback(observation);
            }
        );
    }

    void invalidateQueuedEvents()
    {
        ++dispatch->epoch;
    }

    SelectionObserver *q = nullptr;
    QSharedPointer<SelectionObserverDispatchState> dispatch;
    SelectionObservationMatcher matcher;
    bool systemAvailable = true;
    SelectedTextNativeWindowHandle notificationWindow = nullptr;
    SelectedTextNativeWindowHandle foregroundWindow = nullptr;
    SelectionObserverNativeEventFilter *nativeFilter = nullptr;
#ifdef Q_OS_WIN
    HHOOK mouseHook = nullptr;
    HHOOK keyboardHook = nullptr;
    HWINEVENTHOOK foregroundHook = nullptr;
    bool sessionNotificationRegistered = false;
#endif
};

SelectionObserver::SelectionObserver()
    : m_impl(new Impl(this))
{
}

SelectionObserver::~SelectionObserver()
{
    uninstall();
    delete m_impl;
    m_impl = nullptr;
}

bool SelectionObserver::install(
    SelectedTextNativeWindowHandle notificationWindow,
    QString *error)
{
    if (error) {
        error->clear();
    }
    if (m_impl->dispatch->installed) {
        return true;
    }
#ifdef Q_OS_WIN
    HWND nativeWindow = static_cast<HWND>(notificationWindow);
    if (!nativeWindow || !IsWindow(nativeWindow)) {
        if (error) {
            *error = QStringLiteral("selection_observer.invalid_window");
        }
        return false;
    }
    if (g_selectionObserver && g_selectionObserver != this) {
        if (error) {
            *error = QStringLiteral("selection_observer.already_installed");
        }
        return false;
    }
    g_selectionObserver = this;
    m_impl->notificationWindow = notificationWindow;
    m_impl->mouseHook = SetWindowsHookExW(
        WH_MOUSE_LL,
        selectionMouseHook,
        GetModuleHandleW(nullptr),
        0
    );
    if (!m_impl->mouseHook) {
        if (error) {
            *error = QStringLiteral("selection_observer.mouse_hook_failed");
        }
        uninstall();
        return false;
    }
    m_impl->keyboardHook = SetWindowsHookExW(
        WH_KEYBOARD_LL,
        selectionKeyboardHook,
        GetModuleHandleW(nullptr),
        0
    );
    if (!m_impl->keyboardHook) {
        if (error) {
            *error = QStringLiteral("selection_observer.keyboard_hook_failed");
        }
        uninstall();
        return false;
    }
    m_impl->foregroundHook = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND,
        EVENT_SYSTEM_FOREGROUND,
        nullptr,
        selectionForegroundHook,
        0,
        0,
        WINEVENT_OUTOFCONTEXT
    );
    if (!m_impl->foregroundHook) {
        if (error) {
            *error = QStringLiteral("selection_observer.foreground_hook_failed");
        }
        uninstall();
        return false;
    }
    if (!WTSRegisterSessionNotification(
            nativeWindow,
            NOTIFY_FOR_THIS_SESSION)) {
        if (error) {
            *error = QStringLiteral("selection_observer.session_hook_failed");
        }
        uninstall();
        return false;
    }
    m_impl->sessionNotificationRegistered = true;
    m_impl->nativeFilter = new SelectionObserverNativeEventFilter(this);
    QCoreApplication::instance()->installNativeEventFilter(
        m_impl->nativeFilter
    );
    m_impl->foregroundWindow = normalizedRootWindow(reinterpret_cast<
        SelectedTextNativeWindowHandle>(GetForegroundWindow()));
#else
    Q_UNUSED(notificationWindow);
#endif
    m_impl->systemAvailable = true;
    m_impl->matcher.reset();
    m_impl->dispatch->installed = true;
    m_impl->invalidateQueuedEvents();
    return true;
}

void SelectionObserver::uninstall()
{
    m_impl->dispatch->installed = false;
    m_impl->invalidateQueuedEvents();
    m_impl->dispatch->callback =
        std::function<void(const SelectionObservation &)>();
    m_impl->matcher.reset();
    m_impl->systemAvailable = true;
#ifdef Q_OS_WIN
    if (m_impl->nativeFilter) {
        if (QCoreApplication::instance()) {
            QCoreApplication::instance()->removeNativeEventFilter(
                m_impl->nativeFilter
            );
        }
        delete m_impl->nativeFilter;
        m_impl->nativeFilter = nullptr;
    }
    if (m_impl->sessionNotificationRegistered) {
        WTSUnRegisterSessionNotification(
            static_cast<HWND>(m_impl->notificationWindow)
        );
        m_impl->sessionNotificationRegistered = false;
    }
    if (m_impl->foregroundHook) {
        UnhookWinEvent(m_impl->foregroundHook);
        m_impl->foregroundHook = nullptr;
    }
    if (m_impl->keyboardHook) {
        UnhookWindowsHookEx(m_impl->keyboardHook);
        m_impl->keyboardHook = nullptr;
    }
    if (m_impl->mouseHook) {
        UnhookWindowsHookEx(m_impl->mouseHook);
        m_impl->mouseHook = nullptr;
    }
    if (g_selectionObserver == this) {
        g_selectionObserver = nullptr;
    }
#endif
    m_impl->notificationWindow = nullptr;
    m_impl->foregroundWindow = nullptr;
}

bool SelectionObserver::isInstalled() const
{
    return m_impl->dispatch->installed;
}

void SelectionObserver::setAutomaticEnabled(bool enabled)
{
    m_impl->invalidateQueuedEvents();
    m_impl->matcher.setAutomaticEnabled(enabled);
}

void SelectionObserver::setKeyboardSelectionEnabled(bool enabled)
{
    m_impl->invalidateQueuedEvents();
    m_impl->matcher.setKeyboardSelectionEnabled(enabled);
}

void SelectionObserver::setPaused(bool paused)
{
    m_impl->invalidateQueuedEvents();
    m_impl->matcher.setPaused(paused);
}

void SelectionObserver::setSurfaceVisible(bool visible)
{
    m_impl->matcher.setSurfaceVisible(visible);
}

void SelectionObserver::setCallback(
    const std::function<void(const SelectionObservation &)> &callback)
{
    m_impl->dispatch->callback = callback;
}

void SelectionObserver::triggerFallbackShortcut()
{
    if (!m_impl->dispatch->installed
        || !m_impl->systemAvailable
        || !m_impl->matcher.fallbackShortcutReleased()) {
        return;
    }
    SelectionObservation observation;
    observation.reason = SelectionObservationReason::FallbackShortcut;
    observation.cursorPhysicalPosition = currentPhysicalCursorPosition();
#ifdef Q_OS_WIN
    observation.targetWindow = normalizedRootWindow(
        reinterpret_cast<SelectedTextNativeWindowHandle>(
            GetForegroundWindow()
        )
    );
#endif
    m_impl->enqueueLifecycle(observation);
}

void SelectionObserver::processNativeMouse(
    unsigned int message,
    const QPoint &point)
{
    if (!m_impl->dispatch->installed || !m_impl->systemAvailable) {
        return;
    }
    if (message == kMessageLeftButtonDown) {
        m_impl->matcher.mousePressed(point);
        return;
    }
    if (message != kMessageLeftButtonUp
        || m_impl->matcher.isPaused()) {
        return;
    }
    const SelectedTextNativeWindowHandle target = rootWindowAt(point);
    SelectionObservation outside;
    outside.reason = SelectionObservationReason::OutsidePointerRelease;
    outside.cursorPhysicalPosition = point;
    outside.targetWindow = target;
    m_impl->enqueue(outside, 0);

    if (m_impl->matcher.mouseReleased(point)) {
        SelectionObservation candidate = outside;
        candidate.reason = SelectionObservationReason::MouseSelection;
        m_impl->enqueue(candidate, kCandidateDelayMs);
    }
}

void SelectionObserver::processNativeKey(
    unsigned int message,
    unsigned int nativeKey)
{
    if (!m_impl->dispatch->installed
        || !m_impl->systemAvailable
        || !isRelevantKey(nativeKey)) {
        return;
    }
    if (message == kMessageKeyDown
        || message == kMessageSystemKeyDown) {
        m_impl->matcher.keyPressed(nativeKey);
        return;
    }
    if (message != kMessageKeyUp
        && message != kMessageSystemKeyUp) {
        return;
    }

    SelectionObservation observation;
    observation.cursorPhysicalPosition = currentPhysicalCursorPosition();
#ifdef Q_OS_WIN
    observation.targetWindow = normalizedRootWindow(
        reinterpret_cast<SelectedTextNativeWindowHandle>(
            GetForegroundWindow()
        )
    );
#endif
    if (nativeKey == kKeyEscape) {
        if (m_impl->matcher.escapeReleased()) {
            observation.reason = SelectionObservationReason::EscapePressed;
            m_impl->enqueue(observation, 0);
        }
        return;
    }
    if (m_impl->matcher.keyReleased(nativeKey)) {
        observation.reason = SelectionObservationReason::KeyboardSelection;
        m_impl->enqueue(observation, kCandidateDelayMs);
    }
}

void SelectionObserver::processForegroundWindowChanged(
    SelectedTextNativeWindowHandle window)
{
    if (!m_impl->dispatch->installed || !m_impl->systemAvailable) {
        return;
    }
    window = normalizedRootWindow(window);
    if (window == m_impl->foregroundWindow) {
        return;
    }
    m_impl->foregroundWindow = window;
    m_impl->matcher.reset();
    m_impl->invalidateQueuedEvents();
    SelectionObservation observation;
    observation.reason = SelectionObservationReason::ForegroundChanged;
    observation.cursorPhysicalPosition = currentPhysicalCursorPosition();
    observation.targetWindow = window;
    m_impl->enqueueLifecycle(observation);
}

void SelectionObserver::processSystemAvailabilityChanged(bool available)
{
    if (!m_impl->dispatch->installed
        || available == m_impl->systemAvailable) {
        return;
    }
    m_impl->systemAvailable = available;
    m_impl->matcher.reset();
    m_impl->invalidateQueuedEvents();
    SelectionObservation observation;
    observation.reason = available
        ? SelectionObservationReason::SystemAvailable
        : SelectionObservationReason::SystemUnavailable;
    observation.cursorPhysicalPosition = currentPhysicalCursorPosition();
    observation.targetWindow = m_impl->foregroundWindow;
    m_impl->enqueueLifecycle(observation);
}
